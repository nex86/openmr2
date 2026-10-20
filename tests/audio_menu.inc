// Restored original player skid-loop hysteresis and actual WAV/mixer output.
static void test_tire_audio(){
  Race saved_race=RC;Mixer saved_mixer=MX;Sound saved_sample=SFX[S_DERAP];
  int saved_state=G.state,saved_suppressed=g_engine_suppressed,saved_nearest=g_tex_nearest,saved_percent[AUDIO_COUNT];
  mem_cpy(saved_percent,g_audio_percent,sizeof saved_percent);mem_zero(&MX,sizeof MX);mem_zero(&RC,sizeof RC);mem_zero(&SFX[S_DERAP],sizeof SFX[S_DERAP]);
  MX.sfx_vol=1;G.state=GS_RACE;g_engine_suppressed=0;RC.active=1;RC.phase=PH_RACE;RC.skid_voice=-1;player().fixed_ready=1;
  for(int bus=0;bus<AUDIO_COUNT;bus++)g_audio_percent[bus]=0;g_audio_percent[AUDIO_SOUNDS]=100;
  unsigned rng=g_rng,fx_rng=g_rng_fx;
  check(skid_gain(0)>.1869f&&skid_gain(0)<.1871f&&skid_gain(V_PLAYER)>.3519f&&skid_gain(V_PLAYER)<.3521f,
        "skid gain preserves the original22/20 ratio against the existing low/high-speed engine envelope");
  check(skid_gain(-V_PLAYER)==skid_gain(V_PLAYER),"reverse sliding retains the same original cue-to-engine level ratio");
  for(int mode=0;mode<2;mode++){
    g_tex_nearest=mode;player().v=mode?V_PLAYER:0;
    int serial=MX.serial;
    for(int frames=0;frames<=2;frames++){player().fixed.slip_frames=frames;skid_update();}
    check(MX.serial==serial&&!snd_voice(RC.skid_voice),"brief one/two-update slips do not start the original tire squeal in either mode");
    if(!mode)check(!SFX[S_DERAP].pcm,"ordinary grip and brief slip do not lazily decode a tire WAV");
    player().fixed.slip_frames=3;skid_update();Voice *voice=snd_voice(RC.skid_voice);int handle=RC.skid_voice;
    check(voice&&voice->snd==&SFX[S_DERAP]&&voice->loop&&voice->bus==AUDIO_SOUNDS&&voice->rate==1,
          "three consecutive slipping updates start original DERAP_A as a dedicated 22050-Hz Sounds loop");
    check(voice&&voice->vol==skid_gain(player().v)&&SFX[S_DERAP].pcm,"tire squeal adapts original level22 to the existing engine envelope and loads actual original PCM");
    int16_t mixed[1024];app_audio_fill(mixed,512);int audible=0;for(int i=0;i<1024;i++)if(mixed[i])audible=1;
    check(audible,"actual skid WAV produces nonzero stereo PCM while engines and music are disabled");
    serial=MX.serial;player().fixed.slip_frames=2;skid_update();player().fixed.slip_frames=1;skid_update();
    check(RC.skid_voice==handle&&snd_voice(handle)&&MX.serial==serial,"active squeal keeps its phase through slip-counter hysteresis without repeated sample restarts");
    player().fixed.slip_frames=0;skid_update();check(!snd_voice(handle)&&RC.skid_voice<0,"restored tire grip stops the loop immediately in both modes");
    app_audio_fill(mixed,512);int silent=1;for(int i=0;i<1024;i++)if(mixed[i])silent=0;
    check(silent,"restored tire grip produces exact zero PCM rather than leaving a hidden squeal loop");
  }
  player().fixed.slip_frames=4;skid_update();int handle=RC.skid_voice;RC.paused=1;skid_update();
  check(!snd_voice(handle),"pausing silences an active tire squeal");RC.paused=0;
  skid_update();handle=RC.skid_voice;g_audio_percent[AUDIO_SOUNDS]=0;skid_update();
  check(!snd_voice(handle)&&RC.skid_voice<0,"zero Sounds volume stops tire squeal independently of engine settings");
  sound_free(&SFX[S_DERAP]);int serial=MX.serial;skid_update();
  check(!SFX[S_DERAP].pcm&&MX.serial==serial,"muted sliding neither reloads its WAV nor allocates a voice");
  g_audio_percent[AUDIO_SOUNDS]=100;
  for(int guard=0;guard<5;guard++){
    RC.active=1;player().out=player().finished=0;RC.phase=PH_RACE;g_engine_suppressed=0;
    if(guard==0)RC.active=0;else if(guard==1)player().out=1;else if(guard==2)player().finished=1;else if(guard==3)RC.phase=PH_DONE;else g_engine_suppressed=1;
    skid_update();check(!SFX[S_DERAP].pcm&&MX.serial==serial,"inactive, destroyed, finished or retired race state cannot load/start tire squeal");
  }
  RC.active=1;player().out=player().finished=0;RC.phase=PH_RACE;g_engine_suppressed=0;skid_update();handle=RC.skid_voice;
  check(snd_voice(handle)!=0,"a later valid sliding race can restart the original tire sound");
  RC.skid_voice=-1;race_end_sounds();check(!snd_voice(handle),"race teardown retires tire loops even after losing their handles");
  serial=MX.serial;skid_update();check(MX.serial==serial,"race teardown latch rejects late tire-loop restarts");
  G.state=GS_GAMEOVER;g_engine_suppressed=0;skid_update();int16_t gameover_pcm[128];app_audio_fill(gameover_pcm,64);
  int silent=1;for(int i=0;i<128;i++)if(gameover_pcm[i])silent=0;
  check(silent&&!snd_voice(RC.skid_voice),"Game Over mixer guard silences tire PCM even after a late restart with the teardown latch cleared");G.state=GS_RACE;
  check(g_rng==rng&&g_rng_fx==fx_rng,"tire audio does not consume gameplay or impact-sound random state");
  OriginalDrive before=player().fixed;skid_update();
  int unchanged=1;for(unsigned i=0;i<sizeof before;i++)if(((u8 *)&before)[i]!=((u8 *)&player().fixed)[i])unchanged=0;
  check(unchanged,"tire audio reads the original drive state without changing the validated physics");
  skid_stop();sound_free(&SFX[S_DERAP]);SFX[S_DERAP]=saved_sample;RC=saved_race;MX=saved_mixer;
  G.state=saved_state;g_engine_suppressed=saved_suppressed;g_tex_nearest=saved_nearest;mem_cpy(g_audio_percent,saved_percent,sizeof saved_percent);
}

