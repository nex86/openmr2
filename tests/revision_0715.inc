static void test_revision_0715(){
  G.state=GS_RACE;g_engine_suppressed=0;g_audio_percent[AUDIO_SOUNDS]=100;MX.sfx_vol=1;
  g_rng=12345;g_rng_fx=54321;unsigned gameplay_rng=g_rng;g_impact_last=-1;
  int previous=-1,seen=0;
  for(int n=0;n<30;n++){
    snd_stop_all_sfx();impact_sound(player().W,.8f);
    int id=-1;for(int k=2;k<NVOICES;k++)if(MX.v[k].active)for(int j=0;j<3;j++){
      const int ids[]={S_CHOC1,S_CHOC3,S_CHOC5};if(MX.v[k].snd==&SFX[ids[j]])id=j;
    }
    check(id>=0&&id!=previous,"successive actual impact voices choose different original collision samples");
    if(id>=0)seen|=1<<id;previous=id;
  }
  check(seen==7&&g_rng==gameplay_rng,"impact variety uses all three sounds without changing driving/AI random state");
  int last=g_impact_last,serial=MX.serial;g_audio_percent[AUDIO_SOUNDS]=0;impact_sound(player().W,1);
  check(g_impact_last==last&&MX.serial==serial,"muted collisions allocate no voice and do not advance their selection");
  g_audio_percent[AUDIO_SOUNDS]=100;
  int16_t pcm[4]={1000,1000,1000,1000},mixed[128];Sound sample={pcm,4,1};
  G.state=GS_GAMEOVER;
  // Include wrong buses, a cleared teardown latch, one-shots, music/voice slots
  // and repeated restart attempts: the screen state itself must win.
  for(int block=0;block<40;block++){
    g_engine_suppressed=0;
    for(int i=0;i<NVOICES;i++){Voice &v=MX.v[i];v.snd=&sample;v.active=1;v.pos=0;v.rate=1;v.vol=1;v.bus=i%AUDIO_COUNT;v.loop=block&1;}
    app_audio_fill(mixed,64);int silent=1,active=0;
    for(int i=0;i<128;i++)if(mixed[i])silent=0;
    for(int i=0;i<NVOICES;i++)active|=MX.v[i].active;
    check(silent&&!active,"Game Over produces exact zero PCM on every bus despite restarted or incorrectly classified voices");
  }
  G.state=GS_MENU;g_engine_suppressed=0;MX.muted=0;g_audio_percent[AUDIO_MENU_SOUNDS]=100;
  snd_play(&sample,1,1,1,0,AUDIO_MENU_SOUNDS);app_audio_fill(mixed,1);
  check(mixed[0]!=0&&mixed[1]!=0,"leaving Game Over restores ordinary menu audio immediately");
  snd_stop_all_sfx();
}
