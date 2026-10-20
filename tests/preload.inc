// Resource work, rather than an elapsed-time animation, drives race readiness.
static GLuint g_preload_test_texture=0x700000;
static void preload_test_gen_textures(GLsizei count,GLuint *textures){
  for(int i=0;i<count;i++)textures[i]=g_preload_test_texture++;
}
static void test_race_preload(){
  int saved_audio[AUDIO_COUNT];mem_cpy(saved_audio,g_audio_percent,sizeof saved_audio);
  int saved_nearest=g_tex_nearest,saved_custom=g_custom_textures,saved_ui=g_revived_ui;
  int saved_upscale=g_video_upscale,saved_filter=g_video_filter,saved_fxaa=G.gfx_fxaa;
  int saved_explosions=g_modern_explosions,saved_damage=g_damage_effects,saved_missiles=g_missile_effects;
  int saved_lights=g_vehicle_lights,saved_shadows=g_realtime_shadows,saved_reflections=g_vehicle_reflections;
  Profile saved_profile=PF;PF.car=0;PF.single=0;PF.nfield=3;PF.field[0]=0;PF.field[1]=1;PF.field[2]=2;
  for(int bus=0;bus<AUDIO_COUNT;bus++)g_audio_percent[bus]=0;
  g_tex_nearest=0;g_custom_textures=1;g_revived_ui=0;g_video_upscale=0;g_video_filter=0;G.gfx_fxaa=0;
  g_modern_explosions=g_damage_effects=g_missile_effects=g_vehicle_lights=g_realtime_shadows=g_vehicle_reflections=0;
  auto original_gen_textures=glGenTextures;glGenTextures=preload_test_gen_textures;
  track_free();race_preload_cancel();texture_cache_clear();

  Model *probe=load_model_from("FAC",CARS[PF.car].model);char key[512]={},path[600]="revived_config/texture_replacements/";
  check(probe&&probe->ntex>0,"preload fixture exposes the selected original vehicle texture identity");
  if(probe&&probe->ntex>0)str_cpy(key,probe->texture_keys[0],sizeof key);
  if(probe){model_free(probe);sys_free(probe);}
  str_cat(path,key,sizeof path);size_t previous_size=0;u8 *previous=key[0]?sys_read_file(path,&previous_size):0;
  const u8 replacement[]={12,234,56,255, 200,30,80,128, 80,60,40,0, 3,5,7,255};
  if(key[0])check(png_write(path,replacement,2,2),"preload fixture installs an isolated replacement PNG");

  race_preload_begin(0,0);RacePreload &p=g_race_preload;
  int count=p.count,audio_tasks=0,music_tasks=0;
  for(int i=0;i<count;i++){audio_tasks+=p.tasks[i].kind==RL_AUDIO;music_tasks+=p.tasks[i].kind==RL_MUSIC;}
  check(p.active&&!p.complete&&!p.failed&&!p.next&&count>NCARS,"race preload begins with a finite uncompleted work list");
  check(!audio_tasks&&!music_tasks&&!p.audio.count,"all-zero audio omits sound and music work entirely");
  check(p.tasks[0].kind==RL_TRACK&&p.tasks[count-1].kind==RL_FIRST_FRAME,"track metadata precedes assets and first-frame preparation is last");
  race_preload_step(640,480);
  check(p.next==1&&!p.complete&&!p.failed&&TK.loaded&&TK.idx==0,"one preload step performs exactly the track-metadata job");
  check(TK.vid.streaming&&TK.vid.stream_capacity<TK.vid.size&&TK.vid.stream_bytes_read<TK.vid.size,
        "track preparation indexes an ISO stream instead of retaining the full UBB file");
  check(TK.vid.decoded_frame==-1&&!TK.music.pcm&&!TK.car_models[0],"metadata job does not decode video frames or preload later model/music jobs");
  race_preload_step(640,480);race_preload_step(640,480);
  check(p.next==3&&TK.car_models[0]&&TK.car_models[1]&&!TK.car_models[2],"successive model tasks complete one car at a time");
  race_preload_cancel();int cancelled_next=p.next;race_preload_step(640,480);
  check(!p.active&&!p.complete&&!p.count&&p.next==cancelled_next&&!TK.car_models[2],"cancellation stops work and clears readiness without running another job");

  race_preload_begin(0,1);count=p.count;
  check(p.active&&!p.next&&!p.complete&&p.heat==1&&count>0,"a cancelled race can restart a fresh counted preparation");
  int iterations=0;
  while(p.active&&!p.complete&&!p.failed&&iterations++<192){
    int before=p.next,kind=p.tasks[before].kind,index=p.tasks[before].index;
    race_preload_step(640,480);
    check(p.next==before+1&&p.count==count,"preload progress increases by exactly one completed resource job");
    check(p.complete==(p.next==count),"readiness is published only after the final scheduled job");
    if(kind!=RL_FIRST_FRAME)check(TK.vid.decoded_frame==-1,"asset preparation does not decode the rest of the track video");
    if(kind==RL_CAR&&index==PF.car&&key[0]){
      TextureImage *cached=0;
      for(int i=0;i<g_texture_cache.n;i++)if(!str_icmp(g_texture_cache.p[i].key,key))cached=&g_texture_cache.p[i];
      check(cached&&cached->pixels&&cached->w==2&&cached->h==2&&!memcmp(cached->pixels,replacement,sizeof replacement),
            "selected vehicle task decodes its exact replacement PNG before the race");
      check(cached&&cached->tex>=0x700000,"selected vehicle replacement is uploaded during preparation rather than first drawing");
    }
  }
  check(p.complete&&!p.failed&&p.next==p.count,"the complete counted pipeline reaches readiness");
  for(int car=0;car<NCARS;car++)check(TK.car_models[car]&&TK.car_models[car]->loaded,"all fourteen original vehicle models are ready before the race");
  check(TK.vid.decoded_frame==0&&TK.vid.stream_capacity<TK.vid.size&&TK.vid.stream_bytes_read<TK.vid.size,
        "final job prepares only the first streamed video frame with bounded compressed storage");
  check(!TK.music.pcm,"muted race preparation never decodes a music track");
  uint64_t disk_bytes=TK.vid.stream_bytes_read;int complete_next=p.next;race_preload_step(640,480);
  check(p.next==complete_next&&TK.vid.stream_bytes_read==disk_bytes,"completed preparation performs no repeated work or video reads");

  race_preload_cancel();g_audio_percent[AUDIO_RACE_MUSIC]=100;race_preload_begin(0,0);music_tasks=0;
  for(int i=0;i<p.count;i++)music_tasks+=p.tasks[i].kind==RL_MUSIC;
  check(music_tasks==1,"enabled race music adds its own real loading job");
  race_preload_cancel();track_free();texture_cache_clear();
  if(key[0]){if(previous)sys_write_file(path,previous,previous_size);else sys_delete_file(path);}sys_free(previous);
  glGenTextures=original_gen_textures;PF=saved_profile;mem_cpy(g_audio_percent,saved_audio,sizeof saved_audio);
  g_tex_nearest=saved_nearest;g_custom_textures=saved_custom;g_revived_ui=saved_ui;
  g_video_upscale=saved_upscale;g_video_filter=saved_filter;G.gfx_fxaa=saved_fxaa;
  g_modern_explosions=saved_explosions;g_damage_effects=saved_damage;g_missile_effects=saved_missiles;
  g_vehicle_lights=saved_lights;g_realtime_shadows=saved_shadows;g_vehicle_reflections=saved_reflections;
}
