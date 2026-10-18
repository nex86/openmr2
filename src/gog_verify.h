// Shared by the game and native tests. Unknown files never imply German.
#pragma once
#include "sha256.h"
#include "gog_manifest.h"
#include "game_directory.h"
static int gf_ieq(const char *a,const char *b) {
  while(*a && *b){if((*a|32)!=(*b|32))return 0;a++;b++;}return *a==*b;
}
enum { GOG_UNKNOWN=0, GOG_COMPLETE=1, GOG_INCOMPLETE=2 };
static int gf_language = -1, gf_status;
static const char *gf_text(const char *en,const char *fr,const char *de){return gf_language==1?fr:gf_language==2?de:en;}
static void gf_join(char *p, const char *dir, const char *name) {
  int n=0;while(*dir && n<598)p[n++]=*dir++;p[n++]='/';while(*name && n<698)p[n++]=*name++;p[n]=0;
}
static int gf_present(const char *dir,int (*exists)(const char *)){
  char path[700];for(int i=0;i<GOG_FILE_COUNT-1;i++){gf_join(path,dir,GOG_FILES[i].name);if(!exists(path))return 0;}
  for(int i=0;i<GF_NIMAGES;i++){gf_join(path,dir,GF_IMAGES[i]);if(exists(path))return 1;}return 0;
}
static int gf_check(const char *dir, int (*exists)(const char *), char *list, int maxlist) {
  gf_language=-1;gf_status=GOG_UNKNOWN;
  if(list && maxlist)list[0]=0;
  int count=0,len=0;
  auto add=[&](const char *name){count++;if(!list)return;for(int i=0;name[i] && len+2<maxlist;i++)list[len++]=name[i];if(len+1<maxlist)list[len++]='\n';if(maxlist)list[len<maxlist?len:maxlist-1]=0;};
  char path[700], hash[65];
  gf_join(path,dir,"M.EXE");
  int have_m=exists(path);
  if(have_m && sha256_file(path,hash))for(int i=0;i<3;i++)if(gf_ieq(hash,GOG_FILES[0].sha[i]))gf_language=i;
  // A missing executable can still be classified from an intact known GOG image.
  const char *image=0;
  for(int i=0;i<GF_NIMAGES && !image;i++){gf_join(path,dir,GF_IMAGES[i]);if(exists(path))image=GF_IMAGES[i];}
  char image_hash[65];int image_ok=0;
  if(image){gf_join(path,dir,image);image_ok=sha256_file(path,image_hash);}
  if(gf_language<0 && !have_m && image_ok)for(int i=0;i<3;i++)if(gf_ieq(image_hash,GOG_FILES[GOG_FILE_COUNT-1].sha[i]))gf_language=i;
  if(gf_language<0){add("Unsupported or unknown game version/language. Only the German, English and French GOG data sets are supported.");return count;}
  gf_status=GOG_INCOMPLETE;
  for(int i=0;i<GOG_FILE_COUNT;i++){
    const GogFile &f=GOG_FILES[i];
    if(i==GOG_FILE_COUNT-1){if(!image_ok || !gf_ieq(image_hash,f.sha[gf_language]))add(gf_text("Missing, damaged or mismatched GOG CD image (MEGARACE2.gog).","Image GOG absente, endommag\xe9""e ou incompatible (MEGARACE2.gog).","GOG-CD-Abbild fehlt, ist besch\xe4""digt oder passt nicht zur Sprachfassung (MEGARACE2.gog)."));continue;}
    gf_join(path,dir,f.name);
    if(!exists(path) || !sha256_file(path,hash) || !gf_ieq(hash,f.sha[gf_language]))add(f.name);
  }
  if(!count)gf_status=GOG_COMPLETE;
  return count;
}
static void (*gf_progress_begin)();
static void (*gf_progress_end)();
static int gf_startup_check(const char *dir,int (*exists)(const char *),char *list,int cap){
  int edition=cfg_verified_language(dir);
  if(edition>=0){gf_language=edition;gf_status=gf_present(dir,exists)?GOG_COMPLETE:GOG_INCOMPLETE;
    if(list && cap)list[0]=0;if(gf_status==GOG_COMPLETE)return 0;
    cfg_store_verification(dir,edition,0);return 1;
  }
  if(gf_progress_begin)gf_progress_begin();int errors=gf_check(dir,exists,list,cap);if(gf_progress_end)gf_progress_end();
  cfg_store_verification(dir,gf_language,!errors);return errors;
}
static int gf_startup_candidate(const char *dir,int (*exists)(const char *)){
  int edition=cfg_verified_language(dir);
  if(edition>=0)return !gf_startup_check(dir,exists,0,0);
  return gf_present(dir,exists) && !gf_startup_check(dir,exists,0,0);
}
