// Only cinematic directories: race/track UBBs are never enumerated here.
struct ExtraClip {char path[128];};
static Arr<ExtraClip> g_extra_clips={};static int g_extra_top,g_extra_built;
static void extra_clip_found(void *dir,const char *name,uint32_t){
  size_t n=str_len(name);if(n<5||str_icmp(name+n-4,".UBB"))return;
  ExtraClip e={};str_cpy(e.path,(const char *)dir,sizeof e.path);str_cat(e.path,"/",sizeof e.path);str_cat(e.path,name,sizeof e.path);
  for(int i=0;i<g_extra_clips.n;i++)if(!str_icmp(e.path,g_extra_clips.p[i].path))return;
  g_extra_clips.push(e);
}
static void extra_directory(void *dir,const char *name,int isdir){
  if(name[0]=='.')return;char path[256]="";str_cpy(path,(const char *)dir,sizeof path);if(path[0])str_cat(path,"/",sizeof path);str_cat(path,name,sizeof path);
  if(isdir){iso_list_all(path,extra_directory,path);return;}
  for(int i=0;i<6;i++){char track[128]="";str_cpy(track,TRACKS[i].cd,sizeof track);str_cat(track,".UBB",sizeof track);if(!str_icmp(path,track))return;}
  extra_clip_found(dir,name,0);
}
static void extras_build(){
  if(g_extra_built)return;g_extra_built=1;
  iso_list_all("",extra_directory,(void *)"");
  for(int i=1;i<g_extra_clips.n;i++){ExtraClip e=g_extra_clips.p[i];int j=i;while(j&&str_icmp(g_extra_clips.p[j-1].path,e.path)>0){g_extra_clips.p[j]=g_extra_clips.p[j-1];j--;}g_extra_clips.p[j]=e;}
}
static void screen_extras(const Input *in){
  extras_build();cv_pic(G.bg,0,0,1);cv_text(g_font_h14,320,6,"Extras",ORANGE,1,1);
  const int visible=11,n=g_extra_clips.n+2;int hover=-1;
  if(G.sel<0||G.sel>=n)G.sel=0;
  if(in->pressed[K_DOWN])G.sel=(G.sel+1)%n;if(in->pressed[K_UP])G.sel=(G.sel+n-1)%n;
  if(in->pressed[K_PGDN])G.sel=G.sel+visible<n?G.sel+visible:n-1;if(in->pressed[K_PGUP])G.sel=G.sel>visible?G.sel-visible:0;
  if(in->wheel){G.sel+=(in->wheel>0?-3:3);G.sel=G.sel<0?0:G.sel>=n?n-1:G.sel;}
  if(G.sel<g_extra_top)g_extra_top=G.sel;if(G.sel>=g_extra_top+visible)g_extra_top=G.sel-visible+1;
  if(g_extra_top>n-visible)g_extra_top=n>visible?n-visible:0;
  for(int row=0;row<visible&&g_extra_top+row<n;row++){
    int i=g_extra_top+row,y=54+row*31;
    const char *label=i==0?"Model viewer":i==n-1?"Zur\xfc" "ck":g_extra_clips.p[i-1].path;
    if(G.exit_spr.n)cv_spr(spr_at(G.exit_spr.f[0],35,y-2),G.bg.pal,0,0,1);
    cv_text(g_font_h10,91,y+2,label,i==G.sel?WHITE:BEIGE,0,1);
    if(hit(32,y-3,550,29))hover=i;
  }
  if(n>visible){int h=341*visible/n,y=54+(341-h)*g_extra_top/(n-visible);cv_rect(605,54,7,341,GREY);cv_rect(604,y,9,h,ORANGE);}
  cv_text(g_font_small,320,414,"Extras scroll hint",GREY,1,1);
  cv_text(g_font_h10,600,451,"<< Zur\xfc" "ck",BEIGE,2,1);if(hit(440,438,180,32))hover=n-1;
  if(hover>=0&&(in->mouse_dx||in->mouse_dy))G.sel=hover;
  int act=G.click?hover:in->pressed[K_ENTER]||in->pressed[K_SPACE]?G.sel:-1;if(in->pressed[K_ESC])act=n-1;
  if(act==n-1){go(GS_DEBUG);return;}if(act==0){extras_open_model();return;}
  if(act>0){extras_open_video(g_extra_clips.p[act-1].path);return;}draw_cursor();
}
