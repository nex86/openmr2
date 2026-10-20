// Exercise the actual shop gate, mixer and incremental WAV preload callbacks.
static void test_audio_menu(){
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
