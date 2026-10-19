static Pic g_options_background;static double g_options_time;static int g_options_mode,g_options_selection,g_options_voice_active[24];
static Voice g_options_music,g_options_voice;
static Video g_options_clip;static Sound g_options_fmv_sound;static int g_options_clip_ok,g_options_credits,g_options_clip_next,g_options_clip_count;static double g_options_clip_time;static char g_options_queue[4][40];
static void options_close(){
  if(g_options_return<0){go(GS_MENU);return;}
  clip_stop();G.clip=g_options_clip;mem_zero(&g_options_clip,sizeof g_options_clip);G.clip_ok=g_options_clip_ok;G.clip_t=g_options_clip_time;G.clip_next=g_options_clip_next;G.clip_nq=g_options_clip_count;mem_cpy(G.clip_queue,g_options_queue,sizeof g_options_queue);g_credits_on=g_options_credits;g_fmv_snd=g_options_fmv_sound;
  music_stop();pic_free(&G.bg);G.bg=g_options_background;mem_zero(&g_options_background,sizeof g_options_background);
  G.state=g_options_return;G.t=g_options_time;G.sel=g_options_selection;A.mode=g_options_mode;RC.paused=g_options_pause;g_options_return=-1;g_gray=0;
  for(int i=2;i<NVOICES;i++)if(g_options_voice_active[i]&&MX.v[i].snd&&MX.v[i].snd->pcm&&audio_enabled(MX.v[i].bus))MX.v[i].active=1;
  if(g_options_music.snd&&g_options_music.snd->pcm&&audio_enabled(g_options_music.bus))MX.v[0]=g_options_music;
  if(g_options_voice.snd&&g_options_voice.snd->pcm&&audio_enabled(g_options_voice.bus))MX.v[1]=g_options_voice;
  G.menu_music_on=MX.v[0].active&&MX.v[0].snd==&G.menu_music;
  if(G.state==GS_TRACKINFO)loading_music_start();
  else if(G.state==GS_RACE){g_frame_sys=sys_time();if(!RC.paused&&!MX.v[0].active)race_music_start();}
  else if(G.state!=GS_CLIP&&A.mode==MODE_TRACK&&!G.menu_music_on)menu_music_start();
}
static void options_open(){
  if(g_options_return>=0){options_close();return;}
  g_options_return=G.state;g_options_time=G.t;g_options_selection=G.sel;g_options_pause=RC.paused;g_options_mode=A.mode;g_options_background=G.bg;mem_zero(&G.bg,sizeof G.bg);
  g_options_clip=G.clip;mem_zero(&G.clip,sizeof G.clip);g_options_clip_ok=G.clip_ok;g_options_clip_time=G.clip_t;g_options_clip_next=G.clip_next;g_options_clip_count=G.clip_nq;mem_cpy(g_options_queue,G.clip_queue,sizeof g_options_queue);g_options_credits=g_credits_on;g_options_fmv_sound=g_fmv_snd;g_credits_on=G.clip_ok=G.clip_nq=0;
  g_options_music=MX.v[0];g_options_voice=MX.v[1];for(int i=0;i<NVOICES;i++){g_options_voice_active[i]=MX.v[i].active;MX.v[i].active=0;}
  RC.paused=1;G.menu_music_on=0;A.mode=MODE_TRACK;G.state=GS_OPTIONS;G.t=0;G.sel=0;game_set_bg("DATA640/OPTION3.HLZ");menu_music_start();g_gray=0;
}
static void extras_open_model(){mode_set(MODE_VIEWER);A.show_help=1;}
static void extras_open_video(const char *path){
  G.clip_nq=0;clip_queue(path);G.clip_next=GS_EXTRAS;menu_music_stop();if(clip_start_next()){G.state=GS_CLIP;g_credits_on=0;}else go(GS_EXTRAS);
}
static void global_shortcuts(int W,int H,const Input *in){
  if(!g_game_ready)return;
  if(in->pressed[K_F1]){graphics_mode_set(!G.gfx_mode);shortcut_notice(G.gfx_mode?"Retro Mode":"Revived Mode");}
  if(in->pressed[K_F2]){if(!G.gfx_mode){G.opts_gfx^=1;setup_apply();shortcut_notice(G.opts_gfx?"Texture filtering off":"Texture filtering on (including backgrounds)");}else shortcut_notice("Texture filtering is unavailable in Retro Mode.");}
  if(in->pressed[K_F3]){if(!G.gfx_mode){G.dbg_fpsmode^=1;debug_apply();shortcut_notice(G.dbg_fpsmode?"60fps":"Original framerate");}else shortcut_notice("Retro Mode uses the original framerate.");}
  if(in->pressed[K_F4]){if(!G.gfx_mode){G.custom_textures^=1;texture_options_apply();shortcut_notice(G.custom_textures?"Custom textures on":"Custom textures off");}else shortcut_notice("Custom textures are unavailable in Retro Mode.");}
  if(in->pressed[K_F5]){if(G.gfx_mode){G.gfx_output_filter=(G.gfx_output_filter+1)%6;shortcut_notice(RETRO_FILTER_NAMES[G.gfx_output_filter]);}else shortcut_notice("Output filters are available in Retro Mode.");}
  if(in->pressed[K_F6]){if(G.gfx_mode){G.gfx_scale=G.gfx_scale%4+1;char label[64]="Retro scaling: ";str_int(label,sizeof label,G.gfx_scale);str_cat(label,"x",sizeof label);shortcut_notice(label);}else shortcut_notice("Retro scaling is available in Retro Mode.");}
  if(in->pressed[K_F7]){if(!G.gfx_mode){G.video_upscale^=1;texture_options_apply();shortcut_notice(G.video_upscale?"Experimental video upscaling on":"Video upscaling off");}else shortcut_notice("Video upscaling is unavailable in Retro Mode.");}
  if(in->pressed[K_F8]){if(!G.gfx_mode){G.video_filter=(G.video_filter+1)%VIDEO_FILTER_COUNT;texture_options_apply();shortcut_notice(VIDEO_FILTER_NAMES[G.video_filter]);}else shortcut_notice("Video filters are unavailable in Retro Mode.");}
  if(in->pressed[K_F9]&&G.gfx_mode==0){G.effects_muted^=1;debug_apply();shortcut_notice(G.effects_muted?"All Revived effects off":"Revived effects restored");}
  if(in->pressed[K_F10])options_open();
  if(in->pressed[K_F11])capture_toggle(W,H);
  int keys[]={K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9};for(int i=0;i<9;i++)if(in->pressed[keys[i]]){setup_save();break;}
  g_tex_nearest=G.gfx_mode==1;g_smooth_filter=!G.gfx_mode&&G.opts_gfx==0;
  R.samples=G.gfx_mode?0:(G.gfx_msaa>R.max_samples?R.max_samples:G.gfx_msaa);if(R.samples<2)R.samples=0;
  // Indexed DOS sources have no driver's RGB framebuffer dithering stage.
  if(G.gfx_mode)glDisable(0x0BD0);else glEnable(0x0BD0); // GL_DITHER
}
