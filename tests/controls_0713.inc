static void test_controls_0713(){
  int language=gf_language;gf_language=2;capture_notice("Recording started");check(g_shortcut_right&&!str_icmp(g_shortcut_notice,"Aufnahme gestartet")&&g_shortcut_until>sys_time(),"capture start notification is localized, transient and top-right");capture_notice("Recording stopped");check(g_shortcut_right&&!str_icmp(g_shortcut_notice,"Aufnahme gestoppt"),"recording stop notification uses the top-right capture channel");capture_notice("Screenshot saved");check(g_shortcut_right&&!str_icmp(g_shortcut_notice,"Screenshot erstellt"),"screenshot notification uses the localized top-right capture channel");gf_language=language;
  g_game_ready=1;A.have_mexe=1;A.mode=MODE_TRACK;G.state=GS_MENU;G.dbg_console=0;G.effects_muted=0;graphics_mode_set(0);
  auto key=[](int code){Input input={};input.pressed[code]=1;global_shortcuts(640,480,&input);};
  G.opts_gfx=0;key(K_F2);check(G.opts_gfx==1&&!g_smooth_filter,"F2 switches the common background/model filter off");key(K_F2);check(!G.opts_gfx&&g_smooth_filter,"F2 restores filtered backgrounds and models");
  G.dbg_fpsmode=0;key(K_F3);check(g_out_mode==1,"F3 enables 60fps through the normal timing preferences");
  G.custom_textures=0;key(K_F4);check(g_custom_textures,"F4 enables exact-path replacements");
  G.video_upscale=0;G.video_filter=0;key(K_F7);key(K_F8);check(g_video_upscale&&g_video_filter==1,"F7 toggles upscaling and F8 cycles its filter");
  G.vehicle_lights=1;G.damage_effects=0;G.real_time_shadows=1;debug_apply();key(K_F9);check(!g_vehicle_lights&&!g_realtime_shadows&&!g_modern_explosions&&!g_missile_effects&&!g_enhanced_oil&&!g_damage_effects,"F9 disables every Revived effect together");key(K_F9);check(g_vehicle_lights&&g_realtime_shadows&&!g_damage_effects,"F9 restores individual effect preferences without enabling a previously disabled effect");
  key(K_F1);check(G.gfx_mode==1&&!g_video_upscale&&!g_custom_textures&&!g_smooth_filter&&g_out_mode==0,"F1 enters Retro and keeps its graphics/timing locks");
  int filter=G.gfx_output_filter,scale=G.gfx_scale;key(K_F5);key(K_F6);check(G.gfx_output_filter==(filter+1)%6&&G.gfx_scale==scale%4+1,"F5 and F6 cycle Retro output filters/scales");
  key(K_F2);key(K_F3);key(K_F4);key(K_F7);key(K_F8);check(!g_smooth_filter&&g_out_mode==0&&!g_video_upscale&&!g_custom_textures,"modern shortcuts cannot unlock prohibited Retro graphics");key(K_F1);check(G.gfx_mode==0&&g_video_upscale&&g_custom_textures&&g_out_mode==1,"F1 restores the saved Revived configuration");
  G.state=GS_RACE;G.t=17.25;RC.paused=0;RC.active=1;G.sel=3;int frame=TK.vid.decoded_frame;double elapsed=RC.t;
  RC.phase=PH_RACE;player().fixed_ready=1;player().fixed.slip_frames=3;player().out=player().finished=0;g_engine_suppressed=0;g_audio_percent[AUDIO_SOUNDS]=100;
  skid_update();int skid_handle=RC.skid_voice;Voice *skid=snd_voice(skid_handle);double skid_pos=skid?skid->pos:0;
  check(skid&&skid->snd==&SFX[S_DERAP],"F10 test begins with the actual original tire-squeal loop active");
  int saved_menu_volume=g_audio_percent[AUDIO_MENU_MUSIC];g_audio_percent[AUDIO_MENU_MUSIC]=0;
  key(K_F10);check(G.state==GS_OPTIONS&&RC.paused&&g_options_return==GS_RACE,"F10 opens the full settings menu and pauses a race");
  int16_t options_pcm[128];app_audio_fill(options_pcm,64);int silent=1;for(int i=0;i<128;i++)if(options_pcm[i])silent=0;
  check(!snd_voice(skid_handle)&&silent,"F10 suspends tire squeal before the race's ordinary pause branch and mixes no paused-race PCM");
  Input enter={};enter.pressed[K_ENTER]=1;G.sel=13;G.click=0;screen_options(&enter);
  check(snd_voice(skid_handle)&&snd_voice(skid_handle)->pos==skid_pos,"F10 Apply resumes the same tire-squeal phase without restarting its sample");
  skid_stop();g_audio_percent[AUDIO_MENU_MUSIC]=saved_menu_volume;
  check(G.state==GS_RACE&&!RC.paused&&G.t==17.25&&G.sel==3&&g_options_return<0,"Apply resumes the same race and original UI state");check(RC.t==elapsed&&TK.vid.decoded_frame==frame,"settings do not reset or advance the race/video clock");
  graphics_mode_set(1);key(K_F10);check(G.gfx_mode==1&&G.state==GS_OPTIONS,"F10 opens the Retro options variant in Retro");G.sel=12; // Retro Apply is the final item; use the actual return function below.
  screen_options(&enter);check(G.state==GS_RACE&&!RC.paused,"Retro Apply returns to the same race");
  G.vehicle_lights=G.damage_effects=G.modern_explosions=G.real_time_shadows=G.enhanced_oil=G.missile_effects=1;G.effects_muted=0;debug_apply();
  check(!g_vehicle_lights&&!g_damage_effects&&!g_modern_explosions&&!g_realtime_shadows&&!g_enhanced_oil&&!g_missile_effects,"Retro unconditionally bypasses all six Revived effect groups");
  check(G.gfx_mode==1&&G.opts_gfx==1&&!G.gfx_msaa&&!G.gfx_fxaa&&!G.dbg_fpsmode&&!g_revived_text&&!g_revived_ui&&!g_custom_textures&&!g_video_upscale,"strict Retro preserves every original presentation lock");
  G.state=GS_EXTRAS;A.mode=MODE_VIEWER;key(K_F10);check(A.mode==MODE_TRACK&&G.state==GS_OPTIONS,"F10 also works from the model viewer");options_close();check(A.mode==MODE_VIEWER&&G.state==GS_EXTRAS,"Apply restores a non-game viewer without restarting it");A.mode=MODE_TRACK;G.state=GS_MENU;RC.active=0;graphics_mode_set(0);
  extras_build();check(g_extra_clips.n>20,"Extras enumerates the actual game's cinematic catalog");int tracks=0,intros=0;for(int i=0;i<g_extra_clips.n;i++)for(int t=0;t<6;t++){char path[128]="";str_cpy(path,TRACKS[t].cd,sizeof path);str_cat(path,".UBB",sizeof path);if(!str_icmp(path,g_extra_clips.p[i].path))tracks++;if(!str_icmp(TRACKS[t].intro_clip,g_extra_clips.p[i].path))intros++;}check(!tracks&&intros==6,"Extras excludes all race footage and includes all six track introductions");
  g_audio_percent[AUDIO_MENU_MUSIC]=0;g_audio_percent[AUDIO_RACE_MUSIC]=37;G.state=GS_MENU;setup_apply();menu_music_stop();G.state=GS_TRACKINFO;loading_music_start();check(G.menu_music.pcm&&MX.v[0].active&&MX.v[0].snd==&G.menu_music&&MX.v[0].bus==AUDIO_RACE_MUSIC,"loading music loads and plays on the race bus when menu music is zero");
  g_audio_percent[AUDIO_RACE_MUSIC]=0;loading_music_start();check(!MX.v[0].active,"zero race volume disables loading music");
  G.state=GS_MENU;menu_music_stop();for(int i=0;i<AUDIO_COUNT;i++)g_audio_percent[i]=100;setup_apply();
}
