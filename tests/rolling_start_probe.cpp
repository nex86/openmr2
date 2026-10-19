// Run the actual eight-car rolling start on every supplied track. Private game
// data is read from the native regression fixture location, never redistributed.
#include "platform_stdio.inc"
#ifndef OPENMR2_PROBE_CORE
#define OPENMR2_PROBE_CORE "../src/core.cpp"
#endif
#include OPENMR2_PROBE_CORE
#define PROBE_STUB(ret,name,args) static ret probe_##name args{return (ret)0;}
GL_FUNCS(PROBE_STUB)
#undef PROBE_STUB
int main(){
  g_heap=GetProcessHeap();
#define PROBE_GL(ret,name,args) name=probe_##name;
  GL_FUNCS(PROBE_GL)
#undef PROBE_GL
  gf_language=2;str_cpy(g_gamedir,"../../../Megarace 2 German",sizeof g_gamedir);
  if(!iso_open("../../../Megarace 2 German/MEGARACE2.gog")||!mexe_load("../../../Megarace 2 German/M.EXE"))return 2;
  for(int i=0;i<AUDIO_COUNT;i++)g_audio_percent[i]=0;
  A.start_frame=0;A.objtest=0;g_ai_rescue=0;g_sim_model=0;
  printf("track,step,car,autopilot,node,x,heading,steer,lateral,speed,damage\n");
  for(int track=0;track<6;track++){
    race_end_sounds();if(!track_load(track,0))return 3;
    Loadout lo;default_loadout(&lo);int opp[]={0,1,3,4,5,6,7};
    g_rng=12345;g_rng_fx=54321;RC.heat=0;race_setup(2,lo,0,opp,7,7,5);
    Input input={};
    for(int step=0;step<75;step++){
      race_update(&input,.04);
      for(int i=0;i<8;i++){const RCar &c=RC.car[i];
        printf("%d,%d,%d,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",track,step,i,c.autopilot,c.pp.i,c.x,c.psi,c.steer_st,c.vlat,c.v,c.damage);
      }
    }
  }
  return 0;
}
