// Stable source-relative texture identities. LOCAL and CD never alias each other.
#include "sha256.h"
#include "png.inc"
static int g_custom_textures, g_dump_active,g_texture_reload;
static int g_revived_ui=1,g_ui_assets_allowed=1;
struct TextureImage { char key[512]; u8 *pixels; int w,h; GLuint tex; };
static Arr<TextureImage> g_texture_cache={};
static size_t g_texture_bytes;
static int g_texture_entry_warning;
static void texture_key(char *out,const char *rel,int cd){
  str_cpy(out,cd?"CD/":"LOCAL/",512);
  const char *p=rel;
  size_t root=str_len(g_gamedir);int prefix=root>0;
  for(size_t i=0;i<root;i++)if(!p[i] || up(p[i])!=up(g_gamedir[i])){prefix=0;break;}
  if(prefix && (p[root]=='/'||p[root]=='\\'))p+=root+1;
  size_t n=str_len(out);
  for(;*p && n<475;p++){char c=*p=='\\'?'/':up(*p);if(c==':' || c<32){out[0]=0;return;}out[n++]=c;}out[n]=0;
  if(str_icontains(out,"../") || str_icontains(out,"/.."))out[0]=0;
}
static void texture_palette_id(char out[65],const u8 *pal){Sha256 hash;hash.init();hash.update(pal,768);hash.finish(out);}
static void texture_sprite_key(char out[512],const char *source,int frame,const u8 *pal){
  str_cpy(out,source,512);str_cat(out,"/frame_",512);if(frame<10)str_cat(out,"00",512);else if(frame<100)str_cat(out,"0",512);str_int(out,512,frame);
  char hash[65];texture_palette_id(hash,pal);str_cat(out,"_pal_",512);str_cat(out,hash,512);str_cat(out,".png",512);
}
static void texture_log(const char *action,const char *key,int w=0,int h=0,const u8 *pixels=0){
  if(!sys_debug_enabled())return;char msg[900]="[textures] ";str_cat(msg,action,sizeof msg);str_cat(msg,": ",sizeof msg);str_cat(msg,key,sizeof msg);
  if(w>0){str_cat(msg,"; ",sizeof msg);str_int(msg,sizeof msg,w);str_cat(msg,"x",sizeof msg);str_int(msg,sizeof msg,h);str_cat(msg," RGBA8",sizeof msg);}
  if(pixels){int transparent=0,partial=0;for(int i=0;i<w*h;i++){unsigned a=pixels[i*4+3];if(a==0)transparent=1;else if(a<255)partial=1;}str_cat(msg,partial?"; straight alpha (partial coverage)":transparent?"; straight alpha (transparent pixels)":"; opaque",sizeof msg);}sys_log(msg);
}
static TextureImage *texture_custom(const char *key){
  if(!g_custom_textures || !key[0])return 0;
  for(int i=0;i<g_texture_cache.n;i++)if(!str_icmp(key,g_texture_cache.p[i].key))return g_texture_cache.p[i].pixels?&g_texture_cache.p[i]:0;
  if(g_texture_cache.n>=8192){if(!g_texture_entry_warning){sys_log("[error] Custom texture lookup limit reached (8192 entries). Using originals until cache reload.");g_texture_entry_warning=1;}return 0;}
  TextureImage t={};str_cpy(t.key,key,sizeof t.key);char path[600]="revived_config/texture_replacements/";str_cat(path,key,sizeof path);
  t.pixels=png_read(path,&t.w,&t.h);
  if(t.pixels){GLint maximum=0;glGetIntegerv(0x0D33,&maximum);if(maximum>0 && (t.w>maximum || t.h>maximum)){sys_free(t.pixels);t.pixels=0;texture_log("[error] PNG exceeds device texture limit; using original",path,t.w,t.h);}}
  if(t.pixels){size_t bytes=(size_t)t.w*t.h*4;if(g_texture_bytes+bytes>268435456){sys_free(t.pixels);t.pixels=0;sys_log("[error] Custom texture memory limit reached (256 MiB). Using original.");}else g_texture_bytes+=bytes;}
  if(t.pixels)texture_log("Loaded replacement",path,t.w,t.h,t.pixels);
  else if(sys_debug_enabled()){void *f=sys_fopen(path);if(f){sys_fclose(f);texture_log("[error] Invalid or oversized PNG; using original",path);}else texture_log("No replacement; using original",path);}
  g_texture_cache.push(t);return t.pixels?&g_texture_cache.p[g_texture_cache.n-1]:0;
}
static GLuint texture_gpu(TextureImage *t,int mip,int clamp){
  if(!t)return 0;
  if(!t->tex){glGenTextures(1,&t->tex);glBindTexture(GL_TEXTURE_2D,t->tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,t->w,t->h,0,GL_RGBA,GL_UNSIGNED_BYTE,t->pixels);if(mip)glGenerateMipmap(GL_TEXTURE_2D);}
  glBindTexture(GL_TEXTURE_2D,t->tex);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,mip?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,clamp?GL_CLAMP_TO_EDGE:GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,clamp?GL_CLAMP_TO_EDGE:GL_REPEAT);return t->tex;
}
static void texture_cache_clear(){
  for(int i=0;i<g_texture_cache.n;i++){if(g_texture_cache.p[i].tex)glDeleteTextures(1,&g_texture_cache.p[i].tex);sys_free(g_texture_cache.p[i].pixels);}
  g_texture_cache.release();g_texture_bytes=0;g_texture_entry_warning=0;texture_log("Replacement cache cleared","edited PNGs will be read on next use");
}
static int texture_dump_rgba(const char *key,const u8 *pixels,int w,int h,const char *attributes,const u8 *legacy_pixels=0){
  char path[600]="revived_config/texture_dumps/";str_cat(path,key,sizeof path);
  // Never replace an existing export: users may be editing or comparing it.
  void *existing=sys_fopen(path);if(existing){sys_fclose(existing);int repair=0;
    if(legacy_pixels){int ow=0,oh=0;u8 *old=png_read(path,&ow,&oh);repair=old && ow==w && oh==h;int different=0;
      if(repair)for(size_t i=0;i<(size_t)w*h*4;i++){if(old[i]!=legacy_pixels[i])repair=0;if(pixels[i]!=legacy_pixels[i])different=1;}repair=repair&&different;sys_free(old);}
    if(!repair){texture_log("Kept existing dump",path);return 1;}texture_log("Refreshing unchanged legacy dump with corrected palette",path);
  }
  int ok=png_write(path,pixels,w,h);
  texture_log(ok?"Exported original PNG":"[error] Could not export PNG",path,w,h,pixels);
  if(ok && attributes){str_cat(path,".txt",sizeof path);ok=sys_write_file(path,attributes,str_len(attributes));if(!ok)texture_log("[error] Could not write PNG attributes",path);}
  return ok;
}
