// Real, incremental race preparation. Counts completed resource jobs, never time.
enum RaceLoadKind{RL_TRACK,RL_CAR,RL_OBJECT,RL_HUD,RL_PAUSE,RL_FONT,RL_AUDIO,RL_MUSIC,RL_SHADER,RL_VIDEO_SHADER,RL_FIRST_FRAME};
struct RaceLoadTask{int kind,index;};
struct RacePreload{RaceLoadTask tasks[192];int count,next,active,complete,failed,track,heat;RaceAudioPreload audio;const char *phase;};
static RacePreload g_race_preload;
static void post_ensure(int w,int h);
static void race_preload_add(int kind,int index=0){RacePreload &p=g_race_preload;if(p.count<192)p.tasks[p.count++]={kind,index};}
static void race_preload_begin(int track,int heat){
  RacePreload &p=g_race_preload;mem_zero(&p,sizeof p);p.active=1;p.track=track;p.heat=heat;p.phase="Preparing race";
  race_preload_add(RL_TRACK);
  for(int i=0;i<NCARS;i++)race_preload_add(RL_CAR,i);
  for(int i=0;i<OM_COUNT;i++)race_preload_add(RL_OBJECT,i);
  for(int i=0;i<6;i++)race_preload_add(RL_HUD,i);
  for(int i=0;i<5;i++)race_preload_add(RL_PAUSE,i);
  for(int i=0;i<5;i++)race_preload_add(RL_FONT,i);
  int cars[9],n=0;cars[n++]=PF.car;for(int i=1;i<PF.nfield&&i<8;i++)cars[n++]=PF.field[i];
  race_audio_preload_begin(&p.audio,cars,n);for(int i=0;i<p.audio.count;i++)race_preload_add(RL_AUDIO,i);
  if(audio_enabled(AUDIO_RACE_MUSIC))race_preload_add(RL_MUSIC);
  for(int i=0;i<6;i++)race_preload_add(RL_SHADER,i);
  if(g_video_upscale&&!g_tex_nearest){
    int count=g_video_filter>=4?VIDEO_CNN_MODELS[g_video_filter-4].count+2:g_video_filter==1?2:1;
    for(int i=0;i<count;i++)race_preload_add(RL_VIDEO_SHADER,i);
  }
  race_preload_add(RL_FIRST_FRAME);sys_log("[loading] Preparing race resources; UBB video frames remain streamed.");
}
static void race_preload_model(Model *m){
  if(!m||g_tex_nearest||!g_custom_textures)return;
  for(int i=0;i<m->ntex;i++)texture_gpu(texture_custom(m->texture_keys[i]),1,0);
}
static void race_preload_sprite(const SprFile &sprites,const u8 *palette,int frame=-1){
  if(g_tex_nearest||(!g_custom_textures&&!g_revived_ui))return;
  for(int i=0;i<sprites.n;i++)if(frame<0||frame==i){const Sprite &s=sprites.f[i];char key[512];texture_sprite_key(key,s.texture_source,s.frame,palette);texture_gpu(texture_for_canvas(key),0,1);}
}
static void race_preload_picture(const Pic &pic){
  if(g_tex_nearest)return;TextureImage *image=texture_for_canvas(pic.texture_key);if(!image&&g_smooth_filter)image=texture_original_picture(pic);texture_gpu(image,0,1);
}
static void race_preload_video_shader(int step){
  int filter=g_video_filter;if(filter<0||filter>=VIDEO_FILTER_COUNT)return;
  if(filter>=4){int model=filter-4;const VideoCnnModel &m=VIDEO_CNN_MODELS[model];
    if(step<m.count){if(!g_video_cnn_program[model][step])g_video_cnn_program[model][step]=video_program_build(VS_FULL,m.passes[step].fragment);}
    else if(step==m.count){if(!g_video_cnn_compose)g_video_cnn_compose=video_program_build(VS_FULL,FS_VIDEO_CNN_COMPOSE);}
    else video_filter_program(4);
  }else if(filter==1)video_filter_program(step?2:1);
  else video_filter_program(filter==0?0:filter==2?3:4);
}
static void race_preload_step(int W,int H){
  RacePreload &p=g_race_preload;if(!p.active||p.complete||p.failed)return;
  if(p.next>=p.count){p.complete=1;p.phase="Ready";return;}
  RaceLoadTask task=p.tasks[p.next];
  switch(task.kind){
    case RL_TRACK:p.phase="Loading track data";if(!track_load(p.track,p.heat,0))p.failed=1;break;
    case RL_CAR:p.phase="Loading 3D models and textures";{Model *m=track_car_model(task.index);if(!m)p.failed=1;race_preload_model(m);}break;
    case RL_OBJECT:p.phase="Loading 3D models and textures";if(!TK.obj_models[task.index])TK.obj_models[task.index]=load_model_from(TRACKS[p.track].set,OBJ_MODEL_FILES[task.index]);race_preload_model(TK.obj_models[task.index]);break;
    case RL_HUD:p.phase="Loading HUD and custom textures";race_preload_sprite(TK.hud,TK.hud_pal,task.index);break;
    case RL_PAUSE:p.phase="Loading HUD and custom textures";if(task.index==0)race_preload_picture(g_racemenu);else if(task.index<4)race_preload_sprite(g_pausedrum[task.index-1],g_racemenu.pal);else race_preload_sprite(G.cursor_spr,G.fleche.pal);break;
    case RL_FONT:{p.phase="Loading HUD and custom textures";const Font *fonts[]={&g_font_big,&g_font_small,&g_font_mid,&g_font_h14,&g_font_h10};race_preload_picture(fonts[task.index]->pic);}break;
    case RL_AUDIO:p.phase="Loading race sounds";race_audio_preload_step(&p.audio);break;
    case RL_MUSIC:p.phase="Loading race music";if(!TK.music.pcm){char md[64]="MUSIC/";str_cat(md,TRACKS[p.track].music,sizeof md);load_music_dir(&TK.music,md,".16");}break;
    case RL_SHADER:p.phase="Compiling shaders";switch(task.index){
      case 0:occluder_preload();break;
      case 1:if(!g_tex_nearest&&(g_modern_explosions||g_damage_effects||g_missile_effects))effects_preload();break;
      case 2:if(!g_tex_nearest&&g_vehicle_lights)vehicle_lights_preload(W,H);break;
      case 3:if(!g_tex_nearest&&g_realtime_shadows)shadow_resources();break;
      case 4:reflections_preload(W,H);break;
      case 5:if(!g_tex_nearest&&G.gfx_fxaa)post_ensure(W,H);break;
    }break;
    case RL_VIDEO_SHADER:p.phase="Compiling video shaders";race_preload_video_shader(task.index);break;
    case RL_FIRST_FRAME:p.phase="Preparing streamed video";
      if(!video_decode(&TK.vid,0))p.failed=1;
      else{
        for(int i=0;i<2;i++){video_tex_upload(&vslot[i],TK.vid);if(g_video_upscale&&!g_tex_nearest)video_fsr_texture(vslot[i],W,H);vslot_frame[i]=0;}
        vslot_next=0;g_race_tex_frame=0;glFinish();
      }break;
  }
  p.next++;if(p.failed){p.active=0;sys_log("[error] Race resource preparation failed.");}
  else if(p.next==p.count){p.complete=1;p.phase="Ready";sys_log("[loading] Race resources ready: models, textures, sounds and selected shaders prepared.");}
  if(sys_debug_enabled()){char text[200]="[loading] ";str_cat(text,p.phase,sizeof text);str_cat(text," ",sizeof text);str_int(text,sizeof text,p.next);str_cat(text,"/",sizeof text);str_int(text,sizeof text,p.count);sys_log(text);}
}
static void race_preload_cancel(){mem_zero(&g_race_preload,sizeof g_race_preload);sys_log("[loading] Race loading cancelled.");}
static void race_preload_test_report(){
  RacePreload &p=g_race_preload;int cars=0,objects=0;for(int i=0;i<NCARS;i++)cars+=TK.car_models[i]!=0;for(int i=0;i<OM_COUNT;i++)objects+=TK.obj_models[i]!=0;
  const char *keys[]={"state","completed_jobs","total_jobs","complete","failed","cars","objects","video_streaming","video_bytes","video_size","effects_shader","light_shader"};
  long values[]={G.state,p.next,p.count,p.complete,p.failed,cars,objects,TK.vid.streaming,(long)TK.vid.stream_bytes_read,(long)TK.vid.size,g_effect_program!=0,LM.program!=0};
  char text[1400]="";for(int i=0;i<12;i++){str_cat(text,keys[i],sizeof text);str_cat(text,"=",sizeof text);str_int(text,sizeof text,values[i]);str_cat(text,"\n",sizeof text);}sys_write_file("race-preload-state.txt",text,str_len(text));
}
