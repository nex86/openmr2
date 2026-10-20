// Presentation-only quality checks against the real core, without game assets
// or a GL context. Runtime shader compilation is checked by graphics captures.
#include "platform_stdio.inc"
#include "../src/core.cpp"
#define EFFECT_STUB(ret,name,args) static ret effect_##name args{return (ret)0;}
GL_FUNCS(EFFECT_STUB)
#undef EFFECT_STUB
static int checks,failures,draw_calls;
static void check(int ok,const char *message){checks++;if(!ok){failures++;printf("FAIL: %s\n",message);}}
static void count_draw(GLenum,GLint,GLsizei){draw_calls++;}
static RoadPt road[2];static Model car_model;
static const double camera[12]={0,0,-40000,1,0,0,0,1,0,0,0,1};
static void reset_world(int quality){
  mem_zero(&RC,sizeof RC);mem_zero(&TK,sizeof TK);mem_zero(road,sizeof road);mem_zero(&car_model,sizeof car_model);
  TK.road=road;TK.nroad=TK.L=2;road[0].up[1]=road[1].up[1]=1;road[0].rt[0]=road[1].rt[0]=1;road[0].fw[2]=road[1].fw[2]=1;road[0].w=road[1].w=40000;
  car_model.bmin=v3(-3,0,-5);car_model.bmax=v3(3,4,5);g_scene_count=1;g_scene[0]={&car_model,m4_id(),0,0,0};g_scene[0].mv.m[14]=-40;
  RC.ncar=1;RC.car[0].player=1;RC.car[0].U[1]=1;RC.car[0].damage=99;RC.gain=1;RC.t=1;g_effect_render_time=1;
  g_tex_nearest=0;g_damage_effects=g_modern_explosions=g_missile_effects=g_vehicle_lights=1;g_effect_quality=quality;for(int i=0;i<7;i++)g_effect_strength[i]=1;
  g_scene_count=1;g_proj=m4_persp(1.0f,4.f/3,.5f,4000);g_route_n=1;g_route[0]=0;mem_zero(g_shift,sizeof g_shift);effects_reset();lamp_history_reset();
}
static int explosion_vertices(int quality,int pass){
  reset_world(quality);ExplosionEvent &e=g_explosions[0];e.alive=1;e.node=0;e.birth=.9;e.scale=1;e.seed=123;e.W[2]=10000;
  effects_build(camera,pass);return g_effect_vertex_count;
}
int main(){
  g_heap=GetProcessHeap();
#define EFFECT_GL(ret,name,args) name=effect_##name;
  GL_FUNCS(EFFECT_GL)
#undef EFFECT_GL
  glDrawArrays=count_draw;
  check(explosion_vertices(1,0)==12*24,"Medium keeps the original twelve debris chunks");
  for(int pass=0;pass<4;pass++){int low=explosion_vertices(0,pass),medium=explosion_vertices(1,pass),high=explosion_vertices(2,pass),extreme=explosion_vertices(3,pass);check(low<medium&&medium<=high&&high<=extreme,"quality levels scale each explosion category without changing Medium");}
  reset_world(1);engine_effects_sync(camera);int puffs=0;for(int i=0;i<256;i++)puffs+=g_engine_puffs[0][i].alive;
  check(puffs==1,"Medium keeps one original 20 Hz emitter sample");engine_effects_sync(camera);int again=0;for(int i=0;i<256;i++)again+=g_engine_puffs[0][i].alive;check(again==1,"duplicate renders do not multiply Medium particles");
  RC.t=1.05;engine_effects_sync(camera);g_effect_vertex_count=0;engine_effects_build(camera,3);check(g_effect_vertex_count==12,"Medium keeps one original fire sprite per emitted sample below total destruction");
  for(int quality=2;quality<=3;quality++){
    reset_world(quality);RC.car[0].damage=95;unsigned rng=g_rng,fx_rng=g_rng_fx;engine_effects_sync(camera);
    RC.t=1.12;g_effect_render_time=1.10;g_scene[0].mv.m[14]=-60;engine_effects_sync(camera);
    int samples=0;double previous=-1e30,min=1e30,max=-1e30;
    for(int i=0;i<256;i++)if(g_engine_puffs[0][i].alive){EnginePuff &p=g_engine_puffs[0][i];samples++;check(p.birth>=1&&p.birth<=1.100001,"swept engine samples use presentation time, not the newer simulation time");check(p.W[2]>=previous,"high-speed emissions interpolate monotonically along the travelled path");previous=p.W[2];min=d_clamp(p.W[2],-1e30,min);if(p.W[2]>max)max=p.W[2];}
    check(samples>7&&samples<=14&&max-min>15000,"fixed-time sampling fills high-speed emitter paths with bounded samples");
    g_effect_vertex_count=0;engine_effects_build(camera,3);int fire=0;for(int i=0;i<g_effect_vertex_count;i++)fire+=g_effect_vertices[i].kind==12;check(fire>12&&fire%6==0,"High and Extreme generate connected textured fire strips instead of detached fire sprites");
    float highest=-1e9f;for(int i=0;i<g_effect_vertex_count;i++)highest=m_max(highest,g_effect_vertices[i].p.y);
    V3 engine=shadow_eye(camera,g_engine_emitters[0].W);check(highest-engine.y>3.5f,"attached fire reaches above the body instead of collapsing under a double-tapered atlas");
    A.no_engine_fire=1;g_effect_vertex_count=0;engine_effects_build(camera,3);check(!g_effect_vertex_count,"developer fire isolation suppresses only engine flame geometry");A.no_engine_fire=0;
    g_effect_vertex_count=0;engine_effects_build(camera,1);int smoke=0;for(int i=0;i<g_effect_vertex_count;i++)smoke+=g_effect_vertices[i].kind==13;check(smoke>0,"smoke connects successive world-space emitter samples");
    check(g_rng==rng&&g_rng_fx==fx_rng,"quality and swept effects never consume gameplay or audio RNG");
    int serial=g_engine_emitters[0].serial;engine_effects_sync(camera);check(g_engine_emitters[0].serial==serial,"additional renders at one presentation time do not multiply high-quality particles");
    RC.car[0].pose_gen++;g_scene[0].mv.m[14]=-400;g_effect_render_time=1.11;engine_effects_sync(camera);check(g_engine_emitters[0].serial==1,"recovery teleports start a fresh emitter without bridging the jump");
    g_effect_render_time=.5;engine_effects_sync(camera);check(g_engine_emitters[0].serial==1,"replay seeks clear future emitter history");
  }
  for(int quality=0;quality<4;quality++){
    reset_world(quality);Proj &p=RC.proj[0];p.alive=1;p.kind=W_KARD;p.gen=5;p.life=3.8;
    M4 pose=m4_id();pose.m[14]=-40;missile_effect_pose(camera,0,p,car_model,pose);RC.t=1.1;g_effect_render_time=1.1;pose.m[14]-=20;missile_effect_pose(camera,0,p,car_model,pose);
    int expected=quality==0?7:quality==1?13:quality==2?19:25;check(g_missile_smoke_next==expected,"missile quality selects bounded 60/120/180/240 Hz emission");
    g_effect_vertex_count=0;missile_effects_build(camera,1);check(g_effect_vertex_count>0&&g_effect_vertex_count<=4096*6,"every missile quality produces bounded smoke geometry");
    if(quality>=2){int connected=0;for(int i=0;i<g_effect_vertex_count;i++)connected+=g_effect_vertices[i].kind==13;check(connected>0,"high-quality missile smoke uses continuous swept strips");}
    g_effect_vertex_count=0;missile_effects_build(camera,2);check(g_effect_vertex_count==(quality>=3?18:quality==2?12:6),"missile fire detail scales while Medium remains one original flame");
  }
  for(int quality=2;quality<=3;quality++){
    reset_world(quality);LM.w=1280;LM.h=960;GLint viewport[4]={0,90,1280,700};
    for(int i=0;i<20;i++){RC.t=1+i*.01;g_effect_render_time=RC.t;g_scene[0].mv.m[12]=i*.13f;g_scene[0].mv.m[14]=-40-i*.25f;lamp_pose_record(camera,g_scene[0]);}
    float path[128]={},rect[4];int samples=0,count=lamp_motion_path(camera,g_scene[0],viewport,path,rect,samples);
    check(count==(quality==2?16:32)&&samples>0&&samples<=(quality==2?128:192),"continuous lamp blur uses bounded quality-specific path and integration samples");
    for(int i=0;i<count;i++)check(path[i*4+2]>=.5f&&path[i*4+2]<=2&&path[i*4+3]>=0&&path[i*4+3]<=1,"lamp history projection keeps scale and fade finite");
    check(rect[0]>=-1&&rect[1]>=-1&&rect[2]<=1&&rect[3]<=1&&rect[2]>rect[0]&&rect[3]>rect[1],"lamp blur is restricted to a valid projected car/trail rectangle");
    g_effect_render_time=.2;lamp_pose_record(camera,g_scene[0]);check(!g_lamp_history[0][1].valid,"replay seeks clear future lamp history");
  }
  reset_world(3);RC.t=1;g_effect_render_time=1;double W[3]={0,0,0},up[3]={0,1,0},v[3]={2000,0,-20000};
  for(int i=0;i<200;i++)contact_sparks(W,up,v,1,1);check(g_contact_next==200,"contact event ring remains bounded under repeated impacts");
  for(int i=0;i<48;i++){g_explosions[i].alive=1;g_explosions[i].birth=.9;g_explosions[i].scale=2.4;g_explosions[i].node=0;g_explosions[i].W[2]=10000;g_explosions[i].seed=i;}
  effects_build(camera,0);check(g_effect_vertex_count<=65536,"Extreme stress respects the fixed effect vertex budget");
  g_effect_time=RC.t;g_tex_nearest=1;int before=draw_calls;effects_draw(camera);vehicle_lights_draw(camera,1280,960);check(draw_calls==before,"Retro performs no modern effect or lamp draws at any quality");
  int events=0;for(int i=0;i<48;i++)events+=g_explosions[i].alive;check(!events,"Retro clears modern particle histories");
  printf("Effect quality: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
