// Numerical integration probe. All original fixtures remain in the caller's
// temporary directory; only CSV states are printed.
#include "platform_stdio.inc"
#include "../src/core.cpp"
#define PROBE_STUB(ret,name,args) static ret probe_##name args{return (ret)0;}
GL_FUNCS(PROBE_STUB)
#undef PROBE_STUB
int main(int argc,char **argv){
  if(argc<3)return 2;g_heap=GetProcessHeap();
#define PROBE_GL(ret,name,args) name=probe_##name;
  GL_FUNCS(PROBE_GL)
#undef PROBE_GL
  gf_language=2;str_cpy(g_gamedir,argv[1],sizeof g_gamedir);
  char path[1024];snprintf(path,sizeof path,"%s/MEGARACE2.gog",argv[1]);if(!iso_open(path))return 3;
  snprintf(path,sizeof path,"%s/M.EXE",argv[1]);if(!mexe_load(path))return 4;
  for(int track=0;track<6;track++){
    char disc[128];snprintf(disc,sizeof disc,"%s.PST",TRACKS[track].cd);size_t n=0;u8 *data=iso_load(disc,&n);if(!data)return 5;
    snprintf(path,sizeof path,"%s/track-%d.PST",argv[2],track);int ok=sys_write_file(path,data,n);sys_free(data);if(!ok)return 6;
  }
  for(int i=0;i<AUDIO_COUNT;i++)g_audio_percent[i]=0;
  A.start_frame=0;A.objtest=0;g_ai_rescue=0;g_sim_model=0;g_damage_effects=0;
  int steps=argc>3?atoi(argv[3]):1200;
  printf("track,step,car,node,x,angle,steer,lateral,forward,speed,along\n");
  for(int track=0;track<6;track++){
    race_end_sounds();if(!track_load(track,0))return 7;Loadout lo;default_loadout(&lo);int opp[]={0,1,3,4,5,6,7};
    g_rng=12345;g_rng_fx=54321;RC.heat=0;race_setup(2,lo,0,opp,7,7,5);TK.nobj=0;Input input={};
    for(int step=0;step<steps;step++){
      // Isolate the low-level pilot/drive/road/contact chain from race tactics,
      // weapons, retirement and finish-line state changes.
      for(int i=0;i<8;i++){RC.car[i].autopilot=1;RC.car[i].prog=-1000000;RC.car[i].out=0;RC.car[i].damage=0;}
      race_update(&input,.04);
      for(int i=0;i<8;i++){const RCar &c=RC.car[i];printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",track,step,i,c.pp.i,contact_raw(c.x),c.fixed.angle,c.fixed.steer,c.fixed.old_x,c.fixed.old_z,c.fixed.speed,contact_raw(c.pp.t*physics_length(c.pp.i,c.pp.nx)));}
    }
  }
}
