#pragma once
#include "sys.h"
static unsigned char cfg_ascii_lower(unsigned char c) { return c>='A' && c<='Z' ? c+32 : c; }
// Shared, bounded parser for startup and settings. English keys take precedence.
static int game_directory_from_config(const unsigned char *text, size_t n, char *out, int cap) {
  out[0]=0; int priority=-1;
  size_t a=n>=3 && text[0]==239 && text[1]==187 && text[2]==191?3:0;
  while(a<n){
    size_t e=a;while(e<n && text[e]!='\n')e++;
    size_t p=a;a=e+1;while(p<e && (text[p]==' '||text[p]=='\t'))p++;
    const char *keys[]={"game_directory","spielverzeichnis"};
    for(int key=0;key<2;key++){
      size_t i=p,k=0;while(i<e && keys[key][k] && cfg_ascii_lower(text[i])==keys[key][k]){i++;k++;}
      if(keys[key][k] || (i<e && text[i]!=' ' && text[i]!='\t' && text[i]!='='))continue;
      while(i<e && (text[i]==' '||text[i]=='\t'))i++;
      if(i==e || text[i++]!='=')continue;
      while(i<e && (text[i]==' '||text[i]=='\t'))i++;
      size_t end=e;while(end>i && (text[end-1]=='\r'||text[end-1]==' '||text[end-1]=='\t'))end--;
      if(end>i+1 && (text[i]=='"'||text[i]=='\'') && text[end-1]==text[i]){i++;end--;}
      int rank=2-key;if(rank<priority || end-i>=(size_t)cap)continue;priority=rank;
      int j=0;while(i<end)out[j++]=(char)text[i++];out[j]=0;
    }
  }
  return out[0]!=0;
}
// Verification metadata lives in the same CFG and survives normal settings saves.
static int cfg_value(const unsigned char *t,size_t n,const char *key,char *out,int cap){
  out[0]=0;size_t a=n>=3 && t[0]==239 && t[1]==187 && t[2]==191?3:0;
  while(a<n){size_t e=a;while(e<n && t[e]!='\n')e++;size_t p=a;a=e+1;
    while(p<e && (t[p]==' '||t[p]=='\t'))p++;int k=0;
    while(p<e && key[k] && cfg_ascii_lower(t[p])==cfg_ascii_lower(key[k])){p++;k++;}
    if(key[k])continue;while(p<e && (t[p]==' '||t[p]=='\t'))p++;
    if(p==e || t[p++]!='=')continue;while(p<e && (t[p]==' '||t[p]=='\t'))p++;
    size_t end=e;while(end>p && (t[end-1]=='\r'||t[end-1]==' '||t[end-1]=='\t'))end--;
    if(end-p>=(size_t)cap)continue;int j=0;while(p<end)out[j++]=t[p++];out[j]=0;
  }return out[0]!=0;
}
static const char *const CFG_VERIFY_KEYS[]={"Files_OK","verified_directory","verified_language"};
static size_t cfg_verification_lines(const unsigned char *t,size_t n,char *out,size_t cap){
  size_t used=0;for(int i=0;i<3;i++){char v[700];if(!cfg_value(t,n,CFG_VERIFY_KEYS[i],v,sizeof v))continue;
    const char *parts[]={CFG_VERIFY_KEYS[i]," = ",v,"\r\n"};for(int j=0;j<4;j++)for(const char *p=parts[j];*p && used+1<cap;p++)out[used++]=*p;
  }out[used]=0;return used;
}
static int cfg_verified_language(const char *dir){
  size_t n=0;unsigned char *t=sys_read_file("revived_config/openmr2_setup.cfg",&n);char ok[32],path[700],lang[32];
  cfg_value(t,n,"Files_OK",ok,sizeof ok);cfg_value(t,n,"verified_directory",path,sizeof path);cfg_value(t,n,"verified_language",lang,sizeof lang);sys_free(t);
  if(!(ok[0]=='t'&&ok[1]=='r'&&ok[2]=='u'&&ok[3]=='e'&&!ok[4]))return -1;
  const char *a=dir,*b=path;while(*a && *b){unsigned char x=*a++,y=*b++;
#ifdef _WIN32
    x=cfg_ascii_lower(x);y=cfg_ascii_lower(y);if(x=='\\')x='/';if(y=='\\')y='/';
#endif
    if(x!=y)return -1;
  }if(*a||*b)return -1;return lang[0]>='0'&&lang[0]<='2'&&!lang[1]?lang[0]-'0':-1;
}
static void cfg_store_verification(const char *dir,int language,int valid){
  size_t n=0;unsigned char *t=sys_read_file("revived_config/openmr2_setup.cfg",&n);size_t cap=n+1400,used=0;char *o=(char *)sys_alloc(cap);
  auto put=[&](const char *s){while(*s && used+1<cap)o[used++]=*s++;};
  put(valid?"Files_OK = true\r\n":"Files_OK = false\r\n");put("verified_directory = ");put(dir);put("\r\nverified_language = ");char lang[]={char('0'+language),0};put(language>=0&&language<3?lang:"unknown");put("\r\n");
  size_t a=n>=3 && t[0]==239 && t[1]==187 && t[2]==191?3:0;while(a<n){size_t e=a;while(e<n && t[e]!='\n')e++;int skip=0;for(int i=0;i<3;i++){char value[700];if(cfg_value(t+a,e-a,CFG_VERIFY_KEYS[i],value,sizeof value))skip=1;}
    if(!skip)for(size_t p=a;p<e+(e<n);p++)if(used+1<cap)o[used++]=t[p];a=e+1;
  }sys_write_file("revived_config/openmr2_setup.cfg",o,used);sys_free(o);sys_free(t);
}
