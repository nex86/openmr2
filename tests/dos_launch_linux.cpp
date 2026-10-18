// Exercise the actual fork/exec launcher against a harmless fake DOSBox.
#include "../src/sys.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gamefiles.h"
static Display *g_display;static Window g_window;
static int x_exists(const char *p){struct stat st;return !stat(p,&st)&&S_ISREG(st.st_mode);}
#include "../src/startup_x11.inc"
static int failures,checks;
static void test(int ok,const char *name){checks++;if(!ok){failures++;fprintf(stderr,"FAIL: %s\n",name);}}
static void file(const char *p,const char *text){FILE *f=fopen(p,"wb");if(!f)exit(2);fwrite(text,1,strlen(text),f);fclose(f);}
int main(){
  mkdir("dos-launch-linux",0755);if(chdir("dos-launch-linux"))return 2;mkdir("GOG Game",0755);mkdir("GOG Game/DOSBOX",0755);mkdir("bin",0755);
  file("GOG Game/dosboxmegarace2.conf","");file("GOG Game/dosboxmegarace2_single.conf","");
  char base[4096],bin[4200],error[600];if(!getcwd(base,sizeof base))return 2;snprintf(bin,sizeof bin,"%s/bin",base);setenv("PATH",bin,1);
  gf_status=GOG_COMPLETE;gf_language=0;unlink("bin/dosbox");
  test(!sys_run_dos("GOG Game",error,sizeof error)&&strstr(error,"could not be started"),"exec failure is reported before OpenMR2 would quit");
  file("bin/dosbox","#!/bin/sh\nprintf '%s\\n' \"$PWD\" \"$@\" > launch.args\n");chmod("bin/dosbox",0755);
  test(sys_run_dos("GOG Game",error,sizeof error),"successful exec is acknowledged");int status=0;waitpid(-1,&status,0);test(WIFEXITED(status)&&!WEXITSTATUS(status),"launched child exits normally");
  FILE *f=fopen("GOG Game/DOSBOX/launch.args","rb");char output[6000]={};if(f){size_t count=fread(output,1,sizeof output-1,f);output[count]=0;fclose(f);}char expected[6000];
  snprintf(expected,sizeof expected,"%s/GOG Game/DOSBOX\n-conf\n../dosboxmegarace2.conf\n-conf\n../dosboxmegarace2_single.conf\n-noconsole\n-c\nexit\n",base);
  test(!strcmp(output,expected),"exact GOG arguments and working directory reach DOSBox, including paths with spaces");
  gf_status=GOG_UNKNOWN;test(!sys_run_dos("GOG Game",error,sizeof error),"unknown editions cannot spawn DOSBox");
  gf_status=GOG_COMPLETE;test(!sys_run_dos("bin",error,sizeof error),"missing GOG configurations fail safely");
  printf("Linux DOS launcher: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
