// Exercise the real drive-to-render bridge, original model wheel transforms,
// in-between pictures and replay samples with the caller's private game data.
#include "platform_stdio.inc"
#include "../src/core.cpp"
#define WHEEL_STUB(ret,name,args) static ret wheel_##name args{return (ret)0;}
GL_FUNCS(WHEEL_STUB)
#undef WHEEL_STUB
static int failures,checks,matrix_count;
static M4 matrices[32];
static void check(int ok,const char *message){checks++;if(!ok){failures++;printf("FAIL: %s\n",message);}}
static void capture_matrix(GLint location,GLsizei,GLboolean,const GLfloat *p){
  if(location==R.um_m&&matrix_count<32)mem_cpy(matrices[matrix_count++].m,p,sizeof(M4));
}
static int near(double a,double b,double tolerance=1e-8){return d_abs(a-b)<tolerance;}
int main(int argc,char **argv){
  if(argc!=2){printf("usage: wheel_rotation GAME_DIRECTORY\n");return 2;}
  g_heap=GetProcessHeap();
#define WHEEL_GL(ret,name,args) name=wheel_##name;
  GL_FUNCS(WHEEL_GL)
#undef WHEEL_GL
  glUniformMatrix4fv=capture_matrix;R.um_m=271828;
  str_cpy(g_gamedir,argv[1],sizeof g_gamedir);char path[1024];gf_language=2;
  snprintf(path,sizeof path,"%s/MEGARACE2.gog",argv[1]);if(!iso_open(path))return 3;
  snprintf(path,sizeof path,"%s/M.EXE",argv[1]);if(!mexe_load(path))return 4;
  for(int bus=0;bus<AUDIO_COUNT;bus++)g_audio_percent[bus]=0;
  // Check all 14 vehicle meshes in every original track texture/model set.
  g_shadow_pass=1;
  for(int track=0;track<6;track++)for(int car=0;car<NCARS;car++){
    Model *m=load_model_from(TRACKS[track].set,CARS[car].model);
    check(m&&m->loaded,"original vehicle model loads");if(!m)continue;
    int spin=0;for(int node=0;node<m->nn;node++)spin+=m->nodes[node].spin;
    // INTRUDER/HUNTER are single-node original meshes: their wheel artwork is
    // baked into the body. Preserve those meshes instead of rotating the car.
    check(spin==(m->nn==3?2:0),"vehicles retain their original separate wheel nodes without rotating single-node bodies");
    matrix_count=0;draw_model_mv(*m,m4_id(),0,0);M4 stopped[32];int count=matrix_count;mem_cpy(stopped,matrices,sizeof matrices);
    matrix_count=0;draw_model_mv(*m,m4_id(),.31f,0);
    check(matrix_count==count&&count==m->nn,"renderer uploads one transform per vehicle node");
    for(int node=0;node<count;node++){
      int changed=memcmp(&stopped[node],&matrices[node],sizeof(M4))!=0;
      check(changed==m->nodes[node].spin,"wheel phase changes only wheel geometry, preserving the body");
      check(near(stopped[node].m[12],matrices[node].m[12])&&near(stopped[node].m[13],matrices[node].m[13])&&near(stopped[node].m[14],matrices[node].m[14]),"wheels rotate about their original axle centres without orbiting");
    }
    model_free(m);sys_free(m);
  }
  g_shadow_pass=0;
  // +X rotation moves a wheel's bottom backwards in model +Z-forward space.
  M4 forward=m4_rotx(.1f);check(-forward.m[6]<0,"forward wheel rotation moves the tyre contact patch backwards");
  for(int mode=0;mode<2;mode++)for(int role=0;role<2;role++)for(int speed=-600;speed<=1100;speed+=100){
    g_tex_nearest=mode;mem_zero(&RC,sizeof RC);RC.ncar=1;RCar &c=RC.car[0];
    c.player=role;c.autopilot=1;c.profile=1;c.v=c.vlong=speed*V_RAW;c.wheel=1.234;
    original_car_drive(c,0,0,.04);
    int increment=(int)(((int64_t)c.fixed.motor*21*2048)>>16);
    check(near(c.wheel,1.234+increment*6.2831853071795864769/1024),"player and opponents expose the original speed-dependent wheel phase in both modes");
    check(c.fixed.wheel==(increment&1023),"rendering leaves the verified original wheel state unchanged");
    check(!increment||((c.wheel>1.234)==(increment>0)),"reversing reverses visible wheel rotation");
  }
  mem_zero(&RC,sizeof RC);RC.ncar=1;RCar &c=RC.car[0];c.player=1;c.profile=1;c.wheel=2.345;
  original_car_drive(c,0,0,.04);check(near(c.wheel,2.345),"stopped vehicle keeps its wheel phase");
  c.v=c.vlong=1036*V_RAW;c.autopilot=1;c.fixed_ready=0;double before=c.wheel;original_car_drive(c,1,0,.04);
  check(c.wheel-before>3.141592653589793,"fast wheels retain more than half a rotation per update instead of aliasing backwards");
  // Full race updates and replay serialization use precisely the same phase.
  A.start_frame=0;A.objtest=0;g_damage_effects=0;
  if(!track_load(0,0))return 5;Loadout lo;default_loadout(&lo);int opponents[]={0,1,3,4,5,6,7};race_setup(2,lo,0,opponents,7,7,5);TK.nobj=0;
  Input input={};mem_zero(g_snap,sizeof g_snap);snap_take();race_update(&input,.04);snap_take();
  for(int car=0;car<8;car++){
    check(RC.car[car].wheel>0,"actual race updates animate every car's wheels");
    double previous=g_snap[0].wheel[car],latest=g_snap[1].wheel[car];
    check(near(car_render_wheel(car,.5),(previous+latest)*.5),"60 FPS in-between picture interpolates the full wheel rotation");
    check(near(car_render_wheel(car,-1),RC.car[car].wheel),"original cadence and replays use their stored wheel phase directly");
  }
  g_snap[0].cgen[0]--;check(near(car_render_wheel(0,.3),RC.car[0].wheel),"recovery jumps do not interpolate stale wheel snapshots");
  double stopped_wheel=RC.car[0].wheel;RC.car[0].out=1;race_update(&input,.04);check(near(RC.car[0].wheel,stopped_wheel),"retired cars stop their wheels without snapping back to zero");
  rec_start();rec_capture();float recorded=(float)RC.car[1].wheel;RC.car[1].wheel=-999;
  replay_free();RPL.h=g_rec_h;RPL.d=g_rec.d;RPL.n=g_rec.n;replay_index();replay_apply(0);
  check(near(RC.car[1].wheel,recorded,1e-6),"recorded race replays preserve the repaired wheel rotation");replay_free();
  printf("Wheel rotation: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
