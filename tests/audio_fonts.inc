// Integrated with the real mixer/loaders and input handlers; no user files are used.
static void test_audio_fonts(){
  static const char legacy_audio[]="sound_effects = 0\nrace_music = 1\nmenu_music = 0\nmenu_sounds = 1\n";
  sys_write_file("revived_config/openmr2_setup.cfg",legacy_audio,sizeof legacy_audio-1);setup_load();
  check(!g_audio_percent[0]&&!g_audio_percent[1]&&g_audio_percent[2]==100&&!g_audio_percent[3]&&g_audio_percent[4]==100,"legacy audio toggles migrate to five independent percentages");
  static const char new_audio[]="sounds_volume = 37\nengines_volume = 82\nrace_music_volume = -5\nmenu_music_volume = 0\nmenu_sounds_volume = 150\nsound_effects = 0\nrace_music = 1\n";
  sys_write_file("revived_config/openmr2_setup.cfg",new_audio,sizeof new_audio-1);setup_load();setup_save();setup_load();
  check(g_audio_percent[0]==37&&g_audio_percent[1]==82&&!g_audio_percent[2]&&!g_audio_percent[3]&&g_audio_percent[4]==100,"percentage values win over old flags, clamp and survive saving");
  for(int b=0;b<AUDIO_COUNT;b++)g_audio_percent[b]=0;setup_apply();
  int serial=MX.serial;sfx_load_all();menu_music_start();loading_music_start();race_music_start();msfx(S_MENU2,1);sfx(S_CHOC1,1);
  check(engine_play(S_ENGINE,1,1)<0 && MX.serial==serial,"zero-volume categories do not allocate playback voices");
  int loaded=G.menu_music.pcm!=0||TK.music.pcm!=0;for(int i=0;i<S_COUNT;i++)loaded|=SFX[i].pcm!=0;
  check(!loaded,"zero-volume categories retain no decoded music or sound effects");
  for(int i=0;i<S_COUNT;i++)check(!sfx_sample(i,sfx_bus(i)),"disabled audio cannot pass its file-loading gate");
  size_t video_size=0;u8 *video_data=iso_load("BOYLE/BOYLE01.UBB",&video_size);
  check(video_data!=0,"original speech video is available for muted decoder check");
  if(video_data){
    Video muted={},audible={};u8 *copy=(u8 *)sys_alloc(video_size);mem_cpy(copy,video_data,video_size);
    int a=video_open(&muted,video_data,video_size,400,0),b=video_open(&audible,copy,video_size,400,1);
    check(a&&b&&!muted.pcm&&!muted.pcm_frames&&audible.pcm&&audible.pcm_frames>0,"muted videos skip PCM allocation and audio decoding");
    check(muted.nframes==audible.nframes && muted.fps_exact==audible.fps_exact,"muted speech videos retain exact frame rate and duration");
    video_close(&muted);video_close(&audible);
  }
  // Exact PCM evidence: each of the five bus gains is independent and applied live.
  int16_t sample[4]={10000,10000,10000,10000},out[2];Sound probe={sample,4,1};
  for(int b=0;b<AUDIO_COUNT;b++){
    mem_zero(MX.v,sizeof MX.v);MX.muted=0;
    Voice &v=MX.v[2];v.snd=&probe;v.active=1;v.rate=1;v.vol=1;v.loop=1;v.bus=b;
    g_audio_percent[b]=25;app_audio_fill(out,1);check(out[0]==2500&&out[1]==2500,"25 percent bus produces exact quarter-amplitude PCM");
    g_audio_percent[b]=70;app_audio_fill(out,1);check(out[0]==7000&&out[1]==7000,"active voice volume changes without restarting it");
    g_audio_percent[b]=0;app_audio_fill(out,1);check(!out[0]&&!out[1],"zero percent bus produces silence");
  }
  mem_zero(MX.v,sizeof MX.v);g_audio_percent[AUDIO_ENGINES]=100;
  int engine=engine_play(S_ENGINE,1,1);check(snd_voice(engine)&&SFX[S_ENGINE].pcm,"engine can load and play while sounds remain off");
  g_audio_percent[AUDIO_MENU_SOUNDS]=100;msfx(S_MENU2,1);
  check(MX.v[2].active&&MX.v[3].active&&MX.v[2].bus!=MX.v[3].bus,"menu sounds and engines have separate active buses");
  g_audio_percent[AUDIO_ENGINES]=0;setup_apply();check(!snd_voice(engine)&&!SFX[S_ENGINE].pcm&&MX.v[3].active,"disabling engines frees them while menu sound survives");
  for(int b=0;b<AUDIO_COUNT;b++)g_audio_percent[b]=0;setup_apply();
  pic_load(&G.bg,"DATA640/OPTION3.HLZ");
  font_load(&g_font_h10,"TEXTE/HNDEL10.HLZ");font_load(&g_font_small,"TEXTE/CLR6_13.HLZ");
  G.gfx_mode=0;g_revived_text=1;g_revived_font_tex=123;G.state=GS_OPTIONS;G.sel=6;G.click=0;Input key={};key.pressed[K_RIGHT]=1;
  screen_options(&key);check(g_audio_percent[1]==5&&G.sel==6,"right arrow changes the selected engine slider without moving focus");
  key={};key.pressed[K_LEFT]=1;screen_options(&key);check(!g_audio_percent[1],"left arrow returns slider to zero");
  G.mx=475;G.my=197;G.click=1;key={};key.lmb=1;screen_options(&key);check(g_audio_percent[2]==50,"clicking the race music slider midpoint selects 50 percent");
  G.click=0;G.mx=610;screen_options(&key);check(g_audio_percent[2]==100,"held mouse drags to 100 percent outside the original click point");
  key={};screen_options(&key);check(g_audio_drag<0,"releasing the mouse ends slider dragging");
  G.mx=340;G.my=197;G.click=1;key.lmb=1;screen_options(&key);check(!g_audio_percent[2],"left endpoint disables race music completely");
  G.click=0;key={};cv_clear(0);screen_options(&key);
  int background_ok=0;
  if(g_smooth_filter){for(int i=0;i<g_original_pictures.n;i++){TextureImage &p=g_original_pictures.p[i];if(!str_icmp(p.key,G.bg.texture_key)){uint32_t pixel=0;mem_cpy(&pixel,p.pixels+(35*p.w+15)*4,4);background_ok=pixel==pal_rgba(G.bg.pal,G.bg.px[35*G.bg.w+15]);}}for(int i=0;i<g_canvas_slice_used;i++)if(g_canvas_slices.p[i].pixels[35*CW+15]>>24)background_ok=0;}
  else background_ok=g_canvas_slice_used&&g_canvas_slices.p[0].pixels[35*CW+15]==pal_rgba(G.bg.pal,G.bg.px[35*G.bg.w+15]);
  check(background_ok,"options retain the original unmodified background without dark panels in both canvas paths");
  for(int language=0;language<3;language++)for(int retro=0;retro<2;retro++){
    gf_language=language;g_revived_text=!retro;
    check(cv_text_w(g_font_h10,"Advanced graphics",1)<=220,"advanced graphics fits left column in every language and mode");
    check(cv_text_w(g_font_h10,"Lance Boyle videos",1)<=248,"video checkbox fits left column in every language and mode");
    check(cv_text_w(g_font_h10,"Controller belegen",1)<=240,"controller action fits left column in every language and mode");
    check(cv_text_w(g_font_small,"Audio off hint",1)<=280,"audio hint fits left column in every language and mode");
  }
  g_revived_text=1;g_custom_textures=0;texture_canvas_reset();cv_clear(0);
  cv_text_raw(g_font_small,20,40,"HUD \xc4\xd6\xdc \xe4\xf6\xfc \xdf \xc9\xe8\xe7",0xffffffffu,0,1);
  check(g_texture_layers.n>20&&g_texture_layers.p[0].sdf,"Revived game text uses scalable glyph layers including accented characters");
  graphics_mode_set(1);check(!g_revived_text,"Retro mode switches off scalable text globally");texture_canvas_reset();cv_clear(0);
  cv_text_raw(g_font_small,20,40,"Retro",0xffffffffu,0,1);int ink=0;for(int i=0;i<CW*CH;i++)if(g_cv[i])ink++;
  check(ink>0&&!g_texture_layers.n,"Retro draws only the original bitmap font");graphics_mode_set(0);check(g_revived_text,"returning to Revived restores scalable text");
  gf_language=2;G.click=0;g_audio_drag=-1;
  for(int b=0;b<AUDIO_COUNT;b++)g_audio_percent[b]=100;setup_apply();
}
