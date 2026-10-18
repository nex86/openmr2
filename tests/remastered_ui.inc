static void test_remastered_ui(){
  for(int f=0;f<4;f++){G.video_upscale=1;G.video_filter=f;texture_options_apply();check(g_video_upscale&&g_video_filter==f,"all four GPU video filter choices apply in Revived");setup_save();G.video_filter=99;setup_load();check(G.video_filter==f,"video filter selection survives config reload");graphics_mode_set(1);check(!g_video_upscale&&G.video_filter==f&&G.video_upscale&&graphics_option_locked(13)&&graphics_option_locked(14),"Retro suppresses video filters, locks controls and retains the Revived preference");check(video_fsr_texture(123,1280,960)==123,"Retro bypasses the GPU video preprocessing path");graphics_mode_set(0);}
  G.video_upscale=0;texture_options_apply();
  g_revived_ui=1;g_ui_assets_allowed=1;g_custom_textures=0;
  Sprite tile={};tile.x=73;tile.y=195;tile.w=49;tile.h=41;G.gfx_mode=0;
  for(int i=0;i<11;i++){int x,y,w,h;shop_icon_rect(i,tile,&x,&y,&w,&h);check(x==17+i*56&&y==420&&w==50&&h==41,"Revived shop tiles share exact dimensions, baseline, gaps and hit rectangles");}
  G.gfx_mode=1;{int x,y,w,h;shop_icon_rect(3,tile,&x,&y,&w,&h);check(x==tile.x&&y==tile.y+224&&w==tile.w&&h==tile.h,"Retro shop retains original sprite positions and dimensions");}G.gfx_mode=0;
  int seen[REMASTERED_UI_ASSETS]={},assets=0;
  for(unsigned i=0;i<sizeof REMASTERED_UI_ENTRIES/sizeof *REMASTERED_UI_ENTRIES;i++){
    const RemasteredUIEntry &e=REMASTERED_UI_ENTRIES[i];TextureImage *t=texture_for_canvas(e.key);
    check(t&&t->pixels&&t->w>=64&&t->h>=64,"every built-in sprite decodes through its exact palette-qualified runtime identity");
    if(!seen[e.asset]++){assets++;int partial=0,opaque=0;if(t)for(int n=0;n<t->w*t->h;n++){partial+=t->pixels[n*4+3]>0&&t->pixels[n*4+3]<255;opaque+=t->pixels[n*4+3]>200;}check(partial&&opaque,"every remastered icon preserves smooth alpha and visible detail");}
    if(e.asset>=26&&e.asset<=36)check(t&&t->w==400&&t->h==328,"all eleven shop tile PNGs have the same high-resolution canvas dimensions");
  }
  check(assets==49,"49 HUD/weapon/shop-strip/cursor assets enter the built-in remaster");
  for(int i=48;i<=66;i++)check(!seen[i],"background-integrated buttons remain outside the remaster registry");
  const char *key=REMASTERED_UI_ENTRIES[0].key;graphics_mode_set(1);check(!g_revived_ui&&!texture_for_canvas(key),"Retro bypasses embedded assets even after the Revived cache is populated");graphics_mode_set(0);check(g_revived_ui&&texture_for_canvas(key),"switching back to Revived restores remastered UI automatically");
  char path[600]="revived_config/texture_replacements/";str_cat(path,key,sizeof path);u8 pixel[4]={12,234,56,255};png_write(path,pixel,1,1);g_custom_textures=1;texture_cache_clear();TextureImage *custom=texture_for_canvas(key);check(custom&&custom->w==1&&custom->pixels[1]==234,"user replacement PNG takes priority over the built-in AI remaster");texture_cache_clear();sys_delete_file(path);g_custom_textures=0;
  int state=G.state;u8 saved[768];mem_cpy(saved,G.bg.pal,768);mem_zero(G.bg.pal,768);G.state=GS_OPTIONS;G.bg.pal[0]=252;G.bg.pal[1]=128;G.bg.pal[2]=3;u8 colors[1024];retro_palette_colors(colors);check(colors[0]==255&&colors[1]==129&&!colors[2]&&colors[3]==255,"Retro palette uses 6-bit DAC precision with full white and no RGB332 substitute");mem_cpy(G.bg.pal,saved,768);G.state=state;
}