// Exercise the actual shop gate, mixer and incremental WAV preload callbacks.
static void test_audio_menu(){
  test_tire_audio();
  int saved_state=G.state,saved_message=G.msg_next,saved_single=PF.single,saved_in_single=G.in_single;
  int saved_suppressed=g_engine_suppressed,saved_percent[AUDIO_COUNT];
  Mixer saved_mixer=MX;Sound saved_samples[S_COUNT];
  mem_cpy(saved_percent,g_audio_percent,sizeof saved_percent);mem_cpy(saved_samples,SFX,sizeof saved_samples);
  mem_zero(&MX,sizeof MX);mem_zero(SFX,sizeof SFX);MX.sfx_vol=MX.music_vol=1;g_engine_suppressed=0;
  for(int bus=0;bus<AUDIO_COUNT;bus++)g_audio_percent[bus]=100;
  PF.single=G.in_single=0;G.msg_next=GS_MENU;
  for(int state=0;state<=GS_QUIT;state++){
    if(state==GS_CARSHOP||state==GS_WEAPONSHOP)continue;
    G.state=state;int serial=MX.serial;msfx(S_MENU2,1);
    check(MX.serial==serial,"non-shop screens never start a menu-effect voice");
    check(!SFX[S_MENU2].pcm,"non-shop screens bypass menu WAV loading");
  }
  static int16_t pcm[16]={1000,-1000};Sound tone={pcm,16,1};
  for(int id=S_MENU1;id<=S_MENU4;id++)SFX[id]=tone;
  for(int state=GS_CARSHOP;state<=GS_WEAPONSHOP;state++){
    G.state=state;
    for(int id=S_MENU1;id<=S_MENU4;id++){
      int serial=MX.serial;msfx(id,.75f);int heard=0;
      for(int v=2;v<NVOICES;v++)if(MX.v[v].active&&MX.v[v].snd==&SFX[id]&&MX.v[v].bus==AUDIO_MENU_SOUNDS)heard++;
      check(MX.serial==serial+1&&heard==1,"career shop plays the requested original menu sample on its own bus");
      snd_stop_all_sfx();
    }
  }
  G.state=GS_MESSAGE;
  for(int next=GS_CARSHOP;next<=GS_WEAPONSHOP;next++){
    G.msg_next=next;int serial=MX.serial;msfx(S_MENU1,.75f);
    check(MX.serial==serial+1,"shop modal messages retain their original warning sample");snd_stop_all_sfx();
  }
  G.state=GS_CARSHOP;PF.single=1;int serial=MX.serial;msfx(S_MENU2,1);
  check(MX.serial==serial,"single-race state cannot enable career shop effects");
  PF.single=0;G.in_single=1;msfx(S_MENU2,1);
  check(MX.serial==serial,"single-race car preview remains silent");G.in_single=0;
  g_audio_percent[AUDIO_MENU_SOUNDS]=0;msfx(S_MENU2,1);
  check(MX.serial==serial,"zero shop volume prevents shop playback");g_audio_percent[AUDIO_MENU_SOUNDS]=100;
  G.state=GS_MENU;music_play(&tone,1,AUDIO_MENU_MUSIC);voice_play(&tone,1);serial=MX.serial;msfx(S_MENU2,1);
  check(MX.serial==serial&&MX.v[0].active&&MX.v[0].bus==AUDIO_MENU_MUSIC&&MX.v[1].active,
        "silent menu controls leave menu music and cinematic soundtracks active");
  G.state=GS_RACE;SFX[S_ENGINE]=tone;
  int race_voice=snd_play(&tone,1,1,0,0,AUDIO_SOUNDS),engine_voice=engine_play(S_ENGINE,1,1);
  check(snd_voice(race_voice)&&snd_voice(engine_voice),"shop-only menu gating leaves race effects and engines available");
  snd_stop_all_sfx();music_stop();voice_stop();mem_zero(SFX,sizeof SFX);

  // Enabled race effects are warmed one file at a time, without menu samples.
  for(int bus=0;bus<AUDIO_COUNT;bus++)g_audio_percent[bus]=0;
  g_audio_percent[AUDIO_SOUNDS]=100;int cars[NCARS];for(int i=0;i<NCARS;i++)cars[i]=i;
  RaceAudioPreload job;int count=race_audio_preload_begin(&job,cars,NCARS);
  check(count==S_PLOT-S_CHOC1+1,"race audio preload queues all used effects and skips disabled engines/music/shop effects");
  int valid=1;for(int i=0;i<job.count;i++)if(job.ids[i]<S_CHOC1||job.ids[i]>S_PLOT)valid=0;
  check(valid,"race-effect preload contains no unused or menu-only samples");
  serial=MX.serial;int complete=race_audio_preload_step(&job);
  check(job.next==1&&!complete&&SFX[S_CHOC1].pcm,"one preload tick decodes exactly its first original WAV");
  check(!SFX[S_CHOC3].pcm&&MX.serial==serial,"incremental preload neither decodes the next file nor starts playback");
  g_audio_percent[AUDIO_SOUNDS]=0;while(!race_audio_preload_step(&job)){}
  check(job.next==job.count&&!SFX[S_CHOC3].pcm,"audio disabled during loading skips remaining file decoding");
  g_audio_percent[AUDIO_ENGINES]=100;count=race_audio_preload_begin(&job,cars,NCARS);
  check(count==3,"all fourteen original cars require only three unique engine WAVs");
  int engines[3]={0,0,0};valid=1;
  for(int i=0;i<job.count;i++){
    if(job.ids[i]==S_ENGINE)engines[0]++;
    else if(job.ids[i]==S_ENGINE2)engines[1]++;
    else if(job.ids[i]==S_ENGINE3)engines[2]++;
    else valid=0;
  }
  check(valid&&engines[0]==1&&engines[1]==1&&engines[2]==1,
        "engine preload deduplicates exact original vehicle assignments");
  while(!race_audio_preload_step(&job)){}
  check(SFX[S_ENGINE].pcm&&SFX[S_ENGINE2].pcm&&SFX[S_ENGINE3].pcm&&MX.serial==serial,
        "selected engine prewarming decodes its WAVs without playing them");
  check(!race_audio_preload_begin(&job,cars,NCARS)&&race_audio_preload_step(&job),
        "already cached engines need no repeated loading work");
  for(int bus=0;bus<AUDIO_COUNT;bus++)g_audio_percent[bus]=0;
  check(!race_audio_preload_begin(&job,cars,NCARS)&&race_audio_preload_step(&job)&&MX.serial==serial,
        "all-zero audio has no preload tasks or playback side effects");

  for(int id=0;id<S_COUNT;id++)if(SFX[id].pcm&&SFX[id].pcm!=pcm&&SFX[id].pcm!=saved_samples[id].pcm)sound_free(&SFX[id]);
  mem_cpy(SFX,saved_samples,sizeof saved_samples);mem_cpy(g_audio_percent,saved_percent,sizeof saved_percent);MX=saved_mixer;
  G.state=saved_state;G.msg_next=saved_message;PF.single=saved_single;G.in_single=saved_in_single;g_engine_suppressed=saved_suppressed;
}
