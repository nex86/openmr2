static void test_revision_0714(){
  uint32_t input[]={0x00ffffff,0x800040ff,0xff735129},output[3];
  canvas_premultiply(output,input,3);
  check(output[0]==0&&output[1]==0x80002080&&output[2]==input[2],"filtered canvas clears transparent RGB, premultiplies coverage and preserves opaque colors");
  for(int profile=1;profile<=3;profile++){
    OriginalDrive result[2];
    for(int mode=0;mode<2;mode++){
      feature_world();graphics_mode_set(mode);RCar &c=player();c.profile=profile;
      for(int i=0;i<160;i++)original_car_drive(c,i<105?1:i<130?-1:0,i<60?.2:i<100?-.3:0,.04);
      result[mode]=c.fixed;
    }
    int same=1;for(unsigned i=0;i<sizeof(OriginalDrive);i++)if(((u8 *)&result[0])[i]!=((u8 *)&result[1])[i])same=0;
    check(same,"Revived and Retro use the same driving kernel over acceleration, steering, braking and coasting");
  }
  const char legacy[]="graphics_mode = 1\nretro_surprise = 1\nvehicle_lights = 1\ndamage_effects = 1\nmodern_explosions = 1\nreal_time_shadows = 1\nenhanced_oil = 1\nmissile_effects = 1\n";
  sys_write_file("revived_config/openmr2_setup.cfg",legacy,sizeof legacy-1);setup_load();debug_apply();
  check(!g_vehicle_lights&&!g_damage_effects&&!g_modern_explosions&&!g_realtime_shadows&&!g_enhanced_oil&&!g_missile_effects,"a saved legacy Surprise override cannot enable any modern Retro effects");
  setup_save();size_t n=0;u8 *cfg=sys_read_file("revived_config/openmr2_setup.cfg",&n);
  check(cfg&&!str_icontains((char *)cfg,"retro_surprise"),"saving migrates away the removed Retro override");sys_free(cfg);
  int16_t samples[4]={1000,1000,1000,1000},out[2];Sound arbitrary={samples,4,1};
  g_audio_percent[AUDIO_ENGINES]=100;g_engine_suppressed=0;
  int h=snd_play(&arbitrary,1,1,1,0,AUDIO_ENGINES);check(snd_voice(h)!=0,"engine bus accepts an arbitrary sample while a race is active");
  RC.eng_voice=RC.eng2_voice=-1;race_end_sounds();
  check(!snd_voice(h)&&snd_play(&arbitrary,1,1,1,0,AUDIO_ENGINES)<0,"teardown rejects late engine restarts independently of sample identity and lost handles");
  mem_zero(MX.v,sizeof MX.v);MX.v[0].snd=&arbitrary;MX.v[0].active=MX.v[0].loop=1;MX.v[0].rate=MX.v[0].vol=1;MX.v[0].bus=AUDIO_ENGINES;
  MX.muted=0;app_audio_fill(out,1);check(!MX.v[0].active&&!out[0]&&!out[1],"Game Over mixer guard silences a restored engine even in a reserved voice slot");
  graphics_mode_set(0);
}
