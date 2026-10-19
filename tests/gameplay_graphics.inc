static RoadPt feature_road[40];static Model feature_barrel,feature_mine;
static void feature_world(){
  mem_zero(&TK,sizeof TK);mem_zero(&RC,sizeof RC);mem_zero(feature_road,sizeof feature_road);original_motion_reset();
  TK.road=feature_road;TK.nroad=TK.L=40;TK.obj_models[OM_BARREL]=&feature_barrel;TK.obj_models[OM_MINE]=&feature_mine;
  feature_barrel.bmin=v3(-3,0,-3);feature_barrel.bmax=v3(3,6.4f,3);feature_mine.bmin=v3(-2,0,-2);feature_mine.bmax=v3(2,.476f,2);
  for(int i=0;i<40;i++){RoadPt &r=feature_road[i];r.p[2]=i*PT;r.rt[0]=r.up[1]=r.fw[2]=1;r.w=40000;r.nx[0]=r.nx[1]=(i+1)%40;r.pv[0]=r.pv[1]=(i+39)%40;r.len[0]=r.len[1]=PT;r.physics_length[0]=r.physics_length[1]=(int)PT;r.M=i;}
  RC.ncar=1;RC.active=1;RC.phase=PH_RACE;RCar &c=player();c.pp=pp_make(0,0);c.x=30000;c.player=1;c.hwm=6000;c.profile=1;car_update_world(c);
}
static void feature_barrel_at(int index,int node){TrackObj &o=TK.obj[index];mem_zero(&o,sizeof o);o.alive=1;o.type=2;o.node=node;o.pp=pp_make(node,0);pp_world(o.pp,0,0,o.W);if(index>=TK.nobj)TK.nobj=index+1;}
static Proj feature_missile(int kind=W_KARD){Proj p={};p.alive=1;p.owner=0;p.kind=kind;p.pp=pp_make(0,0);p.life=4;p.v=p.vlong=(kind==W_KARD?2850:kind==W_BOOMER?2280:1710)*V_RAW;return p;}
static void test_gameplay_graphics(){
  int x,y,w,h;
  for(int integer=0;integer<2;integer++)for(int factor=1;factor<=4;factor++){
    retro_output_rect(3840,2160,factor,integer,&x,&y,&w,&h);check(w==640*factor&&h==480*factor&&x==(3840-w)/2&&y==(2160-h)/2,"every selected Retro scale works with integer scaling enabled or disabled");
  }
  retro_output_rect(1920,1080,4,0,&x,&y,&w,&h);check(w==1440&&h==1080,"fractional down-fit respects chosen maximum and fills the available 4:3 area");
  retro_output_rect(1920,1080,4,1,&x,&y,&w,&h);check(w==1280&&h==960,"integer down-fit selects the largest whole factor below the selected maximum");
  retro_output_rect(480,360,1,1,&x,&y,&w,&h);check(w==480&&h==360,"small windows retain a safe fractional fallback even with integer scaling enabled");
  CfgItem items[CFG_MAX_ITEMS];check(cfg_items(items)<=CFG_MAX_ITEMS,"additional graphics preferences fit the config table");
  G.real_time_shadows=G.modern_explosions=1;graphics_mode_set(0);setup_save();G.real_time_shadows=G.modern_explosions=0;setup_load();check(G.real_time_shadows&&G.modern_explosions,"modern shadow/explosion switches persist through config saves");
  graphics_mode_set(1);check(!g_realtime_shadows&&!g_modern_explosions&&G.real_time_shadows&&G.modern_explosions,"Retro suppresses modern effects without losing saved Revived preferences");
  graphics_mode_set(0);check(g_realtime_shadows&&g_modern_explosions,"returning to Revived restores both modern effect preferences");
  for(int language=0;language<3;language++){gf_language=language;check(cv_text_w(g_font_h14,"Modern explosion effects",1)<560&&cv_text_w(g_font_h14,"Real-time shadows",1)<560,"new graphics labels fit every language");}
  const char *sets[]={"FAC","TIB","EXT","LUN","BAY","BRO"},*editions[]={"English","French","German"};
  float lengths[3]={};
  for(int edition=0;edition<3;edition++)for(int world=0;world<6;world++)for(int kind=W_KARD;kind<=W_HITRAX;kind++){
    char directory[512]="../../../Megarace 2 ";str_cat(directory,editions[edition],sizeof directory);str_cat(directory,"/CARS/",sizeof directory);str_cat(directory,sets[world],sizeof directory);
    Model m={};int id=projectile_model(kind);check(model_load(&m,directory,OBJ_MODEL_FILES[id]),"every missile flight model loads in all six worlds of each supported edition");
    float width=m.bmax.x-m.bmin.x,height=m.bmax.y-m.bmin.y,length=m.bmax.z-m.bmin.z;
    check(width>1.9f&&width<2.1f&&height>2.8f&&height<3.8f,"missiles keep their original approximately two-meter native width, without shop-model enlargement");
    if(!world&&!edition)lengths[kind-W_KARD]=length;model_free(&m);
  }
  check(lengths[0]>3.2f&&lengths[0]<3.4f&&lengths[1]>4.2f&&lengths[1]<4.4f&&lengths[2]>6.0f&&lengths[2]<6.2f,"Kard, Boomer and Hi-Trax retain distinct original native lengths");
  track_free();feature_world();
  for(int bus=0;bus<AUDIO_COUNT;bus++)g_audio_percent[bus]=100;str_cpy(g_gamedir,"../../../Megarace 2 German",sizeof g_gamedir);
  player().x=car_hw(player());player().v=player().vlong=V_PLAYER*.6;car_physics(player(),0,0,.04,V_PLAYER);scrape_update();Voice *scrape=snd_voice(RC.scrape_voice);
  check(player().wall_scraping&&scrape&&scrape->loop&&scrape->snd==&SFX[S_FROTTE]&&scrape->bus==AUDIO_SOUNDS,"tangential road-edge contact plays the original metallic FROTTE2 loop");
  int handle=RC.scrape_voice;player().x=0;car_physics(player(),0,0,.04,V_PLAYER);scrape_update();check(!snd_voice(handle),"leaving the road edge stops the scrape immediately");
  player().wall_scraping=1;RC.paused=1;scrape_update();check(RC.scrape_voice<0,"pause suppresses the scrape loop");RC.paused=0;
  g_audio_percent[AUDIO_SOUNDS]=0;sound_free(&SFX[S_FROTTE]);scrape_update();check(!SFX[S_FROTTE].pcm&&RC.scrape_voice<0,"muted effects do not load or play the metallic scrape");
  for(int bus=0;bus<AUDIO_COUNT;bus++)g_audio_percent[bus]=0;
  for(int kind=W_KARD;kind<=W_HITRAX;kind++){
    feature_world();feature_barrel_at(0,2);Proj p=feature_missile(kind);player().hitrax_live=1;projectile_step(p,.04);
    check(!p.alive&&!TK.obj[0].alive,"all missile types hit explosive barrels crossed between simulation endpoints");
    if(kind==W_HITRAX)check(!player().hitrax_live,"exploding a barrel releases the active Hi-Trax weapon lock");
    feature_world();Hazard &mine=RC.haz[0];mine.alive=1;mine.kind=W_MINE;mine.pp=pp_make(2,0);mine.arm=10;pp_world(mine.pp,0,0,mine.W);p=feature_missile(kind);projectile_step(p,.04);
    check(!p.alive&&!mine.alive,"all missiles can destroy low unarmed mines under the flight path");
  }
  feature_world();feature_barrel_at(0,5);feature_barrel_at(1,2);Proj p=feature_missile();projectile_step(p,.08);check(!p.alive&&TK.obj[0].alive&&!TK.obj[1].alive,"swept collision chooses the nearest impact independently of object array order");
  feature_world();feature_barrel_at(0,2);feature_barrel_at(1,3);Hazard &mine=RC.haz[0];mine.alive=1;mine.kind=W_MINE;mine.pp=pp_make(4,0);pp_world(mine.pp,0,0,mine.W);p=feature_missile();projectile_step(p,.04);check(!TK.obj[0].alive&&!TK.obj[1].alive&&!mine.alive,"nearby explosive barrels and mines participate in a bounded chain reaction");
  feature_world();feature_barrel_at(0,2);TK.obj[0].W[1]=20000;p=feature_missile();projectile_step(p,.04);check(p.alive&&TK.obj[0].alive,"a barrel on an overhead road is not hit by a missile below it");
  feature_world();feature_barrel_at(0,20);TK.obj[0].W[2]=2*PT;p=feature_missile();projectile_step(p,.04);check(p.alive&&TK.obj[0].alive,"nearby but disconnected road branches are excluded from projectile collisions");
  feature_world();Hazard &oil=RC.haz[0];oil.alive=1;oil.kind=W_OIL;oil.pp=pp_make(2,0);pp_world(oil.pp,0,0,oil.W);p=feature_missile();projectile_step(p,.04);check(p.alive&&oil.alive,"missiles do not explode harmless oil slicks");
  feature_world();feature_barrel_at(0,2);RC.ncar=2;RC.car[1]=player();RC.car[1].player=0;RC.car[1].x=0;RC.car[1].pp=pp_make(3,0);car_update_world(RC.car[1]);explosive_detonate(0,0,0);check(RC.car[1].damage>0&&RC.car[1].stun>0,"destroyed barrel blast damages nearby vehicles");
  M4 ortho=shadow_ortho(v3(-10,-5,-30),v3(10,5,-1));V3 near=transform_point(ortho,v3(-10,-5,-1)),far=transform_point(ortho,v3(10,5,-30));
  check(m_abs(near.x+1)<1e-5f&&m_abs(near.y+1)<1e-5f&&m_abs(near.z+1)<1e-5f&&m_abs(far.x-1)<1e-5f&&m_abs(far.z-1)<1e-5f,"light-space depth projection maps near and far bounds correctly");
  feature_world();g_route_n=1;g_route[0]=2;RC.t=1;RC.fx[0].alive=1;RC.fx[0].scale=1;RC.fx[0].t=.1;RC.fx[0].W[2]=2*PT;RC.fx[0].W[1]=2500;g_modern_explosions=1;g_tex_nearest=0;effects_reset();unsigned old_rng=g_rng,old_fx=g_rng_fx;effects_sync();
  double cam[12]={0,0,0,1,0,0,0,1,0,0,0,1};effects_build(cam,0);check(g_effect_vertex_count==12*24,"modern explosions generate twelve solid 3D debris chunks");
  check(g_rng==old_rng&&g_rng_fx==old_fx,"particle creation never changes gameplay or audio random sequences");
  effects_sync();int events=0;for(int i=0;i<48;i++)events+=g_explosions[i].alive;check(events==1,"repeated renders do not respawn the same explosion");
  RC.t=2.6;effects_build(cam,0);check(!g_effect_vertex_count,"debris fades completely within roughly one and a half seconds");
  effects_build(cam,1);check(g_effect_vertex_count>0,"smoke outlives the short debris flight");RC.t=5;effects_build(cam,1);check(!g_effect_vertex_count,"smoke expires without accumulating permanent particles");
  g_tex_nearest=1;effects_sync();events=0;for(int i=0;i<48;i++)events+=g_explosions[i].alive;check(!events,"Retro clears modern particles and keeps original sprite effects");
  check(damage_stage(69.99)==0&&damage_stage(70)==1&&damage_stage(84.99)==1&&damage_stage(85)==2&&damage_stage(89.99)==2&&damage_stage(90)==3&&damage_stage(99.99)==3&&damage_stage(100)==4,"damage effect thresholds distinguish white smoke, black smoke, fire and total loss exactly");
  feature_world();g_tex_nearest=0;g_damage_effects=1;g_modern_explosions=0;g_cheat_nodmg=0;g_audio_percent[AUDIO_SOUNDS]=100;snd_stop_all_sfx();RC.ncar=8;
  for(int i=0;i<8;i++){RC.car[i]=player();RC.car[i].player=i==0;RC.car[i].damage=99;RC.car[i].out=0;damage_car(RC.car[i],1,-1);check(RC.car[i].out&&RC.car[i].damage==100,"all player and opponent vehicles enter total loss at 100 percent");}
  int deaths=0,sounds=0;for(int i=0;i<24;i++)if(RC.fx[i].alive&&RC.fx[i].scale>=2&&modern_fx(RC.fx[i]))deaths++;for(int i=2;i<NVOICES;i++)if(MX.v[i].active&&MX.v[i].snd==&SFX[S_EXPLO3])sounds++;
  check(deaths==8&&sounds==8,"all eight total losses emit one large damage explosion and original explosion sound even with weapon effects disabled");
  int serial=MX.serial;damage_car(player(),100,-1);check(MX.serial==serial,"an already destroyed car cannot repeatedly explode or replay its death sound");
  feature_world();g_damage_effects=0;snd_stop_all_sfx();player().damage=99;serial=MX.serial;damage_car(player(),1,-1);deaths=0;for(int i=0;i<24;i++)deaths+=RC.fx[i].alive;check(player().out&&!deaths&&MX.serial==serial,"disabling damage effects suppresses Revived death visuals and explosion audio");
  feature_world();g_tex_nearest=1;player().damage=99;damage_car(player(),1,-1);check(RC.fx[0].alive&&RC.fx[0].scale==1.6&&!modern_fx(RC.fx[0]),"Retro retains its original vehicle destruction effect independently of Revived preferences");
  feature_world();g_tex_nearest=0;g_damage_effects=1;g_scene_count=1;g_scene[0]={&feature_barrel,m4_id(),0,0,0};player().damage=95;g_engine_slots[0]=-10000;RC.t=1;engine_effects_sync(cam);int puffs=0;for(int i=0;i<64;i++)puffs+=g_engine_puffs[0][i].alive;check(puffs==1,"one bounded engine emitter sample is created per car and simulation time slot");engine_effects_sync(cam);puffs=0;for(int i=0;i<64;i++)puffs+=g_engine_puffs[0][i].alive;check(puffs==1,"higher render framerates cannot multiply engine particle emission");
  g_damage_effects=0;engine_effects_sync(cam);puffs=0;for(int i=0;i<64;i++)puffs+=g_engine_puffs[0][i].alive;check(!puffs,"disabling damage effects clears existing engine particles");
  feature_world();effects_reset();g_damage_effects=1;g_tex_nearest=0;player().x=car_hw(player());player().v=player().vlong=.6*V_PLAYER;car_physics(player(),0,0,.04,V_PLAYER);check(g_contact_next==1,"actual tangential wall scraping emits damage sparks at the boundary");
  int emitted=g_contact_next;player().x=0;player().spark_cd=0;car_physics(player(),0,0,.04,V_PLAYER);check(g_contact_next==emitted,"driving away from the wall does not emit boundary sparks");
  feature_world();effects_reset();player().x=car_hw(player())+100;player().v=.8*V_PLAYER;player().vlong=.4*V_PLAYER;player().vlat=.6*V_PLAYER;player().psi=1;car_physics(player(),0,0,.04,V_PLAYER);check(g_contact_next==1&&!g_contacts[0].grinding&&!player().wall_scraping,"hard wall impact emits a brief burst without enabling continuous scraping");
  feature_world();effects_reset();RC.ncar=3;RC.car[0].out=1;RC.car[1]=player();RC.car[1].out=0;RC.car[1].player=0;RC.car[1].x=0;RC.car[1].pp=pp_make(2,0);RC.car[1].vlong=.4*V_PLAYER;car_update_world(RC.car[1]);RC.car[2]=RC.car[1];RC.car[2].x=10000;RC.car[2].vlong=0;car_update_world(RC.car[2]);road_plane_frames_build();g_original_defer=1;for(int i=0;i<RC.ncar;i++)car_physics(RC.car[i],0,0,.04,V_PLAYER);g_original_defer=0;original_contacts(.04);check(g_contact_next>0,"opponent-to-opponent sliding contact emits sparks without requiring the player");
  effects_build(cam,2);check(g_effect_vertex_count>0,"contact sparks produce rendered streak geometry");RC.t=1;effects_build(cam,2);check(!g_effect_vertex_count,"contact spark streaks expire within one second");
  g_damage_effects=0;emitted=g_contact_next;double zero[3]={0,0,0},up[3]={0,1,0};contact_sparks(zero,up,zero,1);check(g_contact_next==emitted,"disabling damage effects suppresses spark emission");
  feature_world();effects_reset();g_tex_nearest=0;g_missile_effects=1;g_damage_effects=g_modern_explosions=0;
  double missile_cam[12]={0,0,-40000,1,0,0,0,1,0,0,0,1},right[3]={1,0,0},up_axis[3]={0,1,0},forward[3]={0,0,1},position[3]={0,0,0};
  old_rng=g_rng;old_fx=g_rng_fx;
  for(int kind=W_KARD;kind<=W_HITRAX;kind++){
    effects_reset();RC.t=10;Proj &shot=RC.proj[0];shot=feature_missile(kind);shot.gen=10+kind;
    M4 pose=world_mv(missile_cam,position,right,up_axis,forward,1);missile_effect_pose(missile_cam,0,shot,feature_barrel,pose);
    check(g_missile_smoke_next==1&&g_missile_emitters[0].kind==kind,"each missile kind creates its own nozzle flame and launch smoke");
    double first=g_missile_smoke[0].W[2];RC.t=10.1;pose.m[14]-=20;missile_effect_pose(missile_cam,0,shot,feature_barrel,pose);
    check(g_missile_smoke_next==13,"missile smoke uses 120 fixed world samples per second across simulation steps");
    check(g_missile_smoke[0].W[2]==first&&g_missile_emitters[0].W[2]>first+19000,"old smoke stays in world space as the missile moves forward");
    int count=g_missile_smoke_next;missile_effect_pose(missile_cam,0,shot,feature_barrel,pose);check(g_missile_smoke_next==count,"extra renders at one simulation time do not multiply trail particles");
    g_effect_vertex_count=0;missile_effects_build(missile_cam,2);check(g_effect_vertex_count==6,"a flying missile renders one compact animated jet flame");
    g_effect_vertex_count=0;missile_effects_build(missile_cam,1);check(g_effect_vertex_count>0,"missile smoke generates connected soft trail geometry");
    shot.alive=0;g_effect_vertex_count=0;missile_effects_build(missile_cam,2);check(!g_effect_vertex_count&&missile_effects_active(),"impact stops the nozzle flame immediately while existing smoke lingers");
    shot.alive=1;shot.gen++;missile_effect_pose(missile_cam,0,shot,feature_barrel,pose);check(g_missile_emitters[0].gen==shot.gen&&g_missile_smoke_next>count,"slot reuse starts a new emitter without connecting it to the previous missile");
    shot.alive=0;RC.t=11;check(!missile_effects_active(),"all missile smoke expires within 650 milliseconds after the last sample");
  }
  check(g_rng==old_rng&&g_rng_fx==old_fx,"missile presentation does not consume gameplay or sound random values");
  g_missile_effects=0;RC.t=12;missile_effect_pose(missile_cam,0,RC.proj[0],feature_barrel,m4_id());check(!missile_effects_active()&&g_missile_time<0,"disabled missile effects clear all emitters and skip new work");
  g_missile_effects=1;g_tex_nearest=1;missile_effect_pose(missile_cam,0,RC.proj[0],feature_barrel,m4_id());check(!missile_effects_active(),"Retro never allocates or draws Revive missile exhaust");
  g_tex_nearest=0;G.missile_effects=1;graphics_mode_set(0);setup_save();G.missile_effects=0;setup_load();check(G.missile_effects&&g_missile_effects,"the dedicated missile effect preference survives a config save/load");graphics_mode_set(1);check(!g_missile_effects&&G.missile_effects,"Retro suppresses missile effects while retaining the saved Revive preference");graphics_mode_set(0);
  feature_world();effects_reset();g_modern_explosions=1;g_damage_effects=0;g_tex_nearest=0;g_route_n=1;g_route[0]=2;RC.t=1;RC.fx[0].alive=1;RC.fx[0].model=OM_EXP_BIG;RC.fx[0].scale=1.4;RC.fx[0].t=.1;RC.fx[0].W[2]=2*PT;RC.fx[0].W[1]=2500;effects_sync();effects_build(cam,0);check(g_explosions[0].hazard&&g_effect_vertex_count==48*24,"explosive barrels produce forty-eight flying solid debris chunks independently of damage effects");
  effects_build(cam,4);int blast_core=0,shock_ring=0;for(int i=0;i<g_effect_vertex_count;i++){blast_core+=g_effect_vertices[i].kind==9;shock_ring+=g_effect_vertices[i].kind==10;}check(blast_core>=6&&shock_ring>=6,"explosive hazard effects include a bright expanding blast core and ground shock ring");
  RC.t=3;effects_build(cam,0);check(!g_effect_vertex_count,"intense hazard debris still fades shortly after its flight");
  effects_reset();RC.t=1;RC.fx[0].scale=1.3;RC.fx[0].t=.1;effects_sync();check(g_explosions[0].hazard,"mines use the same intense explosive blast profile rather than an ordinary flame");
  u8 cluster_mask[8*8*4]={};const int isolated[]={0,5*8+7,6*8};for(int i=0;i<3;i++)cluster_mask[isolated[i]*4]=255;
  const int kept[]={2,3,10,11},tiny[]={29,30,38};for(int i=0;i<4;i++)cluster_mask[kept[i]*4]=255;for(int i=0;i<3;i++)cluster_mask[tiny[i]*4]=255;cluster_mask[1]=255;lamp_cluster_filter(cluster_mask,8,8);
  check(!cluster_mask[0]&&!cluster_mask[(5*8+7)*4]&&!cluster_mask[6*8*4]&&!cluster_mask[29*4]&&!cluster_mask[30*4]&&!cluster_mask[38*4],"red singleton/three-pixel speckles and row-wrapped neighbours are rejected");
  check(cluster_mask[2*4]&&cluster_mask[3*4]&&cluster_mask[10*4]&&cluster_mask[11*4]&&cluster_mask[1],"four connected red pixels survive without changing yellow marker pixels");
  G.gfx_mode=0;effect_defaults();check(cfg_items(items)<=CFG_MAX_ITEMS,"six independent intensities fit the existing config table");
  for(int i=0;i<6;i++){check(G.effect_intensity[i]==100&&*effect_switches(i),"reset restores enabled effects at the exact original 100-percent defaults");effect_intensity_set(i,250);check(G.effect_intensity[i]==200&&g_effect_strength[i]==2,"all effect sliders clamp their maximum to 200 percent");effect_intensity_set(i,-5);check(!G.effect_intensity[i]&&!g_effect_strength[i],"all effect sliders clamp at zero without affecting simulation values");}
  effect_defaults();G.state=GS_REVIVE_EFFECTS;G.sel=1;G.click=0;Input effect_key={};effect_key.pressed[K_RIGHT]=1;screen_revive_effects(&effect_key);check(G.effect_intensity[1]==105&&G.sel==1,"keyboard right adjusts intensity while keeping the selected effect");
  effect_key={};effect_key.lmb=1;G.click=1;G.mx=455;G.my=76;screen_revive_effects(&effect_key);check(G.effect_intensity[0]==100&&G.real_time_shadows,"clicking a slider changes intensity without toggling its checkbox");G.click=0;G.mx=545;screen_revive_effects(&effect_key);check(G.effect_intensity[0]==200,"holding and dragging the intensity slider reaches its maximum");effect_key={};screen_revive_effects(&effect_key);check(g_effect_drag<0,"releasing the mouse stops intensity dragging");
  G.sel=6;effect_key.pressed[K_ENTER]=1;screen_revive_effects(&effect_key);check(G.effect_intensity[0]==100&&G.effect_intensity[1]==100,"the reset button restores tuned defaults through the real input handler");
  G.effect_intensity[3]=137;setup_save();G.effect_intensity[3]=0;setup_load();check(G.effect_intensity[3]==137,"intensity preferences survive config save/load");graphics_mode_set(1);check(!g_modern_explosions&&!g_vehicle_lights&&G.effect_intensity[3]==137,"Retro suppresses effects without discarding intensity preferences");graphics_mode_set(0);effect_defaults();
  int fire_w,fire_h,fire_channels;u8 *fire=stbi_load_from_memory(FIRE_ATLAS_PNG,sizeof FIRE_ATLAS_PNG,&fire_w,&fire_h,&fire_channels,4);check(fire&&fire_w==512&&fire_h==512,"embedded realistic flame atlas decodes on the actual runtime PNG path");
  if(fire){for(int tile=0;tile<4;tile++){int colored=0,transparent=0,partial=0;for(int y=0;y<256;y++)for(int x=0;x<256;x++){const u8 *p=fire+(((tile/2*256+y)*512)+(tile%2*256+x))*4;colored+=p[0]>100&&p[3]>128;transparent+=p[3]==0;partial+=p[3]>0&&p[3]<255;}check(colored>100&&transparent>100&&partial>100,"every flame variation has fire detail, clear background and smooth alpha edges");}sys_free(fire);}
  g_tex_nearest=0;effects_reset();mem_zero(&TK,sizeof TK);mem_zero(&RC,sizeof RC);g_scene_count=0;
  for(int bus=0;bus<AUDIO_COUNT;bus++)g_audio_percent[bus]=100;gf_language=2;setup_load();
}
