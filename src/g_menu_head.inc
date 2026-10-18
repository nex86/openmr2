// ===================================================================================================
// Game flow like the original: intro, main menu (column with 8 drums), car shop, weapon & repair shop,
// championship (6 tracks x 3 heats with 8/6/4 drivers), results, prize doors, game over, high scores,
// setup, load/save (20 slots), replays (20 slots). Lance Boyle clips from the CD between the screens.
// Layouts, texts and prices were taken from the original game (DOSBox screenshots).
// ===================================================================================================

enum GState { GS_CLIP, GS_MENU, GS_CARSHOP, GS_WEAPONSHOP, GS_TRACKINFO, GS_RACE, GS_RESULTS, GS_PRIZE, GS_GAMEOVER,
              GS_HISCORE, GS_OPTIONS, GS_SAVELOAD, GS_TRACKSELECT, GS_MESSAGE, GS_QUITDLG, GS_FONTTEST, GS_REPLAYS, GS_REPLAY, GS_PADMAP, GS_DEBUG, GS_GFX,
              GS_RETRO_SOUND, GS_REVIVE_EFFECTS, GS_EXTRAS, GS_QUIT };

static const char *game_state_name(int state){
  static const char *names[]={"Video","Main menu","Car shop","Weapon shop","Loading track","Race","Results","Prize selection","Game over","Leaderboard","Settings","Load / Save","Single race","Message","Game menu","Font test","Replays","Replay","Controller bindings","Debug / Cheats","Graphics","Advanced sound","Revived effects","Extras","Exit"};
  return state>=0 && state<(int)(sizeof names/sizeof names[0])?names[state]:"Unknown";
}

struct Profile {
  int active;                    // a championship is running
  int car;                       // -1 = no car yet
  int money; double damage; Loadout lo;
  int track, heat;               // championship progress
  int field[8]; int nfield;      // car indices of the drivers still in the championship (field[0] = player)
  int pts[8][3];                 // points per driver per heat
  int last_place;
  int unlocked;                  // number of cars available in the car shop
  int score;                     // high score
  int single;                    // single race (no championship)
  int hall_id;                   // high score entry of this championship
  int campaign;                  // points of the tracks already won (heat 1 + heat 2 of each, M.EXE)
  int grid_player;               // starting slot of the player (0 = front)
};
static Profile PF;

struct Cheats { int money, ammo, weapons, nodamage; };
struct HiEntry { char name[20]; int score, track, heat, id; };   // name: up to 17 characters as in the original

struct GameUI {
  int state, next_state;
  Pic bg; int bg_id;             // current background
  SprFile menu_spr[9]; SprFile shop_spr, shop2_spr, icon_spr, cursor_spr, track_spr[6], save_spr, repl_spr, chk_on, chk_off, exit_spr;
  Pic cursor_pal, fleche;
  int menu_anim[9]; double anim_t;
  int sel, kbd;                  // keyboard selection, keyboard in use
  int mx, my, click;
  double t;
  int menu_click; double menu_click_t;
  // clip playback
  Video clip; int clip_ok; double clip_t; int clip_next; char clip_queue[4][40]; int clip_nq;
  // shop
  int shop_car; double turn; int wsel; int repair_amount;
  Model *item_models[11];
  char msg[256]; int msg_next;
  int opts_laps, opts_music, opts_sfx, opts_clips, opts_gfx, opts_transfer;
  int single_track;
  Sound menu_music; int menu_music_on;
  Sound vox; int vox_ok;
  int loaded;
  HiEntry hi[9]; int hi_edit;    // high scores; entry that gets a name typed in (-1 none)
  int slot;                      // selected save / replay slot
  int rp_return;                 // where a replay returns to
  int pad_map[9]; int pad_wait;  // controller: input bit per action, action waiting for a new button (-1 none)
  Cheats cheat_career, cheat_single;   // cheats: championship and single race are separate
  int dbg_console, dbg_fps, dbg_info, dbg_road, dbg_noopp, dbg_stretch, dbg_camrot, dbg_noclips; int dbg_fpsmode; int ai_rescue; int sim_model;   // ai_rescue: KI-Notfallhilfe; dbg_fpsmode: race output 0 Original, 1 60 FPS, only 0/1   // technical debug options (both modes)
  int single_car, single_laps;   // single race settings (kept apart from the championship)
  Profile career_pf; int in_single;   // the championship is put aside during a single race
  int aborted;                   // the player gave up the race (pause menu)
  int custom_textures;
  int video_upscale,video_filter;
  int real_time_shadows,modern_explosions,vehicle_lights,damage_effects,enhanced_oil,missile_effects;
  int effect_intensity[6]; int effects_muted,retro_surprise;
  int startup_mode, remember_startup_mode;
  int gfx_mode, gfx_scale, gfx_msaa, gfx_fxaa, gfx_full, gfx_output_filter;   // output filter is Retro-only
  int opts_menumusic;            // music in the menus
  int revived_saved, revived_filter, revived_msaa, revived_fxaa, revived_fps;
  int gfx_intscale;              // Retro Mode: integer scaling only
  int opts_menusfx;              // sound effects in the menus
};
static GameUI G;
static int g_options_return=-1,g_options_pause;
static void options_close();
static void extras_open_model();
static void extras_open_video(const char *path);
static Font g_font_h14, g_font_h10;   // Handel Gothic fonts of the original menus
static const char *RETRO_FILTER_NAMES[6]={"Point","Linear","HQ2x","BRZ","Interlacing","CRT Filter"};
static const char *RETRO_FILTER_KEYS[6]={"point","linear","hq2x","brz","interlacing","crt"};

