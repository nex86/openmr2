#include "../src/sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <strings.h>
#include "../src/gamefiles.h"
static void *g_heap;
static void *GetProcessHeap(){return 0;}
static void ExitProcess(unsigned code){exit((int)code);}
static size_t s_len(const char *s){return strlen(s);}
static void s_cpy(char *d,const char *s,size_t n){if(n)snprintf(d,n,"%s",s);}
static void s_cat(char *d,const char *s,size_t n){size_t k=strlen(d);if(k<n)s_cpy(d+k,s,n-k);}
void *sys_alloc(size_t n){return calloc(1,n?n:1);}
void *sys_realloc(void *p,size_t n){return realloc(p,n);}
void sys_free(void *p){free(p);}
uint8_t *sys_read_file(const char *path,size_t *n){FILE *f=fopen(path,"rb");if(!f)return 0;fseek(f,0,SEEK_END);long z=ftell(f);rewind(f);if(z<0){fclose(f);return 0;}uint8_t *p=(uint8_t *)calloc(1,(size_t)z+1);*n=fread(p,1,z,f);fclose(f);return p;}
int sys_write_file(const char *path,const void *p,size_t n){char parent[1024];snprintf(parent,sizeof parent,"%s",path);for(char *s=parent+1;*s;s++)if(*s=='/'){*s=0;mkdir(parent,0755);*s='/';}FILE *f=fopen(path,"wb");if(!f)return 0;int ok=fwrite(p,1,n,f)==n;fclose(f);return ok;}
int sys_delete_file(const char *path){return remove(path)==0;}
int sys_dir_exists(const char *path){struct stat st;return stat(path,&st)==0&&S_ISDIR(st.st_mode);}
void sys_list_dir(const char *dir,const char *ext,void (*cb)(void *,const char *,int),void *user){
  DIR *d=opendir(dir);if(!d)return;
  struct dirent *e;int want_dirs=!strcmp(ext,"/");
  while((e=readdir(d))){
    if(e->d_name[0]=='.')continue;
    char path[1024];snprintf(path,sizeof path,"%s/%s",dir,e->d_name);int isdir=sys_dir_exists(path);
    if(want_dirs){if(isdir)cb(user,e->d_name,1);continue;}
    size_t n=strlen(e->d_name),suffix=strlen(ext);
    if(!isdir && n>suffix && !strcasecmp(e->d_name+n-suffix,ext))cb(user,e->d_name,0);
  }
  closedir(d);
}
void sys_log(const char *){}
int sys_debug_enabled(){return 0;}
double sys_time(){timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int sys_render_text(const char *,int,uint8_t **,int *,int *){return 0;}
void *sys_fopen(const char *path){return fopen(path,"rb");}
int sys_fread_at(void *f,uint64_t off,void *p,uint32_t n){return fseeko((FILE *)f,(off_t)off,SEEK_SET)==0&&fread(p,1,n,(FILE *)f)==n;}
uint64_t sys_fsize(void *f){fseeko((FILE *)f,0,SEEK_END);return ftello((FILE *)f);}
void sys_fclose(void *f){fclose((FILE *)f);}
void sys_audio_flush(){}
int sys_audio_open(int){return 0;}
int sys_pad_read(PadState *p){memset(p,0,sizeof *p);return 0;}
void sys_set_fullscreen(int){}
int sys_is_fullscreen(){return 0;}
int sys_game_language(){return gf_language;}
static int gd_exists(const char *path){struct stat st;return stat(path,&st)==0&&S_ISREG(st.st_mode);}
static int gd_check(const char *path,char *details,int n){return gf_check(path,gd_exists,details,n);}

int sys_debug_console(int enabled){return enabled?1:0;}

int sys_choose_mode(int,const char *,const char *,const char *,const char *,const char *,int *){return -1;}
#include "../src/startup_choice.h"
int sys_game_is_gog(){return gf_status==GOG_COMPLETE&&gf_language>=0&&gf_language<3;}
int sys_run_dos(const char *,char *,int){return 0;}

int sys_encoder_open(const char *,int,int){return 0;}
int sys_encoder_video(const void *,size_t){return 0;}
void sys_encoder_audio(const int16_t *,int){}
int sys_encoder_close(){return 0;}
