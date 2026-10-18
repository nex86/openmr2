// Eager export of the entire loose-file and CD graphics catalog, in small frame-sized jobs.
// SPR has no palette: enumerate every real palette in its source directory, identified by SHA-256.
struct DumpAsset { char key[512]; int cd,sprite; };
struct DumpPalette { char directory[512]; u8 pal[768]; };
static Arr<DumpAsset> g_dump_assets={};
static Arr<DumpPalette> g_dump_palettes={};
struct DumpSeen {char key[512];};
static Arr<DumpSeen> g_dump_seen={};
static int g_dump_slots[131072];
static void texture_dump_finished_sound();
static double g_dump_overlay_until;
static int g_dump_index,g_dump_palette_index,g_dump_pngs,g_dump_errors,g_dump_complete;
static int texture_dump_once(const char *key){
  unsigned hash=2166136261u;for(const char *p=key;*p;p++)hash=(hash^(u8)*p)*16777619u;
  unsigned bucket=hash&131071u;
  while(g_dump_slots[bucket]){if(!str_icmp(key,g_dump_seen.p[g_dump_slots[bucket]-1].key))return 0;bucket=(bucket+1)&131071u;}
  if(g_dump_seen.n>=100000)return 0;
  DumpSeen name={};str_cpy(name.key,key,sizeof name.key);g_dump_seen.push(name);g_dump_slots[bucket]=g_dump_seen.n;return 1;
}
static void texture_dump_sprite(const Sprite &s,const u8 *pal){
  if(!s.texture_source[0] || !s.px || s.w<=0 || s.h<=0)return;
  char key[512];texture_sprite_key(key,s.texture_source,s.frame,pal);if(!texture_dump_once(key))return;
  u8 *pixels=(u8 *)sys_alloc((size_t)s.w*s.h*4);
  for(int i=0;i<s.w*s.h;i++){int c=s.px[i];mem_cpy(pixels+i*4,pal+c*3,3);pixels[i*4+3]=c?255:0;}
  char meta[800]="Source: ";str_cat(meta,s.texture_source,sizeof meta);str_cat(meta,"\nFrame: ",sizeof meta);str_int(meta,sizeof meta,s.frame);
  str_cat(meta,"\nOriginal x: ",sizeof meta);str_int(meta,sizeof meta,s.source_x);str_cat(meta,"\nOriginal y: ",sizeof meta);str_int(meta,sizeof meta,s.source_y);
  str_cat(meta,"\nOriginal width: ",sizeof meta);str_int(meta,sizeof meta,s.w);str_cat(meta,"\nOriginal height: ",sizeof meta);str_int(meta,sizeof meta,s.h);
  str_cat(meta,"\nRGBA8, straight alpha. Original index 0 is transparent; other pixels are opaque.\nPalette SHA-256 is part of the PNG filename. Keep this relative path for replacements.\n",sizeof meta);
  if(texture_dump_rgba(key,pixels,s.w,s.h,meta))g_dump_pngs++;else g_dump_errors++;
  sys_free(pixels);
}
static u8 *texture_source_read(const char *key,size_t *n){
  if(!str_icmp(key,""))return 0;
  if(key[0]=='C' && key[1]=='D' && key[2]=='/')return iso_load(key+3,n);
  char path[1024];str_cpy(path,g_gamedir,sizeof path);str_cat(path,"/",sizeof path);str_cat(path,key+6,sizeof path);return sys_read_file(path,n);
}
static void dump_directory_name(char *out,const char *key){str_cpy(out,key,512);char *last=0;for(char *p=out;*p;p++)if(*p=='/')last=p;if(last)*last=0;}
static void dump_add_palette(const char *key,const u8 *pal){
  char directory[512];dump_directory_name(directory,key);
  for(int i=0;i<g_dump_palettes.n;i++)if(!str_icmp(directory,g_dump_palettes.p[i].directory)){
    int same=1;for(int k=0;k<768;k++)if(pal[k]!=g_dump_palettes.p[i].pal[k]){same=0;break;}if(same)return;
  }
  DumpPalette p={};str_cpy(p.directory,directory,sizeof p.directory);mem_cpy(p.pal,pal,768);g_dump_palettes.push(p);
}
struct DumpWalk { char relative[512]; int cd,depth; };
static void dump_walk(const char *,int,int);
static void dump_walk_cb(void *u,const char *name,int directory){
  DumpWalk *w=(DumpWalk *)u;char rel[512];str_cpy(rel,w->relative,sizeof rel);if(rel[0])str_cat(rel,"/",sizeof rel);str_cat(rel,name,sizeof rel);
  if(directory){if(w->depth<8)dump_walk(rel,w->cd,w->depth+1);return;}
  size_t n=str_len(name);if(n<4)return;
  int sprite=!str_icmp(name+n-4,".SPR"),picture=!str_icmp(name+n-4,".HLZ"),model=!str_icmp(name+n-4,".M3D");if(!sprite&&!picture&&!model)return;
  DumpAsset a={};texture_key(a.key,rel,w->cd);a.cd=w->cd;a.sprite=model?2:sprite;g_dump_assets.push(a);
}
static void dump_walk(const char *relative,int cd,int depth){
  DumpWalk w={};str_cpy(w.relative,relative,sizeof w.relative);w.cd=cd;w.depth=depth;
  if(cd)iso_list_all(relative,dump_walk_cb,&w);
  else {char path[1024];str_cpy(path,g_gamedir,sizeof path);str_cat(path,"/",sizeof path);str_cat(path,relative,sizeof path);sys_list_dir(path,"/",dump_walk_cb,&w);sys_list_dir(path,".HLZ",dump_walk_cb,&w);sys_list_dir(path,".SPR",dump_walk_cb,&w);sys_list_dir(path,".M3D",dump_walk_cb,&w);}
}
static void texture_dump_start(){
  if(g_dump_active){sys_log("[event] Texture export already running; continuing current job.");return;}
  g_dump_active=1;g_dump_overlay_until=0;
  g_dump_assets.release();g_dump_palettes.release();g_dump_seen.release();mem_zero(g_dump_slots,sizeof g_dump_slots);g_dump_index=g_dump_palette_index=g_dump_pngs=g_dump_errors=g_dump_complete=0;
  const char *roots[]={"CARS","OBJS","DATA640","SOURIS","TEXTE","INSTALL"};for(int i=0;i<6;i++)dump_walk(roots[i],0,0);dump_walk("",1,0);
  // Pictures first, so every SPR gets all real palette variants from its own directory.
  for(int i=1;i<g_dump_assets.n;i++){DumpAsset a=g_dump_assets.p[i];int j=i-1;while(j>=0 && g_dump_assets.p[j].sprite>a.sprite){g_dump_assets.p[j+1]=g_dump_assets.p[j];j--;}g_dump_assets.p[j+1]=a;}
  const char *readme="OpenMR2 texture dumps (original assets only)\nCopy only edited PNG files into texture_replacements, keeping the complete relative path.\nLOCAL preserves loose-file directories: CARS/<world>, OBJS/<world>, DATA640, TEXTE, SOURIS. CD preserves disc directories, including track HUD graphics.\nHLZ files use <original-name>.HLZ.png; SPR uses <original-name>.SPR/frame_NNN_pal_<SHA256>.png. SPR palettes are contextual, so all palettes from the source directory are exported. Runtime can add further actually used variants.\nRGBA8 straight alpha. SPR index 0 is transparent. Font atlases export only ink pixels; font metrics stay original. Model additive/translucent/shadow material attributes remain in the original M3D. PNG alpha multiplies those rules in Revived.\nBackground pictures are opaque unless edited. Higher resolutions keep the original display rectangle and UV coordinates.\nNever rename paths or metadata. Original files are never modified. Video frames are not static textures and are not exported.\n";
  sys_write_file("revived_config/texture_dumps/README.txt",readme,str_len(readme));sys_log("[event] Texture catalog export started: texture_dumps.");
}
static void texture_dump_step(){
  if(!g_dump_active || g_dump_complete)return;
  if(g_dump_index>=g_dump_assets.n){g_dump_complete=1;g_dump_active=0;g_dump_overlay_until=sys_time()+2.5;texture_dump_finished_sound();char complete[160]="PNGs: ";str_int(complete,sizeof complete,g_dump_pngs);str_cat(complete,"; errors: ",sizeof complete);str_int(complete,sizeof complete,g_dump_errors);texture_log("Catalog export completed",complete);return;}
  DumpAsset &a=g_dump_assets.p[g_dump_index];size_t n=0;u8 *data=texture_source_read(a.key,&n);
  if(!data){g_dump_errors++;g_dump_index++;g_dump_palette_index=0;return;}
  if(!a.sprite){
    Pic pic={};if(pic_from_mem(&pic,data,n)){
      // Track HUD colors 251..255 are supplied by PAL_RACE, just like track_load.
      dump_add_palette(a.key,pic.pal);
      if(a.cd && str_icontains(a.key,"CD/TRACK") && model_palette_ensure()){u8 hud[768];mem_cpy(hud,pic.pal,768);mem_cpy(hud+251*3,g_model_reserved+3*3,15);dump_add_palette(a.key,hud);}
      if((str_icontains(a.key,"LOCAL/CARS/") || str_icontains(a.key,"LOCAL/OBJS/")) && model_palette_ensure())model_palette_resolve(pic.pal,g_model_reserved);
      u8 *rgba=(u8 *)sys_alloc((size_t)pic.w*pic.h*4);int font=str_icontains(a.key,"LOCAL/TEXTE/");int fonte=0;if(font){int zero=0,separator=0;for(int i=0;i<pic.w*pic.h;i++){if(!pic.px[i])zero++;if(pic.px[i]==255)separator++;}fonte=zero>separator;}
      for(int i=0;i<pic.w*pic.h;i++){int c=pic.px[i];mem_cpy(rgba+i*4,pic.pal+c*3,3);if(font){int ink=fonte?c==250:c==254,shade=!fonte && c==1;if(ink||shade)rgba[i*4]=rgba[i*4+1]=rgba[i*4+2]=ink?255:153;}rgba[i*4+3]=font?(fonte?c==250:(c==254||c==1))?255:0:255;}
      char key[512];str_cpy(key,a.key,sizeof key);str_cat(key,".png",sizeof key);
      u8 *legacy=0;
      if(str_icontains(a.key,"LOCAL/CARS/") || str_icontains(a.key,"LOCAL/OBJS/")){legacy=(u8 *)sys_alloc((size_t)pic.w*pic.h*4);mem_cpy(legacy,rgba,(size_t)pic.w*pic.h*4);for(int i=0;i<pic.w*pic.h;i++)if(pic.px[i]>=248)mem_cpy(legacy+i*4,g_model_reserved+(pic.px[i]-248)*3,3);}
      if(texture_dump_once(key)){if(texture_dump_rgba(key,rgba,pic.w,pic.h,"RGBA8 straight alpha. Original size is the PNG size. Keep the source-relative filename.\n",legacy))g_dump_pngs++;else g_dump_errors++;}sys_free(legacy);
      sys_free(rgba);pic_free(&pic);
    }else g_dump_errors++;
    g_dump_index++;g_dump_palette_index=0;
  }else if(a.sprite==2){
    char text[8192]="Original M3D material attributes (geometry/material rules are not stored in PNG).\nMaterial types: 0 flat color, 1 textured, 3 additive, 4 translucent.\nPNG alpha is straight coverage; it multiplies textured/additive material coverage.\nReplacement PNG dimensions do not change original UV normalization.\n\n";
    char directory[512];dump_directory_name(directory,a.key);
    if(n>=0x60 && data[0]=='7' && data[1]=='0' && data[2]=='B' && data[3]=='D')for(size_t o=0x40,mat=0;o+30<=n && mat<24;o+=30,mat++){
      if(rd32(data+o)==0xffffffffu)break;char name[16];mem_cpy(name,data+o,15);name[15]=0;str_cat(text,"Material: ",sizeof text);str_cat(text,name,sizeof text);str_cat(text,"; type: ",sizeof text);str_int(text,sizeof text,rd16(data+o+26));
      if(!name[0] || !rd16(data+o+26)){str_cat(text,"; PNG: none (untextured material)\n",sizeof text);continue;}
      for(char *p=name;*p;p++)if(*p=='.'){*p=0;break;}str_cat(text,"; PNG: ",sizeof text);str_cat(text,directory,sizeof text);str_cat(text,"/",sizeof text);str_cat(text,name,sizeof text);str_cat(text,".HLZ.png\n",sizeof text);
    }
    char path[600]="revived_config/texture_dumps/";str_cat(path,a.key,sizeof path);str_cat(path,".txt",sizeof path);if(!sys_write_file(path,text,str_len(text)))g_dump_errors++;texture_log("Exported model material attributes",path);sys_free(data);g_dump_index++;g_dump_palette_index=0;
  }else{
    char directory[512];dump_directory_name(directory,a.key);
    while(g_dump_palette_index<g_dump_palettes.n && str_icmp(directory,g_dump_palettes.p[g_dump_palette_index].directory))g_dump_palette_index++;
    if(g_dump_palette_index<g_dump_palettes.n){
      SprFile sprites={};if(spr_from_mem(&sprites,data,n)){for(int i=0;i<sprites.n;i++){str_cpy(sprites.f[i].texture_source,a.key,512);sprites.f[i].frame=i;texture_dump_sprite(sprites.f[i],g_dump_palettes.p[g_dump_palette_index].pal);}spr_free(&sprites);}else g_dump_errors++;
      g_dump_palette_index++;
    }else{sys_free(data);g_dump_index++;g_dump_palette_index=0;}
  }
  char status[300]="Catalog assets: ";str_int(status,sizeof status,g_dump_index);str_cat(status," / ",sizeof status);str_int(status,sizeof status,g_dump_assets.n);str_cat(status,"\nPNG exports: ",sizeof status);str_int(status,sizeof status,g_dump_pngs);str_cat(status,"\nErrors: ",sizeof status);str_int(status,sizeof status,g_dump_errors);str_cat(status,"\n",sizeof status);sys_write_file("revived_config/texture_dumps/status.txt",status,str_len(status));
}
