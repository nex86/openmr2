#include "fire_atlas_data.h"
// Presentation-only explosion events. Seeds never consume the simulation/audio RNG.
struct ExplosionEvent{double W[3],birth,scale;int node,alive,hazard;unsigned seed;};
struct ContactBurst{double W[3],up[3],velocity[3],birth;float intensity;unsigned seed;int alive,grinding;};
static ContactBurst g_contacts[128];static int g_contact_next;
struct EnginePuff{double W[3],up[3],birth;float damage;unsigned seed;int alive;};
static EnginePuff g_engine_puffs[8][64];static int g_engine_slots[8];static double g_engine_time=-1;
static int damage_stage(double percent){return percent>=100?4:percent>=90?3:percent>=85?2:percent>=70?1:0;}
static int modern_fx(const Fx &f){return !(g_tex_nearest&&!g_retro_effects)&&(f.scale>=2?g_damage_effects:g_modern_explosions);}
static ExplosionEvent g_explosions[48];static int g_explosion_next;static double g_effect_time=-1;
static void missile_effect_reset();
static void effects_reset(){missile_effect_reset();mem_zero(g_contacts,sizeof g_contacts);g_contact_next=0;mem_zero(g_engine_puffs,sizeof g_engine_puffs);for(int i=0;i<8;i++)g_engine_slots[i]=-10000;g_engine_time=-1;mem_zero(g_explosions,sizeof g_explosions);g_explosion_next=0;g_effect_time=-1;}
static unsigned effect_hash(unsigned x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
static float effect_random(unsigned &x){x=effect_hash(x+0x9e3779b9u);return (x&0xffffff)/16777215.f;}
static void contact_sparks(const double *W,const double *up,const double *velocity,float intensity,int grinding){
  if(!g_damage_effects||(g_tex_nearest&&!g_retro_effects))return;ContactBurst &e=g_contacts[g_contact_next++%128];e.alive=1;e.grinding=grinding;e.birth=RC.t;e.intensity=intensity;e.seed=effect_hash((unsigned)(int)(RC.t*1000)^(unsigned)(int)W[0]^((unsigned)(int)W[2]<<3));for(int k=0;k<3;k++){e.W[k]=W[k];e.up[k]=up[k];e.velocity[k]=velocity[k];}
}
static void effects_sync(){
  if((g_tex_nearest&&!g_retro_effects)||(!g_modern_explosions&&!g_damage_effects&&!g_missile_effects)){if(g_effect_time>=0)effects_reset();return;}
  for(int i=0;i<48;i++)if(g_explosions[i].alive&&!(g_explosions[i].scale>=2?g_damage_effects:g_modern_explosions))g_explosions[i].alive=0;
  if(g_effect_time>=0&&(RC.t<g_effect_time-0.001||RC.t>g_effect_time+1.5))effects_reset();
  g_effect_time=RC.t;
  for(int j=0;j<24;j++){const Fx &f=RC.fx[j];if(!f.alive||!modern_fx(f))continue;double birth=RC.t-f.t;int exists=0;
    for(int k=0;k<48;k++){ExplosionEvent &e=g_explosions[k];if(!e.alive)continue;double d=0;for(int q=0;q<3;q++)d+=d_abs(e.W[q]-f.W[q]);if(d<4&&d_abs(e.birth-birth)<0.06){exists=1;break;}}
    if(exists)continue;ExplosionEvent &e=g_explosions[g_explosion_next++%48];e.alive=1;e.birth=birth;e.scale=f.scale;e.hazard=f.model==OM_EXP_BIG&&(d_abs(f.scale-1.3)<.015||d_abs(f.scale-1.4)<.015);e.node=player().pp.i;
    for(int q=0;q<3;q++)e.W[q]=f.W[q];double nearest=1e30;
    for(int k=0;k<g_route_n;k++){const RoadPt &r=RP(g_route[k]);double d=0;for(int q=0;q<3;q++)d+=(r.p[q]-e.W[q])*(r.p[q]-e.W[q]);if(d<nearest){nearest=d;e.node=g_route[k];}}
    e.seed=effect_hash((unsigned)(int)(e.W[0]/4)^(unsigned)(int)e.W[1]^((unsigned)(int)e.W[2]<<7)^(unsigned)(int)(birth*1000));
    sys_log("[event] Modern explosion: textured shader fire, smoke, sparks and short-lived 3D debris.");
  }
}
struct EffectVertex{V3 p,n;float u,v,kind,alpha,seed,age;};
static float g_particle_strength=1;
static EffectVertex g_effect_vertices[65536];static int g_effect_vertex_count;static int g_effect_screen_width=640;
static GLuint g_effect_program,g_effect_vao,g_effect_vbo,g_fire_texture;
static const char *VS_EFFECT=
 "#version 330 core\nlayout(location=0)in vec3 aPos;layout(location=1)in vec3 aNormal;layout(location=2)in vec2 aUV;layout(location=3)in vec4 aParams;uniform mat4 uVP;out vec3 n;out vec2 uv;out vec4 p;void main(){n=aNormal;uv=aUV;p=aParams;gl_Position=uVP*vec4(aPos,1.0);}";
static const char *FS_EFFECT=
 "#version 330 core\nin vec3 n;in vec2 uv;in vec4 p;out vec4 o;uniform float uGain,uTime;uniform sampler2D uFireAtlas;\n"
 "float hash(vec2 q){return fract(sin(dot(q,vec2(127.1,311.7)))*43758.5453);}float noise(vec2 q){vec2 i=floor(q),f=fract(q);f=f*f*(3.0-2.0*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1)),f.x),f.y);}float fbm(vec2 q){float s=0.0,a=0.5;for(int k=0;k<4;k++){s+=noise(q)*a;q=q*2.03+vec2(3.2,1.7);a*=0.5;}return s;}\n"
 "void main(){if(p.x>10.5){float core=exp(-pow((uv.x-0.5)*7.0,2.0));float ends=pow(max(0.0,sin(uv.y*3.14159265)),0.4);vec3 color=mix(vec3(1.0,0.18,0.015),vec3(1.0,0.95,0.65),core*(1.0-p.w));o=vec4(color,core*ends*p.y);if(o.a<0.002)discard;return;}if(p.x>8.5){vec2 q=uv*2.0-1.0;float r=length(q);float turbulence=fbm(q*5.0+vec2(p.z*17.0,-uTime*3.0));if(p.x>9.5){float ring=exp(-pow((r-0.78)/0.09,2.0));o=vec4(1.0,0.60,0.23,ring*p.y);}else{float edge=1.0-smoothstep(0.55+turbulence*0.26,1.0,r);float hot=clamp((1.0-r)*1.3+0.45*turbulence-p.w*1.2,0.0,1.0);vec3 color=mix(vec3(0.65,0.055,0.005),vec3(1.0,0.45,0.045),hot);color=mix(color,vec3(1.0,0.95,0.72),pow(hot,4.0));o=vec4(color,edge*p.y);}if(o.a<0.002)discard;return;}if(p.x>6.5){float t=p.w;float turbulence=noise(vec2(uv.y*8.0-uTime*6.0,p.z*31.0));float x=abs(uv.x*2.0-1.0);if(p.x<7.5){float edge=1.0-smoothstep(0.30+turbulence*0.22,1.0,x);float tips=1.0-smoothstep(0.65,1.0,abs(uv.y*2.0-1.0));float cloud=fbm(vec2(uv.x*5.0+p.z*19.0,uv.y*3.0-t*1.7));o=vec4(mix(vec3(0.42,0.44,0.46),vec3(0.76,0.77,0.78),cloud)*uGain,edge*tips*(0.45+cloud*0.55)*p.y);}else{float taper=pow(max(0.0,1.0-uv.y),0.65);float edge=1.0-smoothstep(taper*0.25,taper+0.015,x+sin(uv.y*17.0-uTime*43.0+p.z*21.0)*0.08*uv.y);float core=exp(-x*8.0)*(1.0-uv.y);vec3 color=mix(vec3(1.0,0.22,0.025),vec3(1.0,0.88,0.48),core);o=vec4(color,edge*(1.0-uv.y)*(0.7+0.3*turbulence)*p.y);}if(o.a<0.002)discard;return;}if(p.x<0.5){float light=0.32+0.68*max(dot(normalize(n),normalize(vec3(0.45,0.85,0.35))),0.0);vec3 metal=mix(vec3(0.07,0.08,0.10),vec3(0.30,0.28,0.24),fract(p.z*3.7));o=vec4(metal*light*uGain,p.y);return;}vec2 q=uv*2.0-1.0;float r=length(q);float cloud=fbm(q*3.5+vec2(p.z*43.0,-p.w*0.8));float edge=1.0-smoothstep(0.45+cloud*0.23,1.0,r);if(p.x<1.5||(p.x>3.5&&p.x<5.5)){float volume=edge*(0.35+cloud*0.65);float light=0.65+0.35*(1.0-uv.y);o=vec4((p.x>4.5?vec3(0.028,0.030,0.032):p.x>3.5?vec3(0.70,0.72,0.74):vec3(0.12,0.125,0.135))*light*uGain,volume*p.y);}else if(p.x<2.5||p.x>5.5){vec2 flow=uv;float phase=p.w*9.0+uTime*3.0+p.z*4.0;flow.x+=(noise(vec2(uv.y*5.0-uTime*2.4,p.z*13.0))-0.5)*0.11*(0.3+uv.y);flow.y=1.0-clamp(uv.y+sin(uTime*5.0+p.z*7.0)*0.018*uv.y,0.003,0.997);float frame=floor(phase);vec2 tile=vec2(mod(frame,2.0),mod(floor(frame/2.0),2.0));vec2 next=vec2(mod(frame+1.0,2.0),mod(floor((frame+1.0)/2.0),2.0));flow=clamp(flow,vec2(0.004),vec2(0.996));vec4 f=mix(texture(uFireAtlas,(flow+tile)*0.5),texture(uFireAtlas,(flow+next)*0.5),smoothstep(0.0,1.0,fract(phase)));float flicker=0.88+0.12*sin(uTime*17.0+p.z*21.0+uv.y*4.0);o=vec4(f.rgb*flicker,f.a*p.y); }else{o=vec4(1.0,0.48,0.08,edge*p.y);}if(o.a<0.002)discard;}";
static void effect_vertex(V3 p,V3 n,float u,float v,int kind,float alpha,float seed,float age){
  if(g_effect_vertex_count<65536)g_effect_vertices[g_effect_vertex_count++]={p,n,u,v,(float)kind,alpha*g_particle_strength,seed,age};
}
static void effect_quad(const double *cam,V3 center,float size,float stretch,int kind,float alpha,float seed,float age){
  const int ids[6]={0,1,2,1,3,2};
  for(int j=0;j<6;j++){int i=ids[j];effect_vertex(center+v3((i&1?1:-1)*size,(i&2?1:-1)*size*stretch,0),v3(0,0,1),(float)(i&1),(float)((i>>1)&1),kind,alpha,seed,age);}
}
#include "g_missile_effects.inc"
static void engine_effects_sync(const double *cam){
  if(!g_damage_effects||(g_tex_nearest&&!g_retro_effects)){if(g_engine_time>=0){mem_zero(g_engine_puffs,sizeof g_engine_puffs);for(int i=0;i<8;i++)g_engine_slots[i]=-10000;g_engine_time=-1;}return;}
  if(g_engine_time>=0&&RC.t<g_engine_time){mem_zero(g_engine_puffs,sizeof g_engine_puffs);for(int i=0;i<8;i++)g_engine_slots[i]=-10000;}
  g_engine_time=RC.t;int slot=(int)(RC.t*20);
  for(int i=0;i<g_scene_count;i++){const SceneModel &s=g_scene[i];if(s.car<0||s.car>=8||damage_stage(RC.car[s.car].damage)==0||g_engine_slots[s.car]==slot)continue;
    g_engine_slots[s.car]=slot;EnginePuff &p=g_engine_puffs[s.car][slot%64];p.alive=1;p.birth=RC.t;p.damage=(float)RC.car[s.car].damage;p.seed=effect_hash((unsigned)(s.car*9137+slot*12347));
    V3 extent=s.model->bmax-s.model->bmin,anchor=v3((s.model->bmin.x+s.model->bmax.x)*.5f,s.model->bmin.y+extent.y*.78f,s.model->bmin.z+extent.z*.20f);
    if(p.damage>=80)anchor.y-=extent.y*(p.damage>=100?.22f:.025f);
    V3 eye=transform_point(s.mv,anchor);
    for(int k=0;k<3;k++){p.W[k]=cam[k]+(cam[k+3]*eye.x+cam[k+6]*eye.y-cam[k+9]*eye.z)/MODEL_SCALE-g_shift[k];p.up[k]=RC.car[s.car].U[k];}
  }
}
static void engine_effects_build(const double *cam,int pass){
  g_particle_strength=g_effect_strength[4];if(!g_damage_effects||(g_tex_nearest&&!g_retro_effects)||(pass!=1&&pass!=3))return;
  for(int car=0;car<8;car++)for(int i=0;i<64;i++){EnginePuff &p=g_engine_puffs[car][i];if(!p.alive)continue;float t=(float)(RC.t-p.birth),life=p.damage>=85?2.6f:2.1f;if(t<0||t>life){p.alive=0;continue;}if(pass==3&&(p.damage<90||t>.5f))continue;
    unsigned seed=p.seed;float a=effect_random(seed),b=effect_random(seed);double world[3];for(int k=0;k<3;k++)world[k]=p.W[k]+p.up[k]*(t*2100+t*t*900)+(k==0?(a-.5)*t*1100:k==2?(b-.5)*t*1100:0);
    V3 center=shadow_eye(cam,world);if(center.z>-2)continue;float fade=m_clamp((life-t)/.8f,0,1);
    if(pass==1)effect_quad(cam,center,(280+t*1100)*(0.8f+a*.4f)*MODEL_SCALE,1,p.damage>=85?5:4,fade*(p.damage>=85?.42f:.25f),a,t);
    else {float growth=m_clamp((p.damage-90)/10.f,0,1);int clusters=p.damage>=100?3:1;float intensity=.30f+.55f*growth;for(int flame=0;flame<clusters;flame++){float radius=(300.f+450.f*growth)*(1+t*.25f)*MODEL_SCALE;V3 q=center+shadow_axis(cam,p.up)*(radius*2.3f)+v3((flame-(clusters-1)*.5f)*.75f,0,0);effect_quad(cam,q,radius,2.3f,6,(1-t/.5f)*intensity,a+flame*.23f,t);}}
  }
}
static void contact_effects_build(const double *cam,int pass){
  g_particle_strength=g_effect_strength[4];if(!g_damage_effects||(g_tex_nearest&&!g_retro_effects)){for(int i=0;i<128;i++)g_contacts[i].alive=0;return;}if(pass!=2)return;
  for(int i=0;i<128;i++){ContactBurst &e=g_contacts[i];if(!e.alive)continue;float t=(float)(RC.t-e.birth);if(t<0||t>.58f){e.alive=0;continue;}unsigned seed=e.seed;int count=e.grinding?24+(int)(e.intensity*40):14+(int)(e.intensity*24);
    for(int j=0;j<count;j++){float a=effect_random(seed),b=effect_random(seed),life=e.grinding?.22f+a*.34f:.10f+a*.23f;if(t>life)continue;double p[3],v[3];for(int k=0;k<3;k++){v[k]=e.velocity[k]*(.40+a*.9)+e.up[k]*(1700+6500*b)+(k==0?(a-.5)*8000:k==2?(b-.5)*8000:0);p[k]=e.W[k]+v[k]*t-e.up[k]*12500*t*t;}
      V3 center=shadow_eye(cam,p),motion=shadow_axis(cam,v);if(center.z>-2)continue;V3 axis=norm(v3(motion.x,motion.y,0)),across=v3(-axis.y,axis.x,0);float width=m_max((35+50*a)*MODEL_SCALE,-center.z*1.0f/m_max(.1f,g_proj.m[0]*g_effect_screen_width*.5f)),length=(180+(e.grinding?1700:900)*e.intensity)*(1-t/life*.6f)*MODEL_SCALE;
      const int ids[6]={0,1,2,1,3,2};for(int k=0;k<6;k++){int id=ids[k];V3 vertex=center+across*((id&1?1:-1)*width)+axis*((id&2?1:-1)*length);effect_vertex(vertex,v3(0,0,1),(float)(id&1),(float)((id>>1)&1),11,(1-t/life)*1.15f,a,t/life);}
    }
  }
}
static void effects_build(const double *cam,int pass){
  g_particle_strength=g_effect_strength[3];g_effect_vertex_count=0;int order[48],count=0;float depths[48];
  for(int i=0;i<48;i++){ExplosionEvent &e=g_explosions[i];double age=RC.t-e.birth;if(!e.alive||age<0)continue;if(age>3.6){e.alive=0;continue;}order[count++]=i;depths[i]=-shadow_eye(cam,e.W).z;}
  for(int i=1;i<count;i++){int idx=order[i],j=i-1;while(j>=0&&depths[order[j]]<depths[idx]){order[j+1]=order[j];j--;}order[j+1]=idx;}
  for(int ix=0;ix<count;ix++){
    ExplosionEvent &e=g_explosions[order[ix]];g_particle_strength=g_effect_strength[e.scale>=2?4:3];float t=(float)(RC.t-e.birth),scale=(float)e.scale;const RoadPt &road=RP(e.node);if(e.hazard)scale*=2.6f;
    V3 rt=v3((float)road.rt[0],(float)road.rt[1],(float)road.rt[2]),up=v3((float)road.up[0],(float)road.up[1],(float)road.up[2]),fw=v3((float)road.fw[0],(float)road.fw[1],(float)road.fw[2]);
    unsigned seed=e.seed;
    if(e.hazard&&pass==4&&t<.55f){
      float radius=(4500+25000*m_min(t,.28f))*MODEL_SCALE;V3 core=shadow_eye(cam,e.W)+shadow_axis(cam,road.up)*(radius*.25f);
      effect_quad(cam,core,radius,1,9,m_clamp((.55f-t)/.20f,0,1),.37f,t);
      if(t<.30f){float ring=(3500+65000*t)*MODEL_SCALE;V3 center=shadow_eye(cam,e.W)+shadow_axis(cam,road.up)*(-1.9f);const int ids[6]={0,1,2,1,3,2};V3 R0=shadow_axis(cam,road.rt),F0=shadow_axis(cam,road.fw);for(int k=0;k<6;k++){int id=ids[k];effect_vertex(center+R0*((id&1?1:-1)*ring)+F0*((id&2?1:-1)*ring),v3(0,1,0),(float)(id&1),(float)((id>>1)&1),10,(1-t/.3f)*.7f,.3f,t);}}
    }
    for(int j=0;pass<4&&j<(e.hazard?224:e.scale>=2?112:56);j++){
      int part=j%56,kind=part<12?0:part<30?1:part<38?2:3;float a=effect_random(seed),b=effect_random(seed),c=effect_random(seed),d=effect_random(seed);
      if((pass==0&&kind!=0)||(pass==1&&kind!=1)||(pass==2&&kind!=3)||(pass==3&&kind!=2))continue;
      float life=kind==0?1.1f+a*0.45f:kind==1?2.5f+a*1.1f:kind==2?0.55f+a*0.25f:0.55f+a*0.4f;if(t>=life)continue;
      float angle=b*2*PI,cs=m_cos(angle),sn=m_sin(angle),radial=kind==1?900.f:kind==2?1600.f:kind==0?4000.f+9000*c:6000.f+12000*c;
      if(e.hazard&&kind!=1)radial*=2.4f;V3 pos=rt*(cs*radial*t)+fw*(sn*radial*t);
      float height=kind==1?800*t+1100*t*t:kind==2?800*t:(6000+10000*d)*t-10000*t*t;
      double ground_height=0;for(int k=0;k<3;k++)ground_height+=(e.W[k]-road.p[k])*road.up[k];
      if(kind==0&&height+ground_height<180){height=(float)(180-ground_height);pos=pos*0.72f;}
      pos=pos+up*height;double world[3]={e.W[0]+pos.x,e.W[1]+pos.y,e.W[2]+pos.z};V3 center=shadow_eye(cam,world);if(center.z>-2)continue;
      float fade=m_clamp((life-t)/(kind==0?0.35f:kind==1?1.1f:0.3f),0,1);
      if(kind==0){
        float turn=t*(5+8*c)+angle,ct=m_cos(turn),st=m_sin(turn);V3 R0=rt*ct+fw*st,F0=fw*ct-rt*st;float roll=t*(7+6*d);V3 U0=up*m_cos(roll)+R0*m_sin(roll);R0=R0*m_cos(roll)-up*m_sin(roll);
        float chunk=e.hazard?1.8f:1.f;V3 axes[3]={R0*((120+280*a)*chunk),U0*((90+190*b)*chunk),F0*((180+350*c)*chunk)};V3 points[6];
        for(int k=0;k<3;k++)for(int side=0;side<2;side++){V3 q=axes[k]*(side?1.f:-1.f);double v[3]={q.x,q.y,q.z};points[k*2+side]=shadow_axis(cam,v)*MODEL_SCALE+center;}
        for(int k=0;k<8;k++){V3 v0=points[k&1],v1=points[2+((k>>1)&1)],v2=points[4+((k>>2)&1)];V3 normal=norm(cross(v1-v0,v2-v0));if((k&1)^((k>>1)&1)^((k>>2)&1))normal=normal*-1;
          effect_vertex(v0,normal,0,0,0,fade,a,t);effect_vertex(v1,normal,0,0,0,fade,a,t);effect_vertex(v2,normal,0,0,0,fade,a,t);}
      }else{
        float radius=kind==1?(700+1700*t)*(0.7f+c)*scale:kind==2?(1000+2200*t)*(0.7f+c)*scale:55+50*a;
        float alpha=kind==1?fade*m_clamp(t*7,0,1)*0.32f:fade;
        effect_quad(cam,center,radius*MODEL_SCALE,kind==3?2.4f:kind==2?(e.hazard?.72f:1.45f):1,kind,alpha,a,t);
      }
    }
  }
  engine_effects_build(cam,pass);contact_effects_build(cam,pass);missile_effects_build(cam,pass);
}
static void effects_draw(const double *cam){
  effects_sync();engine_effects_sync(cam);int missile_active=missile_effects_active();if((!g_modern_explosions&&!g_damage_effects&&!g_missile_effects)||(g_tex_nearest&&!g_retro_effects))return;
  int any=missile_active;for(int i=0;i<48;i++)if(g_explosions[i].alive)any=1;for(int car=0;car<8&&!any;car++)for(int i=0;i<64;i++)if(g_engine_puffs[car][i].alive)any=1;for(int i=0;i<128&&!any;i++)if(g_contacts[i].alive&&g_damage_effects)any=1;if(!any)return;
  if(!g_effect_program){g_effect_program=shader_build(VS_EFFECT,FS_EFFECT);if(!g_effect_program)return;
    glGenVertexArrays(1,&g_effect_vao);glGenBuffers(1,&g_effect_vbo);glBindVertexArray(g_effect_vao);glBindBuffer(GL_ARRAY_BUFFER,g_effect_vbo);
    const int sizes[4]={3,3,2,4},offsets[4]={0,3,6,8};for(int i=0;i<4;i++){glEnableVertexAttribArray(i);glVertexAttribPointer(i,sizes[i],GL_FLOAT,GL_FALSE,sizeof(EffectVertex),(void *)(uintptr_t)(offsets[i]*sizeof(float)));}
  }
  if(!g_fire_texture){int w,h,ch;u8 *rgba=stbi_load_from_memory(FIRE_ATLAS_PNG,sizeof FIRE_ATLAS_PNG,&w,&h,&ch,4);if(rgba){g_fire_texture=tex_upload(rgba,w,h,0,1);sys_free(rgba);sys_log("[event] Vehicle fire atlas loaded: animated RGBA flames with flow shader.");}}
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,g_fire_texture);
  GLint viewport[4]={0,0,640,480};glGetIntegerv(0x0BA2,viewport);g_effect_screen_width=viewport[2]>0?viewport[2]:640;
  glUseProgram(g_effect_program);glUniform1i(glGetUniformLocation(g_effect_program,"uFireAtlas"),0);glUniform1f(glGetUniformLocation(g_effect_program,"uTime"),(float)RC.t);glUniformMatrix4fv(glGetUniformLocation(g_effect_program,"uVP"),1,0,g_proj.m);glUniform1f(glGetUniformLocation(g_effect_program,"uGain"),RC.gain);
  glBindVertexArray(g_effect_vao);glBindBuffer(GL_ARRAY_BUFFER,g_effect_vbo);glEnable(GL_DEPTH_TEST);glEnable(GL_BLEND);
  for(int pass=0;pass<5;pass++){effects_build(cam,pass);glDepthMask(pass==0?GL_TRUE:GL_FALSE);glBlendFunc(GL_SRC_ALPHA,(pass==2||pass==4)?GL_ONE:GL_ONE_MINUS_SRC_ALPHA);
    glBufferData(GL_ARRAY_BUFFER,g_effect_vertex_count*sizeof(EffectVertex),g_effect_vertices,GL_DYNAMIC_DRAW);glDrawArrays(GL_TRIANGLES,0,g_effect_vertex_count);
  }
  glDepthMask(GL_TRUE);glDisable(GL_BLEND);
}
