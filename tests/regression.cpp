// Exercises the actual core and Win32 platform, without creating a window.
#ifdef _WIN32
#define WinMainCRTStartup original_gui_entry
#include "../src/win32_main.cpp"
#undef WinMainCRTStartup
#else
#include "platform_stdio.inc"
#endif
#include "../src/core.cpp"
#define TEST_STUB(ret,name,args) static ret test_##name args { return (ret)0; }
GL_FUNCS(TEST_STUB)
#undef TEST_STUB

static int failures, assertions;
static unsigned verification_reads;
static void count_verification(const char *,uint64_t,uint64_t){verification_reads++;}
static int no_required_files(const char *){return 0;}
static char report[16000];
static void check(int condition, const char *name) {
  assertions++;
  if (!condition) { failures++; s_cat(report, "FAIL: ", sizeof report); s_cat(report, name, sizeof report); s_cat(report, "\r\n", sizeof report); }
}

#include "audio_fonts.inc"
#include "engine_retro.inc"
#include "startup_dos.inc"
#include "gameplay_graphics.inc"
#include "remastered_ui.inc"
#include "controls_0713.inc"
#include "revision_0714.inc"
#include "revision_0715.inc"
extern "C" void WinMainCRTStartup(void) {
  g_heap = GetProcessHeap();
#define USE_EARLY_STUB(ret,name,args) name=test_##name;
  GL_FUNCS(USE_EARLY_STUB)
#undef USE_EARLY_STUB
  Sha256 sha;char digest[65];sha.init();sha.finish(digest);
  check(!str_icmp(digest,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),"SHA256 empty known answer");
  sha.init();sha.update((const uint8_t *)"abc",3);sha.finish(digest);
  check(!str_icmp(digest,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),"SHA256 abc known answer");
  char parsed_path[600];
  static const char quoted[]="\xef\xbb\xbf" "game_directory = \"C:/Example/Game Folder\"\r\nspielverzeichnis = Wrong\n";
  check(game_directory_from_config((const unsigned char *)quoted,sizeof quoted-1,parsed_path,sizeof parsed_path) && !str_icmp(parsed_path,"C:/Example/Game Folder"),"BOM, quoted paths, spaces and English key precedence");
  static const char legacy[]="  spielverzeichnis = 'Legacy Folder'\n";
  check(game_directory_from_config((const unsigned char *)legacy,sizeof legacy-1,parsed_path,sizeof parsed_path) && !str_icmp(parsed_path,"Legacy Folder"),"legacy quoted path remains readable");
  check(!game_directory_from_config((const unsigned char *)legacy,sizeof legacy-1,parsed_path,3),"overlong directory is rejected rather than truncated");
#ifdef _WIN32
  static const char saved_game[]="game_directory = \"../../../Megarace 2 English\"\n";
  sys_write_file("revived_config/openmr2_setup.cfg",saved_game,sizeof saved_game-1);
  check(gd_find("../../../Megarace 2 German",parsed_path,sizeof parsed_path) && gf_language==2,"local complete installation wins over saved English data");
  check(gd_find("validation-empty",parsed_path,sizeof parsed_path) && gf_language==0,"saved English data resolves without a dialog");
  gd_cfg_write(parsed_path);setup_load();setup_save();
  check(gd_find("validation-empty",parsed_path,sizeof parsed_path) && gf_language==0,"saved path survives settings rewrite and resolves on next launch");
#endif
  {const char *path="../../../Megarace 2 English";char details[4000];
    cfg_store_verification(path,0,1);verification_reads=0;sha256_progress=count_verification;
    check(!gf_startup_check(path,gd_exists,details,sizeof details) && gf_language==0,"Files_OK true restores verified edition");
    check(!verification_reads,"Files_OK true performs no hashing at all");
    check(cfg_verified_language("another-folder")<0,"verification is bound to the selected path");
    cfg_store_verification(path,0,0);check(cfg_verified_language(path)<0,"Files_OK false requires verification");
    check(!gf_startup_check(path,gd_exists,details,sizeof details) && verification_reads>0,"Files_OK false runs a full verification");
    check(cfg_verified_language(path)==0,"successful one-time verification persists Files_OK true");
    verification_reads=0;check(!gf_startup_candidate(path,no_required_files),"cached installation with missing files is rejected");
    check(cfg_verified_language(path)<0 && !verification_reads,"missing files set Files_OK false without hashing");
    sha256_progress=0;
  }
  MX.sfx_vol = 1;
  static int16_t pcm[32];
  Sound sound = {pcm, 32, 1};
  // Dedicated race loops must stop, leaving music, video and unrelated sounds alone.
  for (int cycle = 0; cycle < 3; cycle++) {
    music_play(&sound, 1); voice_play(&sound, 1);
    RC.eng_voice = snd_play(&sound, 1, 1, 1, 0);
    RC.eng2_voice = snd_play(&sound, 1, 1, 1, 0);
    RC.scrape_voice = snd_play(&sound, 1, 1, 1, 0);
    int other = snd_play(&sound, 1, 1, 0, 0);
    int e1 = RC.eng_voice, e2 = RC.eng2_voice, sc = RC.scrape_voice;
    check(snd_voice(e1) && snd_voice(e2), "new race loops start");
    race_end_sounds(); race_end_sounds();
    check(!snd_voice(e1) && !snd_voice(e2) && !snd_voice(sc), "race loops stop idempotently");
    check(MX.v[0].active && MX.v[1].active && snd_voice(other), "other audio survives");
    check(RC.eng_voice == -1 && RC.eng2_voice == -1 && RC.scrape_voice == -1, "handles invalidated");
    snd_stop_all_sfx();
  }
  // Stale handles must not stop a voice that reused the same mixer slot.
  int old = snd_play(&sound, 1, 1, 1, 0); snd_stop(old);
  int fresh = snd_play(&sound, 1, 1, 0, 0);
  RC.eng_voice = old; RC.eng2_voice = RC.scrape_voice = -1; race_end_sounds();
  check(snd_voice(fresh) != 0, "recycled channel remains active");
  snd_stop_all_sfx();
  // A state transition off the track is sufficient, even when race_finished was bypassed.
  G.state = GS_RACE; RC.eng_voice = snd_play(&sound, 1, 1, 1, 0);
  int e1 = RC.eng_voice; go(GS_MESSAGE);
  check(!snd_voice(e1), "leaving GS_RACE stops loops");
  // Original/Retro HUD pixels must still match their source indices exactly.
  g_revived_ui=0;
  g_cv = (uint32_t *)sys_alloc(CW * CH * 4);
  const char *languages[] = {"English", "French", "German"};
  static char video_hashes[3][3][65];
  for (int lang = 0; lang < 3; lang++) {
    char dir[600] = "../../../Megarace 2 "; s_cat(dir, languages[lang], 600);
    char path[700]; s_cpy(path, dir, 700); s_cat(path, "/MEGARACE2.gog", 700);
    char details[4000];check(!gd_check(dir,details,sizeof details) && gf_status==GOG_COMPLETE && gf_language==lang,"complete GOG install correctly identified");
    check(iso_open(path), "GOG image opens");
    s_cpy(path, dir, 700); s_cat(path, "/M.EXE", 700);
    check(mexe_load(path), "original decoder tables load");
    size_t vn=0;uint8_t *vd=iso_load("TRACK1/FACTO.UBB",&vn);Video video;
    check(vd && video_open(&video,vd,vn,350),"original track video opens");
    const int sample_frames[]={0,150,30};
    for(int sample=0;sample<3;sample++){
      check(video_decode(&video,sample_frames[sample]),"original track video decodes and seeks");
      Sha256 sh;sh.init();sh.update(video.rgba,(size_t)video.w*video.h*4);sh.finish(video_hashes[lang][sample]);
      if(lang)check(!str_icmp(video_hashes[lang][sample],video_hashes[0][sample]),"identical decoded scenery across three GOG languages");
    }
    video_close(&video);
    check(spr_load_cd(&TK.hud, "TRACK1/FACTO.SPR") && TK.hud.n == 6, "six original HUD sprites load");
    size_t n = 0; uint8_t *d = iso_load("TRACK1/FACTO.HLZ", &n);
    check(d && n >= 768, "track HUD palette loads");
    if (d && n >= 768) mem_cpy(TK.hud_pal, d, 768);
    sys_free(d);
    s_cpy(path,dir,sizeof path);s_cat(path,"/DATA640/PAL_RACE.HLZ",sizeof path);d=sys_read_file(path,&n);
    check(d && n>=768,"reserved HUD colors load");if(d && n>=768)mem_cpy(TK.hud_pal+753,d+753,15);sys_free(d);
    for (int icon = 0; icon < TK.hud.n; icon++) {
      mem_zero(g_cv, CW * CH * 4); cv_armes(icon, 14, 35);
      Sprite &s = TK.hud.f[icon];
      for (int y = 0; y < (int)(s.h * 1.2); y++) for (int x = 0; x < s.w * 2; x++) {
        int c = s.px[(int)(y / 1.2) * s.w + x / 2];
        check(g_cv[(35+y)*CW+14+x] == (c ? pal_rgba(TK.hud_pal, c) : 0), "HUD pixel matches original sprite");
      }
    }
    spr_free(&TK.hud);
  }
  g_revived_ui=1;
  gf_language=2; // This fixture explicitly uses the edition already verified above.
  str_cpy(g_gamedir,"../../../Megarace 2 German",sizeof g_gamedir);
  Font check_font={};check(font_load(&check_font,"TEXTE/HNDEL14.HLZ"),"original menu font loads for translated layout checks");
  const char *layout_keys[]={"Controller belegen","Debugmen\xfc","Spielstand zur\xfc" "cksetzen","Rennen laden","Dein Rennen"};
  const int layout_widths[]={255,260,300,270,270};
  for(int language=0;language<3;language++){
    gf_language=language;
    for(unsigned i=0;i<sizeof TRANSLATIONS/sizeof TRANSLATIONS[0];i++){
      const Translation &row=TRANSLATIONS[i];const char *expected=language==2?row.de:language==1?row.fr:row.en;
      check(expected[0] && !str_icmp(ui_translate(row.key),expected),"catalog entry resolves in every supported language");
    }
    for(unsigned i=0;i<sizeof layout_keys/sizeof layout_keys[0];i++)check(cv_text_w(check_font,layout_keys[i],1)<=layout_widths[i],"translated label fits its actual menu region");
    check(!str_icmp(ui_translate("D:/My Games/Player Name"),"D:/My Games/Player Name"),"paths and unknown user content remain unchanged");
  }
  pic_free(&check_font.pic);gf_language=2;
  // Damage/armor bars: empty, half, full, clamped overflow; reserved palette must be applied.
  mem_zero(g_cv,CW*CH*4);cv_bar(400,42,0,1);
  check(g_cv[42*CW+400]==rgba(255,255,255,255),"bar has thin light border");
  check(g_cv[44*CW+404]==rgba(255,0,0,255),"first bar segments red");
  check(g_cv[44*CW+424]==rgba(255,255,255,255),"remaining bar segments light");
  check(g_cv[44*CW+464]==rgba(255,255,255,255),"full new hull has sixteenth segment (65536 raw)");
  cv_bar(160,61,1,1);check(g_cv[63*CW+224]==rgba(0,0,0,255),"full shield has fifteen segments (65535 raw)");
  cv_bar(400,42,0,0);check(g_cv[44*CW+404]==rgba(0,0,0,255),"empty bar has no remaining fill");
  // Only disposable copies below build/validation-sandbox are changed.
  size_t source_size=0;uint8_t *exe=sys_read_file("../../../Megarace 2 German/M.EXE",&source_size);
  check(exe!=0,"read original executable without modifying it");
  if(exe){
    check(sys_write_file("validation-sandbox/M.EXE",exe,source_size),"write disposable executable copy");
    char details[16000];check(gd_check("validation-sandbox",details,sizeof details)>0 && gf_status==GOG_INCOMPLETE && gf_language==2,"known incomplete copy rejected");
    exe[source_size-1]^=1;sys_write_file("validation-sandbox/M.EXE",exe,source_size);
    check(gd_check("validation-sandbox",details,sizeof details)>0 && gf_status==GOG_UNKNOWN,"modified unknown version rejected");sys_free(exe);
  }
  // English config keys win over legacy aliases independent of line order.
  static const char config[]="frame_rate = unlimited\ncheat_money = 1\nbildausgabe_rennen = 0\ndebug_bildausgabe = 1\ncheat_geld = 0\ngame_directory = C:/Example/Unchanged Path\n";
  sys_write_file("revived_config/openmr2_setup.cfg",config,sizeof config-1);setup_load();
  check(G.dbg_fpsmode==1 && G.cheat_career.money==1,"canonical keys override legacy aliases");
  check(!str_icmp(g_cfg_gamedir,"C:/Example/Unchanged Path"),"custom path preserved");
  setup_save();size_t config_size=0;uint8_t *saved=sys_read_file("revived_config/openmr2_setup.cfg",&config_size);
  check(saved && str_icontains((char *)saved,"frame_rate = 60fps") && !str_icontains((char *)saved,"bildausgabe"),"config saves English symbolic frame rate");sys_free(saved);
  for(int mode=0;mode<2;mode++){
    G.dbg_fpsmode=mode;debug_apply();check(g_out_mode==mode,"only two output modes are applied");
  }
  static const char old_interpolation[]="frame_rate = 60fps\ninterpolation = 0\n";
  sys_write_file("revived_config/openmr2_setup.cfg",old_interpolation,sizeof old_interpolation-1);setup_load();setup_save();
  saved=sys_read_file("revived_config/openmr2_setup.cfg",&config_size);
  check(G.dbg_fpsmode==1 && saved && !str_icontains((char *)saved,"interpolation"),"obsolete interpolation toggle removed while 60fps is retained");sys_free(saved);
  // Production mode switching preserves independent preferences across a saved Retro session.
  G.gfx_mode=0;G.revived_saved=0;G.opts_gfx=0;G.gfx_msaa=8;G.gfx_fxaa=1;G.dbg_fpsmode=1;
  graphics_mode_set(1);graphics_mode_set(1);
  check(G.opts_gfx==1 && !G.gfx_msaa && !G.gfx_fxaa && !G.dbg_fpsmode && !g_out_mode,"Retro disables filters/AA/60fps and interpolation");
  check(G.revived_saved && G.revived_filter==0 && G.revived_msaa==8 && G.revived_fxaa==1 && G.revived_fps==1,"repeated Retro activation does not overwrite the Revived snapshot");
  check(VIDEO_Y0==30 && VIDEO_H==420,"350 original video lines occupy 350 of the 400 native race lines");
  const int locked_rows[]={4,5,6,8};
  for(int row:locked_rows){Input key={};key.pressed[K_ENTER]=1;G.sel=row;G.click=0;screen_gfx(&key);
    key={};key.pressed[K_RIGHT]=1;screen_gfx(&key);
    check(G.opts_gfx==1 && !G.gfx_msaa && !G.gfx_fxaa && !G.dbg_fpsmode,"Retro graphics rows reject keyboard activation");
    key={};G.mx=45;G.my=46+row*30;G.click=1;screen_gfx(&key);G.click=0;
    check(G.opts_gfx==1 && !G.gfx_msaa && !G.gfx_fxaa && !G.dbg_fpsmode,"Retro graphics rows reject mouse activation");
  }
  {Input key={};key.pressed[K_DOWN]=1;G.sel=4;screen_gfx(&key);check(G.sel==0,"Retro graphics navigation wraps after its four supported options and Apply");}
  setup_save();setup_load();
  check(G.gfx_mode==1 && G.revived_saved && !G.gfx_msaa && G.revived_msaa==8 && G.revived_fps==1,"Retro snapshot survives settings save and process-style reload");
  graphics_mode_set(0);
  check(G.opts_gfx==0 && G.gfx_msaa==8 && G.gfx_fxaa==1 && G.dbg_fpsmode==1 && g_out_mode==1 && !G.revived_saved,"switching back restores exactly the Revived settings");
  G.opts_gfx=1;G.gfx_msaa=0;G.gfx_fxaa=0;G.dbg_fpsmode=0;graphics_mode_set(1);graphics_mode_set(0);
  check(G.opts_gfx==1 && !G.gfx_msaa && !G.gfx_fxaa && !G.dbg_fpsmode,"Revived options originally disabled remain disabled after a Retro round trip");
  // Execute the real race core against the supplied German Foundry assets. Rendering
  // calls are stubs here; video decoding, camera timing and the simulation are real.
#define USE_STUB(ret,name,args) name=test_##name;
  GL_FUNCS(USE_STUB)
#undef USE_STUB
  gf_language=2; // This fixture explicitly uses the edition already verified above.
  str_cpy(g_gamedir,"../../../Megarace 2 German",sizeof g_gamedir);
  check(iso_open("../../../Megarace 2 German/MEGARACE2.gog") && mexe_load("../../../Megarace 2 German/M.EXE"),"race fixture opens actual original data");
  static RCar reference_cars[8];static TrackObj reference_objects[MAXOBJ];unsigned reference_rng=0;int reference_updates=0;
  const int rates[]={25,60,240};
  A.start_frame=150;A.objtest=0;g_sim_model=0;g_ai_rescue=1;g_cheat_ammo=g_cheat_nodmg=0;
  for(int mode=0;mode<3;mode++){
    race_end_sounds();check(track_load(0,0),"real Foundry track loads for pacing test");
    Loadout lo;default_loadout(&lo);int opp[]={0,1,3,4,5,6,7};g_rng=12345;g_rng_fx=54321;g_out_mode=mode?1:0;
    race_setup(2,lo,0,opp,7,7,5);Input input={};input.down[K_UP]=1;
    int frames=rates[mode]*10;double prior=0;
    for(int frame=1;frame<=frames;frame++){double now=10.0*frame/frames;race_advance(&input,now-prior);prior=now;}
    race_advance(&input,0.000001);
    if(!mode){mem_cpy(reference_cars,RC.car,sizeof reference_cars);mem_cpy(reference_objects,TK.obj,sizeof reference_objects);reference_rng=g_rng;reference_updates=RC.upd_n;}
    else{
      check(RC.upd_n==reference_updates && g_rng==reference_rng,"same simulation update count and RNG across frame rates");
      int same=1;const uint8_t *a=(const uint8_t *)reference_cars,*b=(const uint8_t *)RC.car;for(size_t k=0;k<sizeof reference_cars;k++)if(a[k]!=b[k])same=0;check(same,"identical complete vehicle state at 25/60/240 Hz");
      same=1;a=(const uint8_t *)reference_objects;b=(const uint8_t *)TK.obj;for(size_t k=0;k<sizeof reference_objects;k++)if(a[k]!=b[k])same=0;check(same,"identical road-object state at 25/60/240 Hz");
    }
  }
  // Replacement identities, actual PNG round-trip, transparency and Retro isolation.
  {char key[512];texture_key(key,"CARS/FAC/CAISSE30.HLZ",0);check(!str_icmp(key,"LOCAL/CARS/FAC/CAISSE30.HLZ"),"texture identity preserves original asset directories");
    char disc[512];texture_key(disc,"CARS/FAC/CAISSE30.HLZ",1);check(str_icmp(disc,key)!=0,"disc and loose textures cannot alias");texture_key(disc,"../escape.HLZ",0);check(!disc[0],"texture identities reject traversal");
    u8 pal1[768]={},pal2[768]={};pal2[300]=1;char one[512],two[512];texture_sprite_key(one,"LOCAL/DATA640/VAL.SPR",0,pal1);texture_sprite_key(two,"LOCAL/DATA640/VAL.SPR",0,pal2);check(str_icmp(one,two)!=0,"sprite palette variants have distinct complete identities");
    texture_sprite_key(two,"LOCAL/DATA640/VAL.SPR",1,pal1);check(str_icmp(one,two)!=0,"sprite frames cannot alias");
    u8 pixels[]={200,90,40,0,10,20,30,128,1,2,3,255,255,200,100,64};
    check(png_write("revived_config/texture_replacements/LOCAL/TEST/ALPHA.HLZ.png",pixels,2,2),"PNG writer creates nested replacement directories");
    int w=0,h=0;u8 *roundtrip=png_read("revived_config/texture_replacements/LOCAL/TEST/ALPHA.HLZ.png",&w,&h);int same=roundtrip&&w==2&&h==2;if(same)for(int i=0;i<16;i++)if(roundtrip[i]!=pixels[i])same=0;check(same,"PNG round-trip retains RGB beneath zero alpha and partial alpha exactly");sys_free(roundtrip);
    g_custom_textures=0;check(!texture_custom("LOCAL/TEST/ALPHA.HLZ.png"),"disabled replacements perform no lookup or load");
    G.gfx_mode=0;G.revived_saved=0;G.custom_textures=1;texture_options_apply();check(g_custom_textures && texture_custom("LOCAL/TEST/ALPHA.HLZ.png"),"Revived loads an exact replacement path");
    graphics_mode_set(1);check(!g_custom_textures && G.custom_textures && graphics_option_locked(11),"Retro suppresses replacements and retains the Revived preference");
    graphics_mode_set(0);check(g_custom_textures && G.custom_textures,"returning to Revived restores custom textures");
    check(!texture_custom("LOCAL/TEST/MISSING.HLZ.png"),"missing replacement uses the original");sys_write_file("revived_config/texture_replacements/LOCAL/TEST/BAD.HLZ.png","invalid",7);check(!texture_custom("LOCAL/TEST/BAD.HLZ.png"),"invalid PNG safely falls back to original");
    G.custom_textures=0;texture_options_apply();texture_cache_clear();g_texture_reload=0;sys_delete_file("revived_config/texture_replacements/LOCAL/TEST/ALPHA.HLZ.png");sys_delete_file("revived_config/texture_replacements/LOCAL/TEST/BAD.HLZ.png");
  }
  // Reserved model palette slots are markers in the texture HLZ, not display colors.
  for(int language=0;language<3;language++){
    char folder[600]="../../../Megarace 2 ";str_cat(folder,languages[language],sizeof folder);str_cpy(g_gamedir,folder,sizeof g_gamedir);g_model_reserved_ready=0;
    check(model_palette_ensure(),"load original shared model palette");
    check(g_model_reserved[0]==211 && !g_model_reserved[1] && !g_model_reserved[2] && g_model_reserved[3]==255 && g_model_reserved[4]==255 && !g_model_reserved[5],"original reserved slots identify red and yellow accents");
    for(int world=0;world<7;world++){
      char directory[700];str_cpy(directory,folder,sizeof directory);if(world<6){str_cat(directory,"/CARS/",sizeof directory);str_cat(directory,TRACKS[world].set,sizeof directory);}else str_cat(directory,"/OBJS/TIB",sizeof directory);
      struct TextureNames{char names[96][64];int count;} names={};
      sys_list_dir(directory,".HLZ",[](void *u,const char *name,int){TextureNames *t=(TextureNames *)u;if(t->count<96)str_cpy(t->names[t->count++],name,64);},&names);
      check(names.count>0 && names.count<96,"enumerate all vehicle textures for this world/language");
      for(int texture=0;texture<names.count;texture++){
        char path[800];str_cpy(path,directory,sizeof path);str_cat(path,"/",sizeof path);str_cat(path,names.names[texture],sizeof path);
        Image raw={},corrected={};int loaded=image_load_hlz(path,&raw) && image_load_hlz(path,&corrected,g_model_reserved);check(loaded,"every vehicle texture loads with its shared palette");
        if(loaded){size_t file_size=0;u8 *file=sys_read_file(path,&file_size);size_t pixels=(size_t)raw.w*raw.h;u8 *indices=(u8 *)sys_alloc(pixels+16);
          int decoded=file && hlz_unpack(file,file_size,772,indices,pixels)==(int)pixels;int same=decoded;
          if(decoded)for(size_t pixel=0;pixel<pixels;pixel++){
            const u8 *a=raw.rgba+pixel*4,*b=corrected.rgba+pixel*4;int index=indices[pixel];const u8 *expected=a;if(index==248 && raw.pal[744]==128 && raw.pal[745]==0 && raw.pal[746]==128)expected=g_model_reserved;else if(index==249 && raw.pal[747]==0 && raw.pal[748]==252 && raw.pal[749]==0)expected=g_model_reserved+3;
            if(b[0]!=expected[0]||b[1]!=expected[1]||b[2]!=expected[2])same=0;
          }
          if(world<6 && !str_icmp(names.names[texture],"BID.HLZ") && raw.pal[750]==255 && raw.pal[751]==255 && raw.pal[752]==255){int white=0;for(size_t pixel=0;pixel<pixels;pixel++)if(indices[pixel]==250){white++;if(corrected.rgba[pixel*4]!=255 || corrected.rgba[pixel*4+1]!=255 || corrected.rgba[pixel*4+2]!=255)same=0;}check(white>0 && same,"original barrel skull index 250 stays white in every world/edition");}
          check(same,"all reserved palette slots are correct; every non-marker pixel, including object whites is unchanged");sys_free(file);sys_free(indices);
        }
        sys_free(raw.rgba);sys_free(corrected.rgba);
      }
    }
  }

  {M4 projection=turntable_projection(1.9f);check(projection.m[0]<0 && projection.m[5]>0,"shop camera corrects horizontal handedness without inverting vertical orientation");
    double slow=0,fast=0;for(int i=0;i<25;i++)slow=turntable_advance(slow,1.0/25);for(int i=0;i<60;i++)fast=turntable_advance(fast,1.0/60);
    check(d_abs(slow-(6.2831853071795864769-2.1))<1e-9 && d_abs(fast-slow)<1e-9,"turntable rotates counterclockwise at the same speed at 25/60 Hz");
    check(turntable_advance(.1,.1)>6.1 && turntable_advance(.1,.1)<6.2831853071795864769,"counterclockwise turntable wraps through zero");}
  str_cpy(g_gamedir,"../../../Megarace 2 German",sizeof g_gamedir);g_model_reserved_ready=0;
  // Real looping engine WAVs: game over and subsequent race setup.
  Loadout restart_loadout;default_loadout(&restart_loadout);int restart_opponents[]={0,1,3,4,5,6,7};
  MX.sfx_vol=1;sfx_load_all();race_setup(2,restart_loadout,0,restart_opponents,7,7,5);
  int real_engine=RC.eng_voice;check(snd_voice(real_engine)!=0,"original engine WAV loop starts");
  G.state=GS_RACE;PF.single=1;go(GS_GAMEOVER);check(!snd_voice(real_engine),"actual game-over transition stops original engine WAV");
  race_setup(2,restart_loadout,0,restart_opponents,7,7,5);check(snd_voice(RC.eng_voice)!=0,"original engine WAV restarts after game over");
  // Reproduce a lost RC handle (e.g. a replay reset), leaving actual engine samples active.
  int orphan=snd_play(&SFX[S_ENGINE],1,1,1,0);RC.eng_voice=RC.eng2_voice=RC.scrape_voice=-1;race_end_sounds();
  check(!snd_voice(orphan),"sample-based cleanup stops engines even after their handle is lost");
  int ghost1=snd_play(&SFX[S_ENGINE],1,1,1,0),ghost2=snd_play(&SFX[S_ENGINE2],1,1,1,0);
  RC.eng_voice=RC.eng2_voice=-1;music_play(&SFX[S_ENGINE],1);voice_play(&SFX[S_ENGINE2],1);G.menu_music_on=0;
  G.state=GS_CLIP;go(GS_GAMEOVER);
  check(!snd_voice(ghost1) && !snd_voice(ghost2) && !MX.v[0].active && !MX.v[1].active,"Game Over clears orphan loops, track music and leftover video audio");
  int16_t mixed[1024];int late_nonzero=0;
  for(int block=0;block<220;block++){app_audio_fill(mixed,512);if(block>150)for(int k=0;k<1024;k++)if(mixed[k])late_nonzero=1;}
  check(!late_nonzero,"Game Over mixer is silent after its finite entry effect finishes");
  race_end_sounds();track_free();
  // One-shot export action: repeated activation, view changes, completion and exact Apply sample.
  G.opts_menusfx=1;G.state=GS_DEBUG;G.sel=9;G.kbd=1;G.click=0;Input export_press={};export_press.pressed[K_ENTER]=1;
  screen_debug(&export_press);check(g_dump_active && !g_dump_complete && g_dump_assets.n>500,"Dump textures action starts one complete catalog job");
  texture_dump_step();int exported_index=g_dump_index;screen_debug(&export_press);
  check(g_dump_active && g_dump_index==exported_index,"clicking Dump textures again does not stop or restart it");
  G.sel=14;screen_debug(&export_press);check(g_dump_active,"resetting debug options does not cancel a running export");
  A.mode=MODE_VIEWER;A.export_textures=0;texture_dump_frame_tick();check(g_dump_index>exported_index,"export ticks independently of selected game/viewer mode");
  g_dump_index=g_dump_assets.n/2;texture_dump_overlay_draw(640,480);
  check(g_cv[426*CW+180]==ORANGE && g_cv[426*CW+460]==rgba(20,18,16,255) && g_cv[418*CW+168]==rgba(150,140,120,255),"export overlay uses the shared track loading bar with correct partial fill and frame");
  snd_stop_all_sfx();g_dump_index=g_dump_assets.n;int serial_before=MX.serial;texture_dump_step();int apply_playing=0;
  for(int i=2;i<NVOICES;i++)if(MX.v[i].active && MX.v[i].snd==&SFX[S_MENU2])apply_playing++;
  check(g_dump_complete && !g_dump_active && apply_playing==1 && MX.serial==serial_before+1,"completion stops the job and plays exactly the Apply menu sound once");
  texture_dump_step();check(MX.serial==serial_before+1,"finished export never repeats the completion sound");
  setup_save();char cfg_file[512];cfg_path(cfg_file,"openmr2_setup.cfg");size_t saved_n=0;u8 *saved_cfg=sys_read_file(cfg_file,&saved_n);
  check(saved_cfg && !str_icontains((char *)saved_cfg,"dump_textures"),"one-shot export has no persisted on/off setting");
  char legacy_cfg[10000]="";if(saved_cfg)str_cpy(legacy_cfg,(char *)saved_cfg,sizeof legacy_cfg);str_cat(legacy_cfg,"\ndump_textures = 1\n",sizeof legacy_cfg);sys_free(saved_cfg);sys_write_file(cfg_file,legacy_cfg,str_len(legacy_cfg));setup_load();check(!g_dump_active,"legacy dump checkbox config cannot restart exporting on launch");
  g_dump_overlay_until=0;snd_stop_all_sfx();
  // Repair only pixel-identical legacy exports; edited dump files and replacements are untouched.
  const u8 old_green[]={0,255,0,255},correct_white[]={255,255,255,255},edited_blue[]={0,0,255,255};
  const char *repair_path="revived_config/texture_dumps/LOCAL/TEST/LEGACY.HLZ.png";
  png_write(repair_path,old_green,1,1);texture_dump_rgba("LOCAL/TEST/LEGACY.HLZ.png",correct_white,1,1,"white",old_green);int pw=0,ph=0;u8 *fixed=png_read(repair_path,&pw,&ph);
  check(fixed && pw==1 && ph==1 && fixed[0]==255 && fixed[1]==255 && fixed[2]==255,"unchanged legacy green export is refreshed to corrected white");sys_free(fixed);
  png_write(repair_path,edited_blue,1,1);texture_dump_rgba("LOCAL/TEST/LEGACY.HLZ.png",correct_white,1,1,"white",old_green);fixed=png_read(repair_path,&pw,&ph);
  check(fixed && !fixed[0] && !fixed[1] && fixed[2]==255,"user-edited dump PNG is preserved during palette repair");sys_free(fixed);sys_delete_file(repair_path);sys_delete_file("revived_config/texture_dumps/LOCAL/TEST/LEGACY.HLZ.png.txt");
  test_audio_fonts();
  test_engine_retro();
  test_startup_dos();
  test_gameplay_graphics();
  test_remastered_ui();
  test_controls_0713();
  test_revision_0714();
  test_revision_0715();
  s_cat(report, "Assertions: ", sizeof report); str_int(report, sizeof report, assertions);
  s_cat(report, "\r\nFailures: ", sizeof report); str_int(report, sizeof report, failures); s_cat(report, "\r\n", sizeof report);
  sys_write_file("regression-results.txt", report, s_len(report));
  ExitProcess(failures ? 1 : 0);
}
#ifndef _WIN32
int main(){WinMainCRTStartup();}
#endif