// M.EXE: drivers per heat 8 / 7 / 4 (the 8th of heat 1 is out, after heat 2 the best 4 by points go on, the
// final must be won); points tables per heat (the odd last digits of heat 2 are original); 100 for a car out
static const int HEAT_FIELD[3] = {8, 7, 4};
static const int HEAT_POINTS[3][8] = {{10000, 6000, 5000, 4000, 3000, 2000, 1000, 300},
                                      {16001, 11002, 9003, 7004, 5005, 3006, 2007, 1008},
                                      {200000, 190000, 180000, 170000, 160000, 150000, 140000, 130000}};
static const int POINTS_OUT = 100;
static const int POINTS[8] = {10000, 6000, 5000, 4000, 3000, 2000, 1000, 300};   // single race
static int race_laps(int heat);
static const uint32_t YELLOW = 0xff40d8ffu, WHITE = 0xffffffffu, GREY = 0xffb4b4b4u, ORANGE = 0xff2890ecu, GREEN = 0xff70ff70u, RED = 0xff4040ffu;
static const uint32_t BEIGE = 0xffa0c8d8u, GOLD = 0xff28c0f0u;
// which picture of the track selection (DATA640\TRACKn.SPR) shows which championship track
static const int TRACK_SPRITE[6] = {3, 0, 2, 4, 1, 5};

static void game_set_bg(const char *rel) { pic_free(&G.bg); pic_load(&G.bg, rel); }
static void menu_music_load(int bus=AUDIO_MENU_MUSIC){
  if(!audio_enabled(bus)||G.menu_music.pcm)return;
  size_t n;uint8_t *d=iso_load("MUSIC/WAIT.16",&n);if(d){int fr,ch,rate;int16_t *pcm=wav_decode(d,n,&fr,&ch,&rate);sys_free(d);if(pcm){G.menu_music.pcm=pcm;G.menu_music.frames=fr;G.menu_music.ch=ch;}}
}
static void menu_music_start(void) { if(!audio_enabled(AUDIO_MENU_MUSIC))return;menu_music_load();if(G.menu_music_on&&MX.v[0].snd==&G.menu_music)MX.v[0].bus=AUDIO_MENU_MUSIC;if (G.menu_music.pcm && !G.menu_music_on) { music_play(&G.menu_music, 1,AUDIO_MENU_MUSIC); G.menu_music_on = 1; } }
static void loading_music_start(void) {
  if(!audio_enabled(AUDIO_RACE_MUSIC)){music_stop();G.menu_music_on=0;return;}
  menu_music_load(AUDIO_RACE_MUSIC);
  if(MX.v[0].active && MX.v[0].snd==&G.menu_music)MX.v[0].bus=AUDIO_RACE_MUSIC;
  else music_play(&G.menu_music,1,AUDIO_RACE_MUSIC);
  G.menu_music_on=MX.v[0].active;
}
static void msfx(int id, float vol) { if (audio_enabled(AUDIO_MENU_SOUNDS)) snd_play(sfx_sample(id,AUDIO_MENU_SOUNDS),vol,1,0,0,AUDIO_MENU_SOUNDS); }   // sound effects of the menus (can be switched off)
static void menu_music_stop(void) { if (G.menu_music_on) { music_stop(); G.menu_music_on = 0; } }
static void race_music_start(){
  if(!audio_enabled(AUDIO_RACE_MUSIC))return;
  if(!TK.music.pcm && TK.loaded){char dir[64]="MUSIC/";str_cat(dir,TRACKS[TK.idx].music,sizeof dir);load_music_dir(&TK.music,dir,".16");}
  if(TK.music.pcm)music_play(&TK.music,1);
}

static void go(int state);
static void load_vox(void);
static void save_game(void);
static int load_game(void);
static void hiscore_offer(void);
static void show_message(const char *m, int next) { str_cpy(G.msg, m, 256); G.msg_next = next; if(next==GS_CARSHOP){G.state=GS_MESSAGE;G.sel=0;G.t=0;}else go(GS_MESSAGE); }

