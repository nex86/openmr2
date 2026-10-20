// Exhaust follows the interpolated model, while smoke stays at past world positions.
// Fixed time samples and independent seeds keep emission independent of rendering rate/RNG.
struct MissileEmitter {double W[3],F[3],U[3],time,next,birth;int gen,valid,kind,last;};
struct MissileSmoke {double W[3],F[3],U[3],birth;float half_length,boost;unsigned seed;int alive,previous,gen,emitter;};
static MissileEmitter g_missile_emitters[32];
static MissileSmoke g_missile_smoke[4096];
static int g_missile_smoke_next;static double g_missile_time=-1;
static int g_missile_quality=-1;
static void missile_effect_reset(){mem_zero(g_missile_emitters,sizeof g_missile_emitters);mem_zero(g_missile_smoke,sizeof g_missile_smoke);g_missile_smoke_next=0;g_missile_time=-1;}
static void missile_effect_pose(const double *cam,int index,const Proj &p,const Model &model,const M4 &mv){
  if(!g_missile_effects||(g_tex_nearest)||index<0||index>=32)return;
  if(g_missile_quality!=g_effect_quality){missile_effect_reset();g_missile_quality=g_effect_quality;}
  double now=effect_clock(),rate=g_effect_quality==0?60:g_effect_quality==2?180:g_effect_quality>=3?240:120;
  if(g_missile_time>=0&&(now<g_missile_time-.001||now>g_missile_time+1.5))missile_effect_reset();g_missile_time=now;
  V3 nozzle=v3((model.bmin.x+model.bmax.x)*.5f,model.bmax.y-(model.bmax.x-model.bmin.x)*.18f,model.bmin.z);
  V3 eye=transform_point(mv,nozzle);double W[3],F[3],U[3];
  for(int k=0;k<3;k++){
    W[k]=cam[k]+(cam[k+3]*eye.x+cam[k+6]*eye.y-cam[k+9]*eye.z)/MODEL_SCALE-g_shift[k];
    F[k]=cam[k+3]*mv.m[8]+cam[k+6]*mv.m[9]-cam[k+9]*mv.m[10];
    U[k]=cam[k+3]*mv.m[4]+cam[k+6]*mv.m[5]-cam[k+9]*mv.m[6];
  }
  MissileEmitter &e=g_missile_emitters[index];
  double distance=0;for(int k=0;k<3;k++)distance+=(W[k]-e.W[k])*(W[k]-e.W[k]);
  int fresh=!e.valid||e.gen!=p.gen||now-e.time>.25||distance>200000.0*200000.0;
  if(fresh){e.valid=1;e.gen=p.gen;e.time=now;e.next=now;e.birth=now-d_clamp(4-p.life,0,4);e.kind=p.kind;e.last=-1;for(int k=0;k<3;k++)e.W[k]=W[k];
    if(sys_debug_enabled())sys_log("[event] Missile exhaust: launch pulse, animated jet flame and world-space smoke trail.");}
  double dt=now-e.time,speed=dt>.00001?d_sqrt(distance)/dt:0;
  for(int count=0;e.next<=now+.000001&&count<(g_effect_quality>=2?64:32);count++,e.next+=1.0/rate){
    int slot=g_missile_smoke_next++%4096;MissileSmoke &s=g_missile_smoke[slot];s.alive=1;s.birth=e.next;s.previous=e.last;s.gen=p.gen;s.emitter=index;e.last=slot;
    double a=dt>.00001?d_clamp((e.next-e.time)/dt,0,1):1;
    for(int k=0;k<3;k++){s.W[k]=e.W[k]+(W[k]-e.W[k])*a;s.F[k]=F[k];s.U[k]=U[k];}
    s.half_length=(float)d_clamp(speed/rate*.62,220,6500);s.boost=e.next-e.birth<.12?1.f:0.f;
    s.seed=effect_hash((unsigned)p.gen*19391u^(unsigned)(int)(e.next*rate));
  }
  if(e.next<now)e.next=now+1.0/rate;
  e.time=now;for(int k=0;k<3;k++){e.W[k]=W[k];e.F[k]=F[k];e.U[k]=U[k];}
}
static void missile_ribbon(V3 start,V3 end,float width,int kind,float alpha,float seed,float age){
  V3 axis=end-start;V3 side=norm(cross(axis,(start+end)*.5f));if(dot(side,side)<.1f)side=v3(1,0,0);
  const int ids[6]={0,1,2,1,3,2};for(int k=0;k<6;k++){int i=ids[k];effect_vertex((i&2?end:start)+side*((i&1?1:-1)*width),v3(0,0,1),(float)(i&1),(float)((i>>1)&1),kind,alpha,seed,age);}
}
static void missile_effects_build(const double *cam,int pass){
  g_particle_strength=g_effect_strength[5];if(!g_missile_effects||(g_tex_nearest))return;
  if(pass==1)for(int i=0;i<4096;i++){
    MissileSmoke &s=g_missile_smoke[i];if(!s.alive)continue;float t=(float)(effect_clock()-s.birth);
    if(t<0||t>.65f){s.alive=0;continue;}
    unsigned seed=s.seed;float a=effect_random(seed),b=effect_random(seed);double W[3];
    for(int k=0;k<3;k++)W[k]=s.W[k]+s.U[k]*(t*750+t*t*1300)+(k==0?(a-.5)*t*650:k==2?(b-.5)*t*650:0);
    V3 center=shadow_eye(cam,W);if(center.z>-2)continue;V3 along=shadow_axis(cam,s.F)*(s.half_length*MODEL_SCALE);
    float width=(230+1100*t)*(1+s.boost*.35f)*MODEL_SCALE;
    if(g_effect_quality>=2){
      if(s.previous<0)continue;const MissileSmoke &previous=g_missile_smoke[s.previous];
      if(!previous.alive||previous.gen!=s.gen||previous.emitter!=s.emitter||s.birth<=previous.birth||s.birth-previous.birth>.03)continue;
      float old_t=(float)(effect_clock()-previous.birth);unsigned old_seed=previous.seed;float old_a=effect_random(old_seed),old_b=effect_random(old_seed);double old_W[3];
      for(int k=0;k<3;k++)old_W[k]=previous.W[k]+previous.U[k]*(old_t*750+old_t*old_t*1300)+(k==0?(old_a-.5)*old_t*650:k==2?(old_b-.5)*old_t*650:0);
      V3 tail=shadow_eye(cam,old_W);if(tail.z>-2)continue;effect_strip(center,tail,width,(230+1100*old_t)*(1+previous.boost*.35f)*MODEL_SCALE,t/.65f,old_t/.65f,13,(.35f+s.boost*.15f)*m_clamp((.65f-t)/.25f,0,1),.2f,t);continue;
    }
    missile_ribbon(center-along,center+along,width,7,(.45f+s.boost*.18f)*m_clamp((.65f-t)/.25f,0,1),a,t);
  }
  if(pass==2&&A.graphics_test!=11)for(int i=0;i<32;i++){
    const MissileEmitter &e=g_missile_emitters[i];const Proj &p=RC.proj[i];
    if(!e.valid||!p.alive||p.gen!=e.gen||d_abs(effect_clock()-e.time)>.001)continue;
    V3 start=shadow_eye(cam,e.W);if(start.z>-2)continue;float age=(float)(effect_clock()-e.birth),pulse=m_clamp(1-age/.12f,0,1);
    float size=e.kind==W_HITRAX?1.25f:e.kind==W_BOOMER?1.10f:1.f;
    float flicker=.9f+.1f*m_sin((float)effect_clock()*83+e.gen*1.7f);
    V3 end=start-shadow_axis(cam,e.F)*((1500+1900*pulse)*size*flicker*MODEL_SCALE);
    if(g_effect_quality>=2){int layers=g_effect_quality>=3?3:2;float width=(210+180*pulse)*size*MODEL_SCALE;for(int layer=0;layer<layers;layer++){V3 offset=shadow_axis(cam,e.U)*(width*layer*.25f);effect_strip(start+offset,end+offset,width,width*.02f,0,1,12,.95f/layers,(float)(e.gen%127)/127.f+layer*.23f,age);}}
    else missile_ribbon(start,end,(210+180*pulse)*size*MODEL_SCALE,8,.9f,(float)(e.gen%127)/127.f,age);
  }
}
static int missile_effects_active(){
  if(!g_missile_effects||(g_tex_nearest)){if(g_missile_time>=0)missile_effect_reset();return 0;}
  for(int i=0;i<32;i++)if(g_missile_emitters[i].valid&&RC.proj[i].alive&&RC.proj[i].gen==g_missile_emitters[i].gen&&d_abs(effect_clock()-g_missile_emitters[i].time)<.001)return 1;
  for(int i=0;i<4096;i++)if(g_missile_smoke[i].alive){if(effect_clock()-g_missile_smoke[i].birth>.65||effect_clock()<g_missile_smoke[i].birth)g_missile_smoke[i].alive=0;else return 1;}return 0;
}
