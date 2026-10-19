static void test_engine_retro(){
  static const int profiles[14]={0,1,2,3,1,2,3,1,2,3,1,2,3,1};
  static const int sample_ids[4]={S_ENGINE,S_ENGINE2,S_ENGINE3,S_ENGINE2},levels[4]={20,20,18,18};
  const char *fixtures[]={"../../../Megarace 2 English","../../../Megarace 2 French","../../../Megarace 2 German"};
  const char *files[]={"TURBINE","M_2","MOT3","M_2"};
  for(int language=0;language<3;language++){
    g_engine_suppressed=0;gf_language=language;char path[512];str_cpy(path,fixtures[language],sizeof path);str_cat(path,"/M.EXE",sizeof path);
    check(mexe_load(path),"original edition loads for independent engine-table checks");
    str_cpy(g_gamedir,fixtures[language],sizeof g_gamedir);g_audio_percent[AUDIO_ENGINES]=100;setup_apply();
    for(int car=0;car<14;car++){
      int level=0,profile=mexe_engine_profile(car,&level);
      check(profile==profiles[car] && level==levels[profile],"every car resolves its original sample profile and base gain in each edition");
      check(car_engine_id(car)==sample_ids[profiles[car]],"port engine selection follows the original table for all 14 cars");
      check(!str_icmp(SFX_FILES[car_engine_id(car)],files[profile]),"car uses the original WAV filename rather than generic engine placeholders");
      int h=car_engine_play(car,0.5f,1);Voice *v=snd_voice(h);
      check(v&&v->snd==&SFX[sample_ids[profile]]&&v->bus==AUDIO_ENGINES&&v->snd->pcm,"selected original car sample actually starts on the independent engine bus");snd_stop(h);
    }
    check(mexe_engine_frequency(0)==14000 && mexe_engine_frequency(100)==16600,"original engine pitch curve starts at 14000 Hz and follows original speed mapping");
    check(mexe_engine_frequency(299)==21774&&mexe_engine_frequency(300)==18000&&mexe_engine_frequency(599)==21887&&mexe_engine_frequency(600)==18850,"both original engine gear drops are retained");
    check(mexe_engine_frequency(-200)==mexe_engine_frequency(100)&&mexe_engine_frequency(1000)==22050&&mexe_engine_frequency(2000)==22050,"reverse uses half speed magnitude and overrun is bounded");
    int handle=car_engine_play(0,0.5f,1),old=handle;car_engine_select(&handle,2,0.5f,1);
    check(!snd_voice(old)&&snd_voice(handle)&&snd_voice(handle)->snd==&SFX[S_ENGINE3],"changing audible opponent retires old sample and starts its correct engine");
    RC.eng_voice=RC.eng2_voice=RC.scrape_voice=-1;race_end_sounds();check(!snd_voice(handle),"orphan engine loops for every profile stop on race teardown");
  }
  int unused=0;check(mexe_engine_profile(-1,&unused)<0&&mexe_engine_profile(14,&unused)<0,"invalid car IDs cannot index executable engine data");
  gf_language=2;font_load(&g_font_h14,"TEXTE/HNDEL14.HLZ");
  for(int b=0;b<AUDIO_COUNT;b++)g_audio_percent[b]=0;setup_apply();
  G.dbg_stretch=G.dbg_camrot=G.ai_rescue=G.sim_model=1;graphics_mode_set(1);
  check(!g_sim_model&&!g_ai_rescue&&!g_cam_rot_interp&&!g_revived_text,"Retro disables revived physics, recovery, smoothed camera and scalable fonts");
  G.click=0;G.sel=3;G.state=GS_OPTIONS;Input key={};key.pressed[K_ENTER]=1;screen_options(&key);
  check(G.state==GS_RETRO_SOUND&&G.sel==0,"restored Retro options open a dedicated Advanced sound screen");
  key={};key.pressed[K_RIGHT]=1;screen_retro_sound(&key);check(g_audio_percent[0]==5&&G.sel==0,"Retro pixel slider responds to keyboard and mapped controller arrows");
  G.mx=316;G.my=153;G.click=1;key={};key.lmb=1;screen_retro_sound(&key);check(g_audio_percent[1]==50,"Retro engine slider midpoint sets exactly 50 percent");
  G.mx=562;G.click=0;screen_retro_sound(&key);check(g_audio_percent[1]==100,"Retro slider supports dragging to 100 percent");
  key={};key.pressed[K_ESC]=1;screen_retro_sound(&key);check(G.state==GS_OPTIONS&&G.sel==3&&g_audio_drag<0,"leaving Advanced sound returns focus to its original-menu entry");
  cv_clear(0);retro_volume_bar(70,100,50,BEIGE);check(g_cv[108*CW+95]==ORANGE&&g_cv[108*CW+89]==rgba(30,27,22,255)&&g_cv[108*CW+555]==rgba(78,72,57,255),"Retro slider uses discrete solid-color cells with visible pixel gaps");
  for(int language=0;language<3;language++){
    g_engine_suppressed=0;gf_language=language;
    check(cv_text_w(g_font_h14,"Advanced sound",1)<=279,"large original Advanced sound label fits its Retro column in all editions");
    check(cv_text_w(g_font_h14,"Retro controller",1)<=223,"large original controller label ends before the adjacent Debug button in every language");
    check(cv_text_w(g_font_h14,"Original (5/5/3)",1)<=279,"original-size laps label fits the old menu column");
    check(cv_text_w(g_font_h14,"Alle Waffen und Wagen",1)<=562,"full-width Retro cheats retain the original font size without clipping");
    check(cv_text_w(g_font_h14,"Race music",1)+cv_text_w_raw(g_font_h14,"100%",1)<480,"original-size sound labels leave room for percentage values");
  }
  gf_language=2;G.state=GS_DEBUG;G.sel=10;G.click=0;key={};key.pressed[K_ENTER]=1;g_dump_active=0;screen_debug(&key);
  check(!g_dump_active && G.dbg_stretch&&G.dbg_camrot&&G.ai_rescue&&G.sim_model,"Retro Reset is not Dump textures and retains hidden Revived preferences");
  G.sel=11;screen_debug(&key);check(G.state==GS_OPTIONS&&G.sel==10,"Retro debug has 12 visible entries and returns focus to Debug");
  graphics_mode_set(0);check(!g_sim_model&&g_ai_rescue&&g_cam_rot_interp,"switching back restores effective Revived technical preferences");
  G.dbg_stretch=G.dbg_camrot=G.sim_model=0;G.ai_rescue=1;debug_apply();
  for(int b=0;b<AUDIO_COUNT;b++)g_audio_percent[b]=100;setup_apply();
}
