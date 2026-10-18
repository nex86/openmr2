#pragma once
#include "game_directory.h"
static int startup_choice=-1;
int sys_startup_mode(){return startup_choice;}
static int (*startup_choose)(int,const char *,const char *,const char *,const char *,const char *,int *)=sys_choose_mode;
static int startup_before_window(int argc,char **argv){
  startup_choice=-1;
  // Explicit automated render/export/validation entry points remain unattended.
  for(int i=1;i<argc;i++){const char *skip[]={"--shot","--dump-textures","--demo","--quit","--validate","--dumpvideo"};
    for(unsigned j=0;j<sizeof skip/sizeof skip[0];j++){const char *a=argv[i],*b=skip[j];while(*a&&*a==*b){a++;b++;}if(!*a&&!*b)return 1;}}
  const char *path="revived_config/openmr2_setup.cfg";size_t n=0;unsigned char *text=sys_read_file(path,&n);char value[32];
  cfg_value(text,n,"startup_mode",value,sizeof value);int mode=value[0]=='1'&&!value[1];
  cfg_value(text,n,"remember_startup_mode",value,sizeof value);int remember=value[0]=='1'&&!value[1];
  if(remember){startup_choice=mode;sys_free(text);return 1;}
  int chosen=startup_choose(mode,"OpenMR2 - Startup mode","Choose how you want to start OpenMR2:","Revived Mode","Retro Mode","Remember my choice",&remember);
  if(chosen<0){sys_free(text);return 0;}
  startup_choice=chosen?1:0;
  size_t cap=n+140,used=0;char *output=(char *)sys_alloc(cap);if(!output){sys_free(text);return 0;}
  auto put=[&](const char *s){while(*s&&used+1<cap)output[used++]=*s++;};
  // Preserve all other settings, comments and verification receipts.
  size_t a=n>=3&&text[0]==239&&text[1]==187&&text[2]==191?3:0;
  while(a<n){size_t e=a;while(e<n&&text[e]!='\n')e++;char v[32];int skip=cfg_value(text+a,e-a,"startup_mode",v,sizeof v)||cfg_value(text+a,e-a,"remember_startup_mode",v,sizeof v);
    if(!skip)for(size_t p=a;p<e+(e<n);p++)output[used++]=text[p];a=e+1;}
  put("\r\nstartup_mode = ");put(startup_choice?"1":"0");put("\r\nremember_startup_mode = ");put(remember?"1":"0");put("\r\n");
  int ok=sys_write_file(path,output,used);sys_free(output);sys_free(text);
  if(!ok)sys_log("[error] The startup mode preference could not be saved.");
  return 1;
}
