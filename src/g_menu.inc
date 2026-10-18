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

// ------------------------------------------------------------------ clips (Lance Boyle, intro)
static void clip_queue(const char *path) { if (G.clip_nq < 4) str_cpy(G.clip_queue[G.clip_nq++], path, 40); }
static int clip_start_next(void) {
  while (G.clip_nq > 0) {
    char p[40]; str_cpy(p, G.clip_queue[0], 40);
    for (int i = 1; i < G.clip_nq; i++) str_cpy(G.clip_queue[i - 1], G.clip_queue[i], 40);
    G.clip_nq--;
    if (G.clip.data) video_close(&G.clip);
    size_t n; uint8_t *d = iso_load(p, &n);
    if (d && video_open(&G.clip, d, n, 400,audio_enabled(AUDIO_SOUNDS))) {
      if(sys_debug_enabled()){char event[100]="[event] Playing video: ";str_cat(event,p,sizeof event);sys_log(event);}
      G.clip_ok = 1; G.clip_t = 0;
      if (G.clip.pcm) { g_fmv_snd.pcm = G.clip.pcm; g_fmv_snd.frames = G.clip.pcm_frames; g_fmv_snd.ch = 2; voice_play(&g_fmv_snd, 0.9f); }
      return 1;
    }
    if(sys_debug_enabled()){char error[100]="[error] Could not load video: ";str_cat(error,p,sizeof error);sys_log(error);}
  }
  return 0;
}
static void clip_play_then(int next) {
  if (!G.opts_clips || G.dbg_noclips) { G.clip_nq = 0; go(next); return; }
  G.clip_next = next;
  menu_music_stop();
  if (clip_start_next()) { G.state = GS_CLIP; }
  else go(next);
}
static void clip_stop(void) { voice_stop(); if (G.clip.data) video_close(&G.clip); G.clip_ok = 0; mem_zero(&g_fmv_snd, sizeof g_fmv_snd); }

// ------------------------------------------------------------------ credits: CREDITS.UBB with the credits text scrolling up
static char g_cred_txt[4096]; static const char *g_cred_line[160]; static int g_cred_n, g_cred_head[160];
static void credits_build(void) {
  if (g_cred_n) return;
  int o = 0;
  auto add = [&](const char *t, int head) {
    if (g_cred_n >= 160 || o + (int)str_len(t) + 1 >= (int)sizeof g_cred_txt) return;
    str_cpy(g_cred_txt + o, t, (int)sizeof g_cred_txt - o); g_cred_line[g_cred_n] = g_cred_txt + o; g_cred_head[g_cred_n++] = head; o += (int)str_len(t) + 1;
  };
  add("OpenMR2", 2); add("MegaRace 2 REVIVED", 2); add("", 0);
  add("Windows OpenGL Portierung", 1); add("Nexxus86", 0); add("", 0); add("", 0);
  // the credits of the original game, read from M.EXE
  char p[512]; str_cpy(p, g_gamedir, 512); str_cat(p, "/M.EXE", 512);
  size_t n; uint8_t *d = sys_read_file(p, &n);
  if (d) {
    const char *key = "Cryo Interactive Entertainment"; size_t kl = str_len(key), st = 0;
    for (size_t i = 1; i + kl < n && !st; i++) if (d[i - 1] == '2') { size_t q = 0; while (q < kl && d[i + q] == (uint8_t)key[q]) q++; if (q == kl) st = i - 1; }
    size_t i = st;
    for (int g = 0; st && g < 140 && i < n; g++) {
      while (i < n && d[i] == 0) i++;
      if (i >= n || d[i] != '2') break;
      char line[96]; int k = 0; size_t j = i + 1;
      for (; j < n && d[j]; j++) { char c = (char)d[j]; if (c == '\t') { if (k && line[k - 1] != ' ' && k < 94) { line[k++] = ' '; line[k++] = '-'; line[k++] = ' '; } continue; } if (k < 90) line[k++] = c; }
      while (k && line[k - 1] == ' ') k--; line[k] = 0; i = j;
      int upper = 0; for (int a = 0; line[a]; a++) { int b = a; while (line[b] >= 'A' && line[b] <= 'Z') b++; if (b - a >= 3) upper = 1; if (b > a) a = b - 1; }
      int head = !upper;
      if (!str_icmp(line, "Megarace II") || !str_icmp(line, "Cryo Interactive Entertainment")) head = 1;
      if (head && g_cred_n && g_cred_line[g_cred_n - 1][0]) add("", 0);
      add(line, head);
      if (!str_icmp(line, "Copyright 1996 Cryo/Mindscape")) break;
    }
    sys_free(d);
  }
}
static int g_credits_on;
static double credits_height(void) { return g_cred_n * 26.0; }
static void credits_draw(double t) {
  double y0 = 480 - t * 34.0;   // scroll speed in pixels per second (640x480)
  cv_clear(0);
  for (int i = 0; i < g_cred_n; i++) {
    double y = y0 + i * 26; if (y < -30 || y > 480) continue;
    const Font &f = g_cred_head[i] == 2 ? g_font_big : g_font_h14;
    cv_text(f, 320, (int)y, g_cred_line[i], g_cred_head[i] ? ORANGE : WHITE, 1, 1);
  }
}
static int credits_done(double t) { return 480 - t * 34.0 + credits_height() < -10; }

static int clip_frame(int W, int H, double dt, const Input *in) {
  int skip = in->pressed[K_ESC] || in->pressed[K_SPACE] || in->pressed[K_ENTER] || in->lmb_pressed;
  if (in->pressed[K_ESC]) G.clip_nq = 0;     // ESC skips everything up to the next screen (as in the original)
  G.clip_t += dt;
  int frame;
  Voice *v = &MX.v[1];
  if (MX.dev && G.clip.pcm && v->active && v->snd == &g_fmv_snd) frame = (int)((v->pos > 7000 ? v->pos - 7000 : 0) * G.clip.nframes / G.clip.pcm_frames);
  else frame = (int)(G.clip_t * G.clip.fps_exact);
  if (g_credits_on && !skip && frame >= G.clip.nframes && !credits_done(G.clip_t)) {   // the text is longer than the film: keep showing the last picture
    frame = G.clip.nframes - 1;
  }
  else if (g_credits_on && !skip && credits_done(G.clip_t)) skip = 1;
  if (skip || frame >= G.clip.nframes) {
    clip_stop(); g_credits_on = 0;
    if (!skip || !in->pressed[K_ESC]) { if (clip_start_next()) return 1; }
    G.clip_nq = 0; go(G.clip_next); return 1;
  }
  video_decode(&G.clip, frame);
  video_tex_upload(&TK.vtex, G.clip);
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  float aspect = (G.clip.w * 4.0f / 320) / (G.clip.h * 3.0f / 400);
  int rx, ry, rw, rh; fit_rect_px(W, H, aspect, &rx, &ry, &rw, &rh);
  draw_video_rect(TK.vtex, W, H, rx, ry, rw, rh);
  if (g_credits_on) { credits_draw(A.graphics_test==18?1.5:G.clip_t); canvas_draw(W, H); }
  return 1;
}
static void credits_play_then(int next) {   // the credits are shown even when the Lance Boyle clips are switched off
  if (G.dbg_noclips) { go(next); return; }
  credits_build();
  G.clip_nq = 0; clip_queue("BOYLE/CREDITS.UBB");
  G.clip_next = next; menu_music_stop();
  if (clip_start_next()) { G.state = GS_CLIP; g_credits_on = 1; } else go(next);
}

// ------------------------------------------------------------------ helpers for the screens
static int hit(int x, int y, int w, int h) { return G.mx >= x && G.mx < x + w && G.my >= y && G.my < y + h; }
static int spr_hit(const Sprite &s) { return hit(s.x, s.y, s.w, s.h); }
static void draw_cursor(void) {
  if (G.cursor_spr.n) { Sprite s = G.cursor_spr.f[0]; s.x = G.mx; s.y = G.my; cv_spr(s, G.fleche.pal, 0, 0, 1); }
  else { cv_rect(G.mx, G.my, 3, 12, rgba(255, 220, 60, 255)); cv_rect(G.mx, G.my, 10, 3, rgba(255, 220, 60, 255)); }
}
static void text_box(int x, int y, const char *s, uint32_t col) { cv_text(g_font_small, x, y, s, col, 0, 1); }
// framed title like "Wagen waehlen" in the car shop
static void title_box(int cx, int y, const char *s) {
  int w = cv_text_w(g_font_h14, s, 1) + 20, h = g_font_h14.height + 8, x = cx - w / 2;
  cv_rect(x, y, w, h, rgba(40, 38, 34, 235));
  cv_rect(x, y, w, 2, rgba(150, 140, 120, 255)); cv_rect(x, y + h - 2, w, 2, rgba(70, 64, 56, 255));
  cv_rect(x, y, 2, h, rgba(150, 140, 120, 255)); cv_rect(x + w - 2, y, 2, h, rgba(70, 64, 56, 255));
  cv_text(g_font_h14, cx, y + 4, s, ORANGE, 1, 1);
}
static Sprite spr_at(const Sprite &s, int x, int y) { Sprite r = s; r.x = x; r.y = y; return r; }

// DOS shop preview uses the opposite horizontal camera basis to the OpenGL viewer.
static M4 turntable_projection(float aspect){M4 p=m4_persp(32.0f*PI/180.0f,aspect,0.1f,200);p.m[0]=-p.m[0];return p;}
static const double TURNTABLE_RADIANS_PER_SECOND=2.1; // 3.5 times the former 0.6 rad/s
static double turntable_advance(double angle,double dt){double a=angle+dt*TURNTABLE_RADIANS_PER_SECOND;const double tau=6.283185307179586;while(a>=tau)a-=tau;while(a<0)a+=tau;return a;}
// rotating car (3D on the GPU) in a rectangle of the 640x480 screen
static void draw_turntable(int W, int H, int car, double angle, float x, float y, float w, float h, const char *set) {
  if (car < 0) return;
  if(A.graphics_test==14||A.graphics_test==15)angle=A.effect_age_ms*.001;
  Model *m = track_car_model(car);
  if (!m) { static Model *cache[NCARS]; if (!cache[car]) cache[car] = load_model_from(set, CARS[car].model); m = cache[car]; }
  if (!m) return;
  int vx, vy, vw, vh; vrect_viewport(W, H, x, y, w, h, &vx, &vy, &vw, &vh);
  glViewport(vx, vy, vw, vh);
  glEnable(GL_DEPTH_TEST); glClear(GL_DEPTH_BUFFER_BIT);
  V3 ext = m->bmax - m->bmin; float r = m_sqrt(dot(ext, ext)) * 0.5f;
  M4 proj = turntable_projection(w/h);
  V3 eye = v3(0, r * 0.9f, r * 2.9f);
  M4 view = m4_look(eye, v3(0, ext.y * 1.1f, 0), v3(0, 1, 0));
  M4 vp = m4_mul(proj, view);
  model_shader_begin(vp,1);glUniform1i(R.um_lamps,g_vehicle_lights&&(!g_tex_nearest||g_retro_effects));glUniform1i(R.um_rear_only,A.graphics_test==15);glUniform1f(R.um_brake,A.graphics_test==15&&A.test_damage?1.f:0.f);
  glUniformMatrix4fv(R.um_vp, 1, GL_FALSE, vp.m); glUniform3f(R.um_cam, eye.x, eye.y, eye.z);
  glUniform1i(R.um_tex, 0); glUniform1f(R.um_lit, 1.0f); glUniform1f(R.um_gain, 1.0f); glUniform1f(R.um_gray, 0.0f);
  V3 c = (m->bmin + m->bmax) * 0.5f;
  M4 base = m4_mul(m4_mul(m4_roty((float)angle), m4_trans(v3(-c.x, -m->bmin.y, -c.z))), m4_id());
  draw_model_mv(*m, base, (float)(angle * 3), 0);
  preview_lights_draw(*m,vp,base,eye,(float)(angle*3),W,H);
  glBindVertexArray(0);
}

// ------------------------------------------------------------------ championship logic
static void profile_new(void) {
  mem_zero(&PF, sizeof PF);
  PF.active = 1; PF.car = -1; PF.money = 25000; PF.unlocked = 3; PF.track = 0; PF.heat = 0; PF.grid_player = 7;
  // opponents: 7 different cars
  int pool[NCARS]; for (int i = 0; i < NCARS; i++) pool[i] = i;
  for (int i = NCARS - 1; i > 0; i--) { int j = irnd(i + 1); int t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
  PF.nfield = 8; PF.field[0] = -1; for (int i = 1; i < 8; i++) PF.field[i] = pool[i - 1];
}
static void default_loadout(Loadout *lo) {   // as in the original after buying a car: L3 with 12 Kard, L6 with 20 Boomer, 2 mines
  mem_zero(lo, sizeof *lo);
  lo->front[0] = W_KARD; lo->fammo[0] = 12; lo->front[1] = W_BOOMER; lo->fammo[1] = 20;
  lo->rear[0] = W_MINE; lo->rammo[0] = 2;
}
// ------------------------------------------------------------------ cheats (championship and single race separately)
static const Cheats &cheats_cur(void) { return PF.single ? G.cheat_single : G.cheat_career; }
static int can_pay(int cost) { return cheats_cur().money || PF.money >= cost; }
static void pay(int cost) {
  if (!cheats_cur().money) PF.money -= cost;
  if(sys_debug_enabled()){char m[128]="[event] Payment: ";str_int(m,sizeof m,cost);str_cat(m,"; balance = ",sizeof m);str_int(m,sizeof m,PF.money);if(cheats_cur().money)str_cat(m," (money cheat active)",sizeof m);sys_log(m);}
}
static int money_shown(void) { return cheats_cur().money ? 999999 : PF.money; }
static void allweapons_loadout(Loadout *lo) {   // cheat: every launcher full, shield
  mem_zero(lo, sizeof *lo);
  lo->front[0] = W_HITRAX; lo->fammo[0] = WEAPONS[W_HITRAX].capacity; lo->front[1] = W_BOOMER; lo->fammo[1] = WEAPONS[W_BOOMER].capacity;
  lo->rear[0] = W_MINE; lo->rammo[0] = WEAPONS[W_MINE].capacity; lo->rear[1] = W_OIL; lo->rammo[1] = WEAPONS[W_OIL].capacity;
  lo->shield = 100;
}
static int race_laps(int heat) { return G.opts_laps > 0 ? G.opts_laps : heat < 2 ? 5 : 3; }   // original: 5 / 5 / 3
static int car_available(int car) {   // the shop sells the cheapest cars first, more unlock during the championship
  if (cheats_cur().weapons) return 1;   // cheat: all cars and upgrades
  int order[NCARS]; for (int i = 0; i < NCARS; i++) order[i] = i;
  for (int i = 1; i < NCARS; i++) { int k = order[i], j = i - 1; while (j >= 0 && CARS[order[j]].price > CARS[k].price) { order[j + 1] = order[j]; j--; } order[j + 1] = k; }
  for (int i = 0; i < PF.unlocked && i < NCARS; i++) if (order[i] == car) return 1;
  return 0;
}

static void start_race(void) {
  menu_music_stop();
  int ti = PF.single ? G.single_track : PF.track;
  int heat = PF.single ? 0 : PF.heat;
  if (!track_load(ti, heat)) { show_message("Strecke konnte nicht geladen werden.", GS_MENU); return; }
  int opp[7]; int n = 0;
  for (int i = 1; i < PF.nfield && n < 7; i++) opp[n++] = PF.field[i];
  int start_rank = PF.single ? PF.nfield - 1 : PF.grid_player;   // the field is ordered by the standings (M.EXE re-sorts it)
  const Cheats &ch = cheats_cur();
  Loadout lo = PF.lo; if (ch.weapons) allweapons_loadout(&lo);
  if (G.dbg_noopp) { n = 0; start_rank = 0; }
  g_cheat_ammo = ch.ammo; g_cheat_nodmg = ch.nodamage;
  RC.heat = heat;
  race_setup(PF.car, lo, ch.nodamage ? 0 : PF.damage, opp, n, start_rank, PF.single && G.single_laps ? G.single_laps : race_laps(heat));
  rec_start();
  race_music_start();
  go(GS_RACE);
}

static void race_finished(void) {
  race_end_sounds(); music_stop();
  RCar &P = player();
  // write back the player's car state
  if (!cheats_cur().weapons) { PF.lo = P.lo; PF.lo.shield = P.shield > 0 ? (P.shield < 2 ? 2 : (int)(P.shield + 0.999)) : 0; }   // remaining shield in % (2..100; 1 = full, older saves)
  if (!cheats_cur().nodamage) PF.damage = P.damage;
  PF.money += P.earned;   // bonus objects picked up on the track
  // points for the heat (M.EXE tables), money: half of the heat 1 / heat 2 points, 6000 for winning the final
  for (int pos = 0; pos < RC.ncar; pos++) {
    int ci = RC.order[pos];                  // race car index == field index
    if (ci < 8 && !PF.single) PF.pts[ci][PF.heat] = RC.car[ci].out ? POINTS_OUT : HEAT_POINTS[PF.heat][pos];
  }
  PF.last_place = P.place;
  replay_autosave();
  load_vox();
  if (!PF.single && !RC.car[0].out) {
    if (PF.heat < 2) PF.money += PF.pts[0][PF.heat] / 2;
    else if (P.place == 1) PF.money += 6000;
  }
  if (!PF.single) PF.score = PF.campaign + PF.pts[0][0] + PF.pts[0][1];   // high score: not the money
  go(GS_RESULTS);
}

// single race: the championship is put aside and restored afterwards (nothing of the single race is kept in it)
static void single_end(void) { if (G.in_single) { PF = G.career_pf; G.in_single = 0; } G.aborted = 0; }
// after the results: qualification
static void game_over(void) {   // Lance Boyle's game over clip, then the GAME OVER screen
  race_end_sounds();music_stop();G.menu_music_on=0;
  G.clip_nq = 0; clip_queue("BOYLE0/BOYLE15.UBB"); clip_play_then(GS_GAMEOVER);
}
static void results_continue(void) {
  G.aborted = 0;
  if (!PF.single) save_game();
  if (PF.single) { single_end(); go(GS_MENU); return; }
  if (RC.crashed) { game_over(); return; }
  if (PF.heat < 2) {
    // heat 1: the 8th is out; heat 2: the best 4 by the points of heats 1 + 2 go on (M.EXE)
    int keep = HEAT_FIELD[PF.heat + 1];
    int idx[8]; for (int i = 0; i < PF.nfield; i++) idx[i] = i;
    auto total = [&](int i) { return PF.pts[i][0] + PF.pts[i][1]; };
    for (int i = 1; i < PF.nfield; i++) { int k = idx[i], j = i - 1; while (j >= 0 && total(idx[j]) < total(k)) { idx[j + 1] = idx[j]; j--; } idx[j + 1] = k; }
    int player_in = 0; for (int i = 0; i < keep && i < PF.nfield; i++) if (idx[i] == 0) player_in = 1;
    if (!player_in) { game_over(); return; }
    // the new field in the order of the standings (= starting order); the player keeps index 0
    int nf[8], np[8][3], c = 0;
    nf[c] = PF.field[0]; for (int h = 0; h < 3; h++) np[c][h] = PF.pts[0][h]; c++;
    for (int i = 0; i < keep && i < PF.nfield; i++) if (idx[i] != 0) { nf[c] = PF.field[idx[i]]; for (int h = 0; h < 3; h++) np[c][h] = PF.pts[idx[i]][h]; c++; }
    PF.nfield = c; for (int i = 0; i < c; i++) { PF.field[i] = nf[i]; for (int h = 0; h < 3; h++) PF.pts[i][h] = np[i][h]; }
    for (int i = 0; i < keep; i++) if (idx[i] == 0) PF.grid_player = i;
    PF.heat++;
    go(GS_MENU);
  } else {
    if (PF.last_place != 1) { game_over(); return; }
    // track won: prize, next track with a fresh field
    // prize door clips per track, table from M.EXE
    static const char *doors[6] = {"BOYLE/BOYLE07A.UBB", "BOYLE/BOYLE03A.UBB", "BOYLE/BOYLE09A.UBB", "BOYLE2/BOYLE11A.UBB", "BOYLE2/BOYLE05A.UBB", "BOYLE2/BOYLE13.UBB"};
    clip_queue(TRACKS[PF.track].win_clip); clip_queue(doors[PF.track % 6]);
    PF.unlocked = NCARS;   // after the first track all 14 cars can be bought (M.EXE switches from 3 to 14)
    PF.campaign += PF.pts[0][0] + PF.pts[0][1]; PF.score = PF.campaign;
    clip_play_then(GS_PRIZE);
  }
}
static void next_track(void) {
  PF.track++; PF.heat = 0; mem_zero(PF.pts, sizeof PF.pts); PF.grid_player = 7;
  if (PF.track >= 6) { clip_queue("BOYLE2/OUTRO.UBB"); PF.track = 5; hiscore_offer(); PF.active = 0; save_game(); clip_play_then(GS_HISCORE); return; }
  int pool[NCARS], n = 0; for (int i = 0; i < NCARS; i++) if (i != PF.car) pool[n++] = i;
  for (int i = n - 1; i > 0; i--) { int j = irnd(i + 1); int t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
  PF.nfield = 8; PF.field[0] = PF.car; for (int i = 1; i < 8; i++) PF.field[i] = pool[i - 1];
  go(GS_MENU);
}

// ------------------------------------------------------------------ save / load (autosave + 20 slots)
static void save_path(char *out, int slot) {   // slot -1 = automatic save of the running championship
  char n[64] = "openmr2_save"; if (slot >= 0) { str_cat(n, "_", 64); if (slot + 1 < 10) str_cat(n, "0", 64); str_int(n, 64, slot + 1); } str_cat(n, ".dat", 64); cfg_path(out, n);
}
static int save_to(int slot, const Profile &p) {
  char path[512]; save_path(path, slot);
  uint8_t buf[sizeof(Profile) + 8]; mem_cpy(buf, "MR2GLSV1", 8); mem_cpy(buf + 8, &p, sizeof p);
  return sys_write_file(path, buf, sizeof buf);
}
static int load_from(int slot, Profile *p) {
  char path[512]; save_path(path, slot);
  size_t n; uint8_t *d = sys_read_file(path, &n);
  if (!d) return 0;
  int ok = n > 8 + 64 && n <= sizeof(Profile) + 8 && d[0] == 'M' && d[6] == 'V';   // older versions saved a shorter profile
  if (ok) { mem_zero(p, sizeof *p); mem_cpy(p, d + 8, n - 8); }
  sys_free(d); return ok;
}
static void save_game(void) { if (!PF.single) save_to(-1, PF); }
static int load_game(void) { return load_from(-1, &PF); }

// ------------------------------------------------------------------ high scores (9 entries, hall of fame as in the original)
// As in M.EXE: opening "High Scores" checks the score of the running championship; when it is good enough an
// entry "Player" appears in red, the name is typed (up to 17 characters) and confirmed with Enter (yellow).
// The same happens automatically after being eliminated or winning the championship. The list is saved on leaving.
static void hiscore_save(void) { char p[512]; cfg_path(p, "openmr2_hall.dat"); sys_write_file(p, G.hi, sizeof G.hi); }
static void hiscore_load(void) {
  static const HiEntry def[9] = {{"Franoukwel", 120000, 5, 2, 0}, {"Shune", 50000, 4, 2, 0}, {"Popof", 40000, 3, 2, 0}, {"Juan", 35000, 2, 2, 0}, {"Kongo", 10000, 1, 2, 0},
                                 {"Laurizz", 5000, 0, 1, 0}, {"Orus", 4000, 0, 0, 0}, {"Peej", 3000, 0, 0, 0}, {"Thomas", 2000, 0, 0, 0}};
  mem_cpy(G.hi, def, sizeof G.hi);
  char p[512]; cfg_path(p, "openmr2_hall.dat");
  size_t n; uint8_t *d = sys_read_file(p, &n);
  if (d && n == sizeof G.hi) mem_cpy(G.hi, d, sizeof G.hi);
  sys_free(d);
  for (int i = 0; i < 9; i++) G.hi[i].name[19] = 0;
  G.hi_edit = -1;
}
static void hiscore_offer(void) {   // put the running championship into the list if its score is good enough
  G.hi_edit = -1;
  if (PF.single || PF.score <= 0) return;
  if (!PF.hall_id) PF.hall_id = 1 + (int)(rnd() & 0x3fffffff);
  int cur = -1; for (int i = 0; i < 9; i++) if (G.hi[i].id == PF.hall_id) cur = i;
  char name[20] = "Player";
  if (cur >= 0) { str_cpy(name, G.hi[cur].name, 20); for (int i = cur; i < 8; i++) G.hi[i] = G.hi[i + 1]; mem_zero(&G.hi[8], sizeof G.hi[8]); }
  int pos = 9; for (int i = 8; i >= 0; i--) if (PF.score > G.hi[i].score) pos = i;
  if (pos >= 9) { if (cur >= 0) hiscore_save(); return; }
  for (int i = 8; i > pos; i--) G.hi[i] = G.hi[i - 1];
  HiEntry &e = G.hi[pos]; mem_zero(&e, sizeof e);
  str_cpy(e.name, name, 20); e.score = PF.score; e.track = PF.track > 5 ? 5 : PF.track; e.heat = PF.heat > 2 ? 2 : PF.heat; e.id = PF.hall_id;
  G.hi_edit = (cur < 0 || !str_icmp(name, "Player")) ? pos : -1;   // a name that was already typed stays
  if (G.hi_edit < 0) hiscore_save();
  save_game();
}

// ------------------------------------------------------------------ setup (MEGARACE II SETUP)
// Active Retro values and a persistent snapshot of the user's Revived preferences.
static void graphics_options_apply(){
  if(G.gfx_mode==1){
    if(!G.revived_saved){G.revived_filter=G.opts_gfx;G.revived_msaa=G.gfx_msaa;G.revived_fxaa=G.gfx_fxaa;G.revived_fps=G.dbg_fpsmode;G.revived_saved=1;}
    G.opts_gfx=1;G.gfx_msaa=0;G.gfx_fxaa=0;G.dbg_fpsmode=0;
  }else if(G.revived_saved){
    G.opts_gfx=G.revived_filter;G.gfx_msaa=G.revived_msaa;G.gfx_fxaa=G.revived_fxaa;G.dbg_fpsmode=G.revived_fps;G.revived_saved=0;
  }
}
static int graphics_option_locked(int kind){if(kind==14&&!G.video_upscale)return 1;return G.gfx_mode==1 && (kind==4||kind==5||kind==6||kind==10||kind==11||kind==13||kind==14);}
static void setup_apply(void) {
  graphics_options_apply();
  g_revived_text=G.gfx_mode==0;
  G.opts_sfx=audio_enabled(AUDIO_SOUNDS);G.opts_music=audio_enabled(AUDIO_RACE_MUSIC);
  G.opts_menumusic=audio_enabled(AUDIO_MENU_MUSIC);G.opts_menusfx=audio_enabled(AUDIO_MENU_SOUNDS);
  MX.music_vol=0.55f;MX.sfx_vol=0.9f;
  for(int i=0;i<NVOICES;i++)if(!audio_enabled(MX.v[i].bus))MX.v[i].active=0;
  for(int i=0;i<S_COUNT;i++)if(!audio_enabled(sfx_bus(i)))sound_free(&SFX[i]);
  if(!G.opts_menumusic&&G.state!=GS_TRACKINFO){G.menu_music_on=0;sound_free(&G.menu_music);}
  if(!G.opts_music)sound_free(&TK.music);
  if(!G.opts_sfx){voice_stop();sound_free(&G.vox);G.vox_ok=0;}
  g_smooth_filter = G.opts_gfx == 0;
}
// ------------------------------------------------------------------ game controller (XInput)
// input bits: 0..15 = XInput buttons, 16/17 = left/right trigger, 18..21 = left stick left/right/up/down,
// 22..25 = right stick left/right/up/down
enum { PA_GAS, PA_BRAKE, PA_LEFT, PA_RIGHT, PA_FIRE, PA_DROP, PA_FSWITCH, PA_RSWITCH, PA_PAUSE, PA_COUNT };
static const char *PA_NAME[PA_COUNT] = {"Gas", "Bremse / R\xfc" "ckw\xe4rts", "Links lenken", "Rechts lenken", "Frontwaffe feuern", "Heckwaffe abwerfen", "Frontwaffe wechseln", "Heckwaffe wechseln", "Pause"};
static const int PA_DEFAULT[PA_COUNT] = {17, 16, 18, 19, 12, 13, 15, 14, 4};   // RT, LT, left stick, A, B, Y, X, Start
static const char *pad_bit_name(int b) {
  static const char *n[26] = {"Steuerkreuz hoch", "Steuerkreuz runter", "Steuerkreuz links", "Steuerkreuz rechts", "Start", "Back / Ansicht", "Linker Stick (Klick)", "Rechter Stick (Klick)",
                              "LB", "RB", "?", "?", "A", "B", "X", "Y", "LT", "RT", "Linker Stick links", "Linker Stick rechts", "Linker Stick hoch", "Linker Stick runter",
                              "Rechter Stick links", "Rechter Stick rechts", "Rechter Stick hoch", "Rechter Stick runter"};
  return b >= 0 && b < 26 ? n[b] : "-";
}
static PadState g_pad; static float g_pad_val[26]; static unsigned g_pad_down, g_pad_prev, g_pad_pressed;
static double g_pad_rep_t[4]; static int g_pad_cursor; static float g_pad_cx, g_pad_cy;
static void pad_poll(double dt) {
  g_pad_prev = g_pad_down; g_pad_down = 0;
  for (int i = 0; i < 26; i++) g_pad_val[i] = 0;
  if (!sys_pad_read(&g_pad)) { g_pad_pressed = 0; g_pad_analog = 0; return; }
  for (int i = 0; i < 16; i++) if (g_pad.buttons & (1u << i)) g_pad_val[i] = 1;
  g_pad_val[16] = g_pad.lt; g_pad_val[17] = g_pad.rt;
  g_pad_val[18] = g_pad.lx < 0 ? -g_pad.lx : 0; g_pad_val[19] = g_pad.lx > 0 ? g_pad.lx : 0; g_pad_val[20] = g_pad.ly > 0 ? g_pad.ly : 0; g_pad_val[21] = g_pad.ly < 0 ? -g_pad.ly : 0;
  g_pad_val[22] = g_pad.rx < 0 ? -g_pad.rx : 0; g_pad_val[23] = g_pad.rx > 0 ? g_pad.rx : 0; g_pad_val[24] = g_pad.ry > 0 ? g_pad.ry : 0; g_pad_val[25] = g_pad.ry < 0 ? -g_pad.ry : 0;
  for (int i = 0; i < 26; i++) if (g_pad_val[i] > 0.5f) g_pad_down |= 1u << i;
  g_pad_pressed = g_pad_down & ~g_pad_prev;
  (void)dt;
}
static int pad_is(unsigned bits, int b) { return b >= 0 && b < 26 && (bits >> b) & 1; }
// translate the controller into the keys and mouse the game already understands
static void pad_apply(Input *in, double dt, int W, int H) {
  pad_poll(dt);
  g_pad_analog = 0;
  if (!g_pad.connected) return;
  int racing = G.state == GS_RACE && !RC.paused && RC.phase != PH_FLYBY;
  if (G.state == GS_PADMAP && G.pad_wait >= 0) return;   // the remap screen reads the raw buttons
  if (racing) {
    const int *m = G.pad_map; const int key[PA_COUNT] = {K_UP, K_DOWN, K_LEFT, K_RIGHT, K_SPACE, K_ENTER, K_TAB, K_SLASH, K_ESC};
    for (int a = 0; a < PA_COUNT; a++) {
      if (a <= PA_RIGHT) continue;   // driving is analog
      if (pad_is(g_pad_down, m[a])) in->down[key[a]] = 1;
      if (pad_is(g_pad_pressed, m[a])) in->pressed[key[a]] = 1;
    }
    // the steering also works with the d-pad
    g_pad_steer = g_pad_val[m[PA_RIGHT]] - g_pad_val[m[PA_LEFT]] + g_pad_val[3] - g_pad_val[2];
    if (g_pad_steer > 1) g_pad_steer = 1; if (g_pad_steer < -1) g_pad_steer = -1;
    g_pad_gas = g_pad_val[m[PA_GAS]]; g_pad_brake = g_pad_val[m[PA_BRAKE]];
    g_pad_analog = 1;
    return;
  }
  // menus, pause, clips: d-pad / left stick = arrows (with repeat), A = Enter, B = Esc, Start = Enter (Esc in the pause menu)
  static const int dirkey[4] = {K_UP, K_DOWN, K_LEFT, K_RIGHT};
  static const int dpad[4] = {0, 1, 2, 3}, stick[4] = {20, 21, 18, 19};
  for (int d = 0; d < 4; d++) {
    int now = pad_is(g_pad_down, dpad[d]) || pad_is(g_pad_down, stick[d]);
    int edge = pad_is(g_pad_pressed, dpad[d]) || pad_is(g_pad_pressed, stick[d]);
    if (edge) { in->pressed[dirkey[d]] = 1; g_pad_rep_t[d] = 0.4; }
    else if (now) { g_pad_rep_t[d] -= dt; if (g_pad_rep_t[d] <= 0) { in->pressed[dirkey[d]] = 1; g_pad_rep_t[d] = 0.12; } }
    if (now) in->down[dirkey[d]] = 1;
  }
  if (pad_is(g_pad_pressed, 12)) in->pressed[K_ENTER] = 1;
  if (pad_is(g_pad_pressed, 13)) in->pressed[K_ESC] = 1;
  if (pad_is(g_pad_pressed, 4)) in->pressed[G.state == GS_RACE ? K_ESC : K_ENTER] = 1;
  if (pad_is(g_pad_pressed, 5)) in->pressed[K_ESC] = 1;
  // right stick moves the mouse pointer, X clicks
  if (in->mouse_dx || in->mouse_dy) g_pad_cursor = 0;
  float rx = g_pad.rx, ry = g_pad.ry;
  if (rx * rx + ry * ry > 0.01f) {
    if (!g_pad_cursor) { g_pad_cursor = 1; g_pad_cx = (float)in->mx; g_pad_cy = (float)in->my; }
    float sp = (float)H * 0.9f * (float)dt;
    g_pad_cx += rx * sp; g_pad_cy -= ry * sp;
    if (g_pad_cx < 0) g_pad_cx = 0; if (g_pad_cx > W - 1) g_pad_cx = (float)(W - 1); if (g_pad_cy < 0) g_pad_cy = 0; if (g_pad_cy > H - 1) g_pad_cy = (float)(H - 1);
    in->mouse_dx = rx; in->mouse_dy = -ry;
  }
  if (g_pad_cursor) { in->mx = (int)g_pad_cx; in->my = (int)g_pad_cy; }
  if (pad_is(g_pad_pressed, 14)) { in->lmb_pressed = 1; in->lmb = 1; }
  else if (pad_is(g_pad_down, 14)) in->lmb = 1;
}

static void texture_options_apply();
static void debug_apply(void) {
  g_retro_effects=G.gfx_mode==1&&G.retro_surprise&&!G.effects_muted;
  for(int i=0;i<6;i++)g_effect_strength[i]=g_retro_effects?1.f:(float)G.effect_intensity[i]/100.f;
  int enabled=G.gfx_mode==0&&!G.effects_muted;
  g_vehicle_lights=g_retro_effects||(enabled&&G.vehicle_lights&&G.effect_intensity[1]>0);
  g_damage_effects=g_retro_effects||(enabled&&G.damage_effects&&G.effect_intensity[4]>0);
  g_enhanced_oil=g_retro_effects||(enabled&&G.enhanced_oil&&G.effect_intensity[2]>0);
  g_realtime_shadows=g_retro_effects||(enabled&&G.real_time_shadows&&G.effect_intensity[0]>0);
  g_modern_explosions=g_retro_effects||(enabled&&G.modern_explosions&&G.effect_intensity[3]>0);
  g_missile_effects=g_retro_effects||(enabled&&G.missile_effects&&G.effect_intensity[5]>0);
  graphics_options_apply();   // technical debug options (championship and single race)
  if(A.debug_start_console){G.dbg_console=1;A.debug_start_console=0;}
  G.dbg_console=A.debug_console=sys_debug_console(G.dbg_console);
  texture_options_apply();
  static int cmd_road = -1; if (cmd_road < 0) cmd_road = A.roaddbg;
  A.roaddbg = G.dbg_road || cmd_road; g_dbg_info = G.dbg_info; g_dbg_fps = G.dbg_fps; g_dbg_noopp = G.dbg_noopp; g_cam_rot_interp = G.gfx_mode==1?0:G.dbg_camrot; g_out_mode = G.dbg_fpsmode == 0 ? 0 : 1; g_ai_rescue = G.gfx_mode==1?0:G.ai_rescue; g_sim_model = G.gfx_mode==1?0:A.sim_revived ? 1 : G.sim_model;
  static int last_output=-1;if(last_output!=g_out_mode){sys_log(g_out_mode==1?"[event] Race output: 60fps; interpolation enabled.":"[event] Race output: Original; interpolation disabled.");last_output=g_out_mode;}
  if (G.gfx_mode==1 || G.dbg_stretch) { VIDEO_Y0 = 30; VIDEO_H = 420; } else { VIDEO_Y0 = 45; VIDEO_H = 350; }
}
static void texture_options_apply(){
  g_revived_ui=G.gfx_mode==0 && g_ui_assets_allowed;
  g_video_upscale=G.gfx_mode==0 && G.video_upscale;g_video_filter=G.video_filter;
  int enabled=G.gfx_mode==0 && G.custom_textures;
  if(g_custom_textures!=enabled){g_custom_textures=enabled;if(enabled)g_texture_reload=1;sys_log(enabled?"[event] Custom textures enabled.":"[event] Custom textures disabled.");}
}
static void graphics_mode_set(int mode){
  graphics_options_apply();G.gfx_mode=mode?1:0;graphics_options_apply();setup_apply();debug_apply();
}
// settings as plain text: revived_config/openmr2_setup.cfg ("key = value" per line, # comments)
struct CfgItem { const char *key; int *val; int lo, hi; const char *note; };
enum { CFG_MAX_ITEMS=80 };
#include "config_keys.h"
#include "game_directory.h"
static char g_cfg_gamedir[600];   // "spielverzeichnis" (kept when the settings are written)
static int cfg_items(CfgItem *it) {
  int n = 0;
  auto add = [&](const char *k, int *v, int lo, int hi, const char *note) { it[n].key = cfg_english_key(k); it[n].val = v; it[n].lo = lo; it[n].hi = hi; it[n].note = ""; (void)note; n++; };
  add("grafik_detail", &G.opts_gfx, 0, 1, "0 = hoch (Filter), 1 = niedrig");
  add("grafikmodus", &G.gfx_mode, 0, 1, "0 = Revived, 1 = Retro Mode");
  add("startup_mode",&G.startup_mode,0,1,"");
  add("remember_startup_mode",&G.remember_startup_mode,0,1,"");
  add("retro_scale", &G.gfx_scale, 1, 4, "1-4");
  add("retro_output_filter",&G.gfx_output_filter,0,5,"");
  add("ganzzahlige_skalierung", &G.gfx_intscale, 0, 1, "0/1");
  add("revived_options_saved",&G.revived_saved,0,1,"");
  add("revived_texture_filter",&G.revived_filter,0,1,"");
  add("revived_msaa",&G.revived_msaa,0,16,"");
  add("revived_fxaa",&G.revived_fxaa,0,1,"");
  add("revived_frame_rate",&G.revived_fps,0,1,"");
  add("retro_surprise",&G.retro_surprise,0,1,"");
  add("effects_muted",&G.effects_muted,0,1,"");
  add("vehicle_lights",&G.vehicle_lights,0,1,"");
  add("damage_effects",&G.damage_effects,0,1,"");
  add("enhanced_oil",&G.enhanced_oil,0,1,"");
  add("real_time_shadows",&G.real_time_shadows,0,1,"");
  add("modern_explosions",&G.modern_explosions,0,1,"");
  add("missile_effects",&G.missile_effects,0,1,"");
  const char *intensity_keys[]={"shadow_intensity","light_intensity","oil_intensity","explosion_intensity","damage_intensity","missile_intensity"};
  for(int i=0;i<6;i++)add(intensity_keys[i],&G.effect_intensity[i],0,200,"");
  add("msaa", &G.gfx_msaa, 0, 16, "0, 2, 4, 8, 16");
  add("fxaa", &G.gfx_fxaa, 0, 1, "0/1");
  add("vollbild", &G.gfx_full, 0, 1, "0/1");
  add("bildausgabe_rennen", &G.dbg_fpsmode, 0, 1, "Race framerate: 0 = Original, 1 = 60fps");
  add("soundeffekte", &G.opts_sfx, 0, 1, "0/1");
  add("musik_rennen", &G.opts_music, 0, 1, "0/1");
  add("musik_hauptmenue", &G.opts_menumusic, 0, 1, "0/1");
  add("geraeusche_menues", &G.opts_menusfx, 0, 1, "0/1");
  static const char *volume_keys[AUDIO_COUNT]={"sounds_volume","engines_volume","race_music_volume","menu_music_volume","menu_sounds_volume"};
  for(int i=0;i<AUDIO_COUNT;i++)add(volume_keys[i],&g_audio_percent[i],0,100,"");
  add("lance_boyle_videos", &G.opts_clips, 0, 1, "0/1");
  add("runden", &G.opts_laps, 0, 5, "0 = Original (5/5/3), 3 oder 5");
  static const char *PADK[9] = {"pad_gas", "pad_bremse", "pad_links", "pad_rechts", "pad_frontwaffe", "pad_heckwaffe", "pad_frontwaffe_wechseln", "pad_heckwaffe_wechseln", "pad_pause"};
  for (int a = 0; a < PA_COUNT && a < 9; a++) add(PADK[a], &G.pad_map[a], 0, 25, a == 0 ? "Controller binding indices 0-25" : "");
  add("custom_textures",&G.custom_textures,0,1,"");
  add("video_upscaling",&G.video_upscale,0,1,"");add("video_filter",&G.video_filter,0,3,"");
  add("debug_console", &G.dbg_console, 0, 1, "");
  add("debug_fps", &G.dbg_fps, 0, 1, "Debug options (0/1)");
  add("debug_renninfos", &G.dbg_info, 0, 1, "");
  add("debug_fahrbahn_markierungen", &G.dbg_road, 0, 1, "");
  add("debug_ohne_gegner", &G.dbg_noopp, 0, 1, "");
  add("debug_bild_4_3_gestreckt", &G.dbg_stretch, 0, 1, "");
  add("debug_alte_kamera", &G.dbg_camrot, 0, 1, "");
  add("debug_videos_ueberspringen", &G.dbg_noclips, 0, 1, "");
  add("fahrmodell", &G.sim_model, 0, 1, "Simulation: 0 = Original update, 1 = subdivided Revived model");
  add("ki_notfallhilfe", &G.ai_rescue, 0, 1, "AI recovery assistance (modern, not original)");
  add("cheat_geld", &G.cheat_career.money, 0, 1, "Cheats der Meisterschaft (0/1)");
  add("cheat_munition", &G.cheat_career.ammo, 0, 1, "");
  add("cheat_waffen_und_wagen", &G.cheat_career.weapons, 0, 1, "");
  add("cheat_kein_schaden", &G.cheat_career.nodamage, 0, 1, "");
  return n;
}
static void setup_save(void) {
  graphics_options_apply();
  CfgItem it[CFG_MAX_ITEMS]; int n = cfg_items(it);
  static char t[8192]; t[0] = 0;
  str_cat(t, "# " OPENMR2_FULLTITLE " settings\r\n# Format: key = value; # starts a comment.\r\n# frame_rate: original, 60fps. retro_output_filter: point, linear, hq2x, brz, interlacing, crt.\r\n# Other option values use integers.\r\n\r\n", 8192);
  if (g_cfg_gamedir[0]) { str_cat(t, "# Original game data directory\r\ngame_directory = ", 8192); str_cat(t, g_cfg_gamedir, 8192); str_cat(t, "\r\n", 8192); }
  for (int i = 0; i < n; i++) {
    if (it[i].note[0]) { str_cat(t, "# ", 8192); str_cat(t, it[i].note, 8192); str_cat(t, "\r\n", 8192); }
    str_cat(t, it[i].key, 8192); str_cat(t, " = ", 8192);
    if (!str_icmp(it[i].key,"frame_rate")) { static const char *names[]={"original","60fps"};str_cat(t,names[*it[i].val],8192); }
    else if(!str_icmp(it[i].key,"retro_output_filter"))str_cat(t,RETRO_FILTER_KEYS[*it[i].val],8192);
    else str_int(t, 8192, *it[i].val);
    str_cat(t, "\r\n", 8192);
  }
  char p[512]; cfg_path(p, "openmr2_setup.cfg"); size_t vn=0;unsigned char *previous=sys_read_file(p,&vn);char receipt[1500];cfg_verification_lines(previous,vn,receipt,sizeof receipt);
  if(sys_debug_enabled())for(int i=0;i<n;i++){
    char old[64],value[64]="";cfg_value(previous,vn,it[i].key,old,sizeof old);
    if(!str_icmp(it[i].key,"frame_rate"))str_cpy(value,*it[i].val?"60fps":"original",sizeof value);else if(!str_icmp(it[i].key,"retro_output_filter"))str_cpy(value,RETRO_FILTER_KEYS[*it[i].val],sizeof value);else str_int(value,sizeof value,*it[i].val);
    if(str_icmp(old,value)){char event[192]="[event] Setting changed: ";str_cat(event,it[i].key,sizeof event);str_cat(event," = ",sizeof event);str_cat(event,value,sizeof event);sys_log(event);}
  }
  sys_free(previous);str_cat(t,receipt,sizeof t);
  if(sys_write_file(p,t,str_len(t)))sys_log("[event] Settings saved: revived_config/openmr2_setup.cfg");else sys_log("[error] Could not save settings.");
}
static void single_save(void) {   // single race settings (separate from the championship)
  int v[12] = {0x5352, G.single_car, G.single_track, G.single_laps, G.cheat_single.money, G.cheat_single.ammo, G.cheat_single.weapons, G.cheat_single.nodamage};
  char p[512]; cfg_path(p, "openmr2_single.dat"); sys_write_file(p, v, sizeof v);
}
static void single_load(void) {
  G.single_car = 2; G.single_track = 0; G.single_laps = 5; mem_zero(&G.cheat_single, sizeof G.cheat_single);
  char p[512]; cfg_path(p, "openmr2_single.dat");
  size_t n; int *v = (int *)sys_read_file(p, &n);
  if (v && n == 48 && v[0] == 0x5352) {
    G.single_car = v[1] >= 0 && v[1] < NCARS ? v[1] : 2; G.single_track = v[2] >= 0 && v[2] < 6 ? v[2] : 0; G.single_laps = v[3] == 3 ? 3 : 5;
    G.cheat_single.money = v[4]; G.cheat_single.ammo = v[5]; G.cheat_single.weapons = v[6]; G.cheat_single.nodamage = v[7];
  }
  sys_free(v);
}
static void setup_load(void) {
  G.vehicle_lights=G.damage_effects=G.enhanced_oil=1;
  G.real_time_shadows=G.modern_explosions=G.missile_effects=1;
  for(int i=0;i<6;i++)G.effect_intensity[i]=100;
  G.startup_mode=G.remember_startup_mode=0;
  G.gfx_output_filter=0;
  for(int i=0;i<AUDIO_COUNT;i++)g_audio_percent[i]=100;
  G.retro_surprise=0;G.effects_muted=0;G.video_upscale=0;G.video_filter=0;G.revived_saved=0;G.revived_filter=0;G.revived_msaa=4;G.revived_fxaa=0;G.revived_fps=0;
  G.opts_gfx = 0; G.opts_sfx = 1; G.opts_music = 1; G.opts_clips = 1; G.opts_laps = 0;
  G.gfx_mode = 0; G.gfx_scale = 2; G.gfx_msaa = 4; G.gfx_fxaa = 0; G.gfx_full = 0; G.opts_menumusic = 1; G.gfx_intscale = 1; G.opts_menusfx = 1; G.ai_rescue = 1; G.dbg_fpsmode = 0; G.dbg_console = 0;
  for (int a = 0; a < PA_COUNT; a++) G.pad_map[a] = PA_DEFAULT[a];
  G.pad_wait = -1;
  char p[512]; cfg_path(p, "openmr2_setup.cfg");
  size_t n; char *txt = (char *)sys_read_file(p, &n);
  if (txt) {   // plain text settings
    game_directory_from_config((const unsigned char *)txt,n,g_cfg_gamedir,sizeof g_cfg_gamedir);
    CfgItem it[CFG_MAX_ITEMS]; int ni = cfg_items(it); int priority[CFG_MAX_ITEMS]={}; int migrated=0;
    size_t a = n>=3 && (unsigned char)txt[0]==239 && (unsigned char)txt[1]==187 && (unsigned char)txt[2]==191?3:0;
    while (a < n) {
      size_t e = a; while (e < n && txt[e] != '\n') e++;
      char line[1024]; size_t L = e - a; if (L > sizeof line-1) L = sizeof line-1; mem_cpy(line, txt + a, L); line[L] = 0; a = e + 1;
      char *q = line; while (*q == ' ' || *q == '\t') q++;
      if (*q == '#' || !*q) continue;
      char key[64]; int k = 0; while (*q && *q != '=' && *q != ' ' && *q != '\t' && k < 63) key[k++] = *q++; key[k] = 0;
      while (*q == ' ' || *q == '\t' || *q == '=') q++;
      const char *canonical=cfg_english_key(key);int rank=!str_icmp(key,canonical)?2:!str_icmp(key,"debug_bildausgabe")?0:1;
      if (str_icmp(key,canonical))migrated=1;
      str_cpy(key,canonical,sizeof key);
      if (!str_icmp(key, "game_directory")) continue;
      if (!str_icmp(key,"interpolation")) { migrated=1;continue; }
      if (!str_icmp(key,"frame_rate")) {
        char value[32];int j=0;while(q[j] && q[j]!='\r' && q[j]!=' ' && q[j]!='\t' && j<31){value[j]=q[j];j++;}value[j]=0;
        const char *names[]={"original","60fps","unlimited"};
        for(int v=0;v<3;v++)if(!str_icmp(value,names[v])){q=value;value[0]=(char)('0'+v);value[1]=0;break;}
        // Parse while the local symbolic value buffer is alive.
        if(value[0]>='0' && value[0]<='2' && value[1]==0){for(int i=0;i<ni;i++)if(!str_icmp(it[i].key,key) && rank>=priority[i]){*it[i].val=value[0]=='0'?0:1;priority[i]=rank;if(value[0]=='2')migrated=1;}continue;}
        continue;
      }
      if(!str_icmp(key,"retro_output_filter")){
        char value[32];int j=0;while(q[j]&&q[j]!='\r'&&q[j]!=' '&&q[j]!='\t'&&q[j]!='#'&&j<31){value[j]=q[j];j++;}value[j]=0;
        int found=-1;for(int v=0;v<6;v++)if(!str_icmp(value,RETRO_FILTER_KEYS[v]))found=v;
        if(found>=0){for(int i=0;i<ni;i++)if(it[i].val==&G.gfx_output_filter&&rank>=priority[i]){G.gfx_output_filter=found;priority[i]=rank;}continue;}
      }
      int neg = 0; if (*q == '-') { neg = 1; q++; }
      if (*q < '0' || *q > '9') continue;
      int val = 0; while (*q >= '0' && *q <= '9') val = val * 10 + (*q++ - '0'); if (neg) val = -val;
      if (!str_icmp(key, "debug_bildausgabe")) str_cpy(key, "bildausgabe_rennen", 64);   // name in 0.5.1
      for (int i = 0; i < ni; i++) if (!str_icmp(it[i].key, key) && rank>=priority[i]) { if (val < it[i].lo) val = it[i].lo; if (val > it[i].hi) val = it[i].hi; *it[i].val = val;priority[i]=rank; }
    }
    int legacy_volumes[AUDIO_COUNT]={G.opts_sfx,G.opts_sfx,G.opts_music,G.opts_menumusic,G.opts_menusfx};
    for(int b=0;b<AUDIO_COUNT;b++)for(int i=0;i<ni;i++)if(it[i].val==&g_audio_percent[b] && !priority[i])g_audio_percent[b]=legacy_volumes[b]?100:0;
    if (G.opts_laps != 3 && G.opts_laps != 5) G.opts_laps = 0;
    if(migrated){char backup[512];cfg_path(backup,"openmr2_setup.cfg.pre-0.7");size_t bn=0;uint8_t *old=sys_read_file(backup,&bn);if(!old)sys_write_file(backup,txt,n);sys_free(old);}
    sys_free(txt);
    if(migrated)setup_save();
    int upgrade_retro=G.gfx_mode==1 && !G.revived_saved;
    setup_apply(); debug_apply(); single_load();if(upgrade_retro)setup_save();
    return;
  }
  // older versions: binary openmr2_setup.dat (converted to the text file once)
  cfg_path(p, "openmr2_setup.dat");
  int *v = (int *)sys_read_file(p, &n);
  int had_old = v != 0;

  if (v && (n == 96 || n == 160) && v[6] == 0x4d53) {
    G.opts_gfx = v[0]; G.opts_sfx = v[1]; G.opts_music = v[2]; G.opts_clips = v[3]; G.opts_laps = v[4] == 3 || v[4] == 5 ? v[4] : 0;
    for (int a = 0; a < PA_COUNT; a++) if (v[8 + a] >= 0 && v[8 + a] < 26) G.pad_map[a] = v[8 + a];
    if (n == 160) {
      G.cheat_career.money = v[20]; G.cheat_career.ammo = v[21]; G.cheat_career.weapons = v[22]; G.cheat_career.nodamage = v[23];
      G.dbg_fps = v[24]; G.dbg_info = v[25]; G.dbg_road = v[26]; G.dbg_noopp = v[27]; G.dbg_stretch = v[28]; G.dbg_camrot = v[29]; G.dbg_noclips = v[30];
      if (v[7] >= 1 && v[7] <= 3) G.dbg_fpsmode = v[7] == 1 ? 0 : 1;
      if (v[37] == 0x4d54) { G.gfx_mode = v[31] & 1; G.gfx_scale = v[32] >= 1 && v[32] <= 4 ? v[32] : 2; G.gfx_msaa = v[33]; G.gfx_fxaa = v[34]; G.gfx_full = v[35]; G.opts_menumusic = v[36]; if (v[38]) G.gfx_intscale = v[38] - 1; if (v[39]) G.opts_menusfx = v[39] - 1; }
    }
  }
  sys_free(v);
  g_audio_percent[0]=g_audio_percent[1]=G.opts_sfx?100:0;g_audio_percent[2]=G.opts_music?100:0;g_audio_percent[3]=G.opts_menumusic?100:0;g_audio_percent[4]=G.opts_menusfx?100:0;
  setup_save();                  // the text file is written right away (defaults or the converted old values)
  (void)had_old; // retain the legacy binary settings as a migration backup
  setup_apply(); debug_apply(); single_load();
}

// ------------------------------------------------------------------ state changes
static void go(int state) {
  if(sys_debug_enabled() && G.state!=state){char event[160]="[event] State: ";str_cat(event,game_state_name(G.state),sizeof event);str_cat(event," -> ",sizeof event);str_cat(event,game_state_name(state),sizeof event);sys_log(event);}
  if (state != GS_RACE && (G.state == GS_RACE || state == GS_GAMEOVER)) race_end_sounds();
  G.state = state; G.sel = 0; G.t = 0; G.menu_click = -1;
  if (state != GS_TRACKINFO && !G.opts_menumusic) menu_music_stop();   // the loading music only belongs to the loading screen
  switch (state) {
    case GS_MENU: game_set_bg("DATA640/MENUFOND.HLZ"); menu_music_start(); break;
    case GS_CARSHOP: game_set_bg("DATA640/CARSHOP.HLZ"); menu_music_start(); if (G.shop_car < 0 || G.shop_car >= NCARS) G.shop_car = PF.car >= 0 ? PF.car : 0; break;
    case GS_WEAPONSHOP: game_set_bg("DATA640/SHOPFOND.HLZ"); menu_music_start(); G.repair_amount = 0; break;
    case GS_TRACKINFO: {
      pic_free(&G.bg);
      char p[64]; int ti = PF.single ? G.single_track : PF.track;
      str_cpy(p, TRACKS[ti].cd, 64); int k = (int)str_len(p); while (k > 0 && p[k - 1] != '/') k--;
      char q[64]; str_cpy(q, p, 64); q[k] = 0; str_cat(q, "D_", 64); str_cat(q, p + k, 64); str_cat(q, ".HLZ", 64);
      pic_load_cd(&G.bg, q);
      loading_music_start();
      break;
    }
    case GS_RESULTS: game_set_bg("DATA640/CLASS.HLZ"); menu_music_start(); if (G.vox_ok && G.opts_clips) { voice_play(&G.vox, 1.0f); MX.v[1].rate = 44100.0 / 22050.0; } break;
    case GS_PRIZE: game_set_bg("DATA640/PORTES.HLZ"); menu_music_start(); break;
    case GS_GAMEOVER: snd_stop_all_sfx();music_stop();G.menu_music_on=0;voice_stop();sys_audio_flush();game_set_bg("DATA640/PERDU.HLZ"); menu_music_stop(); msfx(S_SCREAM, 0.8f); hiscore_offer(); PF.active = 0; save_game(); break;
    case GS_HISCORE: game_set_bg("DATA640/HISCORE2.HLZ"); menu_music_start(); if (PF.active && G.hi_edit < 0) hiscore_offer(); break;
    case GS_EXTRAS: case GS_OPTIONS: case GS_RETRO_SOUND: game_set_bg("DATA640/OPTION3.HLZ"); menu_music_start(); break;
    case GS_SAVELOAD: game_set_bg("DATA640/SAVE2.HLZ"); menu_music_start(); G.slot = 0; break;
    case GS_REPLAYS: game_set_bg("DATA640/REPLAYER.HLZ"); menu_music_start(); G.slot = 0; break;
    case GS_TRACKSELECT: game_set_bg("DATA640/FOND_SEL.HLZ"); menu_music_start(); break;
    case GS_MESSAGE: break;
  }
}

// files of earlier versions (MR2GL, OpenMR2 up to 0.5.0 in the game folder) are moved into revived_config once
static void migrate_one(const char *oldn, const char *newn) {
  char po[512], pn[512]; game_path(po, oldn); cfg_path(pn, newn);
  size_t n; uint8_t *d = sys_read_file(pn, &n); if (d) { sys_free(d); return; }
  d = sys_read_file(po, &n); if (!d) return;
  int ok = sys_write_file(pn, d, n); sys_free(d);
  (void)ok; // import by copying: never remove saves or settings from the original installation
}
static void migrate_old_files(void) {
  migrate_one("mr2gl_save.dat", "openmr2_save.dat"); migrate_one("mr2gl_hiscore2.dat", "openmr2_hiscore2.dat");
  static const char *names[] = {"openmr2_save.dat", "openmr2_hall.dat", "openmr2_hiscore2.dat", "openmr2_setup.dat", "openmr2_single.dat"};
  for (unsigned k = 0; k < sizeof names / sizeof names[0]; k++) migrate_one(names[k], names[k]);
  // 0.5.1 - 0.6.0 kept revived_config in the game folder; it is now next to OpenMR2.exe
  static const char *more[] = {"openmr2_setup.cfg", "openmr2_save.dat", "openmr2_hall.dat", "openmr2_single.dat"};
  for (unsigned k = 0; k < sizeof more / sizeof more[0]; k++) { char a[96] = "revived_config/"; str_cat(a, more[k], 96); migrate_one(a, more[k]); }
  for (int i = 1; i <= 20; i++) { char nn[8] = ""; if (i < 10) str_cat(nn, "0", 8); str_int(nn, 8, i); str_cat(nn, ".dat", 8);
    char a[96] = "revived_config/openmr2_save_", b[64] = "openmr2_save_", c[96] = "revived_config/openmr2_replay_", d[64] = "openmr2_replay_";
    str_cat(a, nn, 96); str_cat(b, nn, 64); str_cat(c, nn, 96); str_cat(d, nn, 64); migrate_one(a, b); migrate_one(c, d); }
  for (int i = 1; i <= 20; i++) {
    char a[64] = "mr2gl_save_", b[64] = "openmr2_save_", c[64] = "mr2gl_replay_", d[64] = "openmr2_replay_";
    char nn[8] = ""; if (i < 10) str_cat(nn, "0", 8); str_int(nn, 8, i); str_cat(nn, ".dat", 8);
    str_cat(a, nn, 64); str_cat(b, nn, 64); str_cat(c, nn, 64); str_cat(d, nn, 64);
    migrate_one(a, b); migrate_one(c, d); migrate_one(b, b); migrate_one(d, d);
  }
}

static void game_init(void) {
  mem_zero(&G, sizeof G);
  G.opts_laps = 0; G.shop_car = 0; G.menu_click = -1; G.hi_edit = -1;
  MX.music_vol = 0.55f; MX.sfx_vol = 0.9f; MX.voice_vol = 1.0f;
  for (int i = 0; i < 9; i++) { static const char *nm[9] = {"1", "2", "3", "4", "5", "6", "10", "8", "7"}; char p[64] = "DATA640/MENU"; str_cat(p, nm[i], 64); str_cat(p, ".SPR", 64); spr_load(&G.menu_spr[i], p); }
  spr_load(&G.shop_spr, "DATA640/SHOP.SPR"); spr_load(&G.shop2_spr, "DATA640/SHOP2.SPR"); spr_load(&G.icon_spr, "DATA640/ICON.SPR");
  spr_load(&G.save_spr, "DATA640/SAVE.SPR"); spr_load(&G.repl_spr, "DATA640/REPL_P.SPR");
  spr_load(&G.chk_on, "DATA640/VAL.SPR"); spr_load(&G.chk_off, "DATA640/BUT.SPR"); spr_load(&G.exit_spr, "DATA640/EXIT.SPR");
  spr_load(&G.cursor_spr, "SOURIS/FLECHE.SPR"); pic_load(&G.fleche, "SOURIS/FLECHE.HLZ");
  for (int i = 0; i < 6; i++) { char p[64] = "DATA640/TRACK"; str_int(p, 64, i + 1); str_cat(p, ".SPR", 64); spr_load(&G.track_spr[i], p); }
  font_load(&g_font_big, "TEXTE/FONTE21.HLZ"); font_load(&g_font_small, "TEXTE/CLR6_13.HLZ"); font_load(&g_font_mid, "TEXTE/LATIN11.HLZ");
  font_load(&g_font_h14, "TEXTE/HNDEL14.HLZ"); font_load(&g_font_h10, "TEXTE/HNDEL10.HLZ");
  pic_load(&g_racemenu, "DATA640/MENU0.HLZ");
  spr_load(&g_pausedrum[0], "DATA640/MENUA.SPR"); spr_load(&g_pausedrum[1], "DATA640/MENUB.SPR"); spr_load(&g_pausedrum[2], "DATA640/MENUC.SPR");
  migrate_old_files(); hiscore_load(); setup_load();
  if(sys_startup_mode()>=0)graphics_mode_set(sys_startup_mode());
  sfx_load_all();menu_music_load();
  PF.car = -1;
  G.single_track = 0;
}
static void load_vox(void) {   // Lance Boyle's comment for the classification (as in M.EXE)
  // index = (place-1)/2*2 + heat: D1/D2 = 1st/2nd place, C = 3rd/4th, B = 5th/6th, A = 7th/8th; X1 after heat 1,
  // X2 after heat 2, none after heat 3. The files are 16-bit mono at 44100 Hz.
  static const char *vox[8] = {"VOX/D1.RAW", "VOX/D2.RAW", "VOX/C1.RAW", "VOX/C2.RAW", "VOX/B1.RAW", "VOX/B2.RAW", "VOX/A1.RAW", "VOX/A2.RAW"};
  sound_free(&G.vox); G.vox_ok = 0;
  if(!audio_enabled(AUDIO_SOUNDS))return;
  int heat = PF.single ? 0 : PF.heat;
  if (heat >= 2) return;
  int place = player().place; if (place < 1) place = 1; if (place > 8) place = 8;
  if (RC.car[0].out) place = 8;
  size_t n; uint8_t *d = iso_load(vox[(place - 1) / 2 * 2 + heat], &n);
  if (!d) return;
  G.vox.pcm = (int16_t *)d; G.vox.frames = (int)(n / 2); G.vox.ch = 1; G.vox_ok = 1;
}

// a 3D model in a rectangle of the 640x480 screen, turning (shop items)
static void draw_model_box(int W, int H, Model *m, double angle, float x, float y, float w, float h) {
  if (!m) return;
  int vx, vy, vw, vh; vrect_viewport(W, H, x, y, w, h, &vx, &vy, &vw, &vh);
  glViewport(vx, vy, vw, vh);
  glEnable(GL_DEPTH_TEST); glClear(GL_DEPTH_BUFFER_BIT);
  V3 ext = m->bmax - m->bmin; float r = m_sqrt(dot(ext, ext)) * 0.5f; if (r < 1e-4f) return;
  M4 proj = m4_persp(30.0f * PI / 180.0f, w / h, r * 0.1f, r * 20);
  V3 eye = v3(0, r * 0.7f, r * 3.3f);
  M4 view = m4_look(eye, v3(0, 0, 0), v3(0, 1, 0));
  M4 vp = m4_mul(proj, view);
  glUseProgram(R.prog_model);
  glUniformMatrix4fv(R.um_vp, 1, GL_FALSE, vp.m); glUniform3f(R.um_cam, eye.x, eye.y, eye.z);
  glUniform1i(R.um_tex, 0); glUniform1f(R.um_lit, 1.0f); glUniform1f(R.um_gain, 1.0f); glUniform1f(R.um_gray, 0.0f);
  V3 c = (m->bmin + m->bmax) * 0.5f;
  draw_model_mv(*m, m4_mul(m4_roty((float)angle), m4_trans(v3(-c.x, -c.y, -c.z))), 0, 0);
  glBindVertexArray(0);
}

// ------------------------------------------------------------------ main menu (column with 8 drums)
static void menu_action(int act) {
  switch (act) {
    case 0:   // championship
      if (!PF.active) { profile_new(); G.loaded = 0; }
      if (PF.car < 0) { go(GS_CARSHOP); break; }
      PF.field[0] = PF.car;
      if (PF.heat == 0) { clip_queue(TRACKS[PF.track].intro_clip); clip_play_then(GS_TRACKINFO); }
      else go(GS_TRACKINFO);
      break;
    case 1: if (!PF.active) profile_new(); go(GS_CARSHOP); break;     // wrench: car shop (then weapons)
    case 2: go(GS_SAVELOAD); break;
    case 3: go(GS_OPTIONS); break;
    case 4: go(GS_REPLAYS); break;
    case 5: go(GS_HISCORE); break;
    case 6: credits_play_then(GS_MENU); break;   // MegaRace logo: credits
    case 7: G.state = GS_QUITDLG; G.sel = 0; G.t = 0; break;
  }
}
static void screen_menu(const Input *in) {
  cv_pic(G.bg, 0, 0, G.bg.w <= 320 ? 2 : 1);
  // items: 0 championship, 1 car shop, 2 load/save, 3 setup, 4 replays, 5 high scores, 6 credits, 7 black flag
  int hover = -1;
  for (int i = 0; i < 8; i++) { SprFile &s = G.menu_spr[i]; if (s.n && spr_hit(s.f[0])) hover = i; }
  if (in->pressed[K_DOWN]) { G.sel = (G.sel + 2) % 8; G.kbd = 1; } if (in->pressed[K_UP]) { G.sel = (G.sel + 6) % 8; G.kbd = 1; }
  if (in->pressed[K_RIGHT] || in->pressed[K_LEFT]) { G.sel ^= 1; G.kbd = 1; }
  if (in->mouse_dx || in->mouse_dy) { G.kbd = 0; if (hover >= 0) G.sel = hover; }
  int active = G.menu_click >= 0 ? G.menu_click : hover >= 0 ? hover : G.kbd ? G.sel : -1;
  for (int i = 0; i < 8; i++) {
    SprFile &s = G.menu_spr[i]; if (!s.n) continue;
    int f = 0;
    if (i == active) { G.anim_t += 0; f = (int)(G.t * 18) % s.n; }   // the drum under the mouse turns (as in the original)
    cv_spr(s.f[f], G.bg.pal, 0, 0, 1);
  }
  cv_text(g_font_small, 636, 2, OPENMR2_NAME " v" OPENMR2_VERSION, GREY, 2, 1);
  {   // status line under the tower (UI02): next track, round and start position (the first race starts last)
    int tr = PF.active ? PF.track : 0, ht = PF.active ? PF.heat : 0;
    int pos = ht > 0 && PF.last_place > 0 ? PF.last_place : 8;
    char t[160] = ""; str_cpy(t, ui_translate("Fertigmachen f\xfcrs Rennen : "), 160); str_cat(t, ui_translate(TRACKS[tr % 6].name), 160); str_cat(t, ui_translate(" , Runde : "), 160); str_int(t, 160, ht + 1);
    str_cat(t, ui_translate(" , Position : "), 160); str_int(t, 160, pos);
    const Font *fs[3] = {&g_font_h14, &g_font_mid, &g_font_small}; int fi = 0;
    while (fi < 2 && cv_text_w(*fs[fi], t, 1) > 616) fi++;
    int w = cv_text_w(*fs[fi], t, 1);
    cv_rect(320 - w / 2 - 8, 448, w + 16, 26, rgba(20, 14, 8, 200));
    cv_text(*fs[fi], 320, fi == 0 ? 451 : 454, t, ORANGE, 1, 1);
  }
  if (G.menu_click >= 0) {
    G.menu_click_t += 1.0 / 60;
    if (G.menu_click_t >= 0.3) { int a = G.menu_click; G.menu_click = -1; menu_action(a); }
    return;
  }
  int act = -1;
  if (G.click && hover >= 0) act = hover;
  if (in->pressed[K_ENTER]) act = G.sel;
  if (in->pressed[K_ESC]) act = 7;
  // a key, button or click that was still held from the intro (or a skipped video) must not open a screen:
  // the menu accepts input only after it has been shown for a moment
  if (G.t < 0.6) act = -1;
  if (act >= 0) { msfx(S_MENU1, 0.8f); G.menu_click = act; G.menu_click_t = 0; }
}

// Event-driven inventory diagnostics: never poll the profile in the frame loop.
static void debug_purchase(const char *action,const char *item,int amount,int quantity=1){
  if(!sys_debug_enabled())return;
  char m[512]="[event] ";str_cat(m,action,sizeof m);str_cat(m,": ",sizeof m);str_cat(m,ui_debug_text(item),sizeof m);
  str_cat(m,"; quantity = ",sizeof m);str_int(m,sizeof m,quantity);str_cat(m,"; amount = ",sizeof m);str_int(m,sizeof m,amount);
  str_cat(m,"; balance = ",sizeof m);str_int(m,sizeof m,PF.money);sys_log(m);
}

// ------------------------------------------------------------------ car shop ("Wagen waehlen")
static void screen_carshop(int W, int H, const Input *in, double dt) {
  cv_pic(G.bg, 0, 0, 1);
  G.turn=turntable_advance(G.turn,dt);
  title_box(318, 8, "Wagen w\xe4hlen");
  int c = G.shop_car;
  char t[256];
  cv_text(g_font_h14, 320, 288, CARS[c].name, BEIGE, 1, 1);
  t[0] = 0; str_int(t, 256, money_shown()); cv_text(g_font_mid, 70, 400, t, WHITE, 2, 1);
  int avail = car_available(c);
  t[0] = 0; str_cat(t, ui_translate("Geschwindigkeit : "), 256); str_int(t, 256, CARS[c].mph); str_cat(t, ui_translate(" Mph\nKraftstoff : "), 256); str_cat(t, ui_translate(CARS[c].fuel), 256);
  str_cat(t, ui_translate("\nMotor      : "), 256); str_cat(t, CARS[c].engine, 256);
  str_cat(t, ui_translate("\n\nPreis : "), 256); str_int(t, 256, CARS[c].price);
  if (!avail) str_cat(t, ui_translate("   (noch nicht erh\xe4ltlich)"), 256);
  if (PF.car == c) str_cat(t, ui_translate("   (dein Wagen)"), 256);
  text_box(198, 392, t, avail ? BEIGE : 0xff707070u);
  // buttons: 0 left, 1 right, 2 back, 3 buy
  int b = -1; for (int i = 0; i < 4 && i < G.shop_spr.n; i++) if (spr_hit(G.shop_spr.f[i])) b = i;
  if (b >= 0 && in->lmb) cv_spr(G.shop_spr.f[b], G.bg.pal, 0, 0, 1);
  int act = G.click ? b : -1;
  if (in->pressed[K_LEFT]) act = 0; if (in->pressed[K_RIGHT]) act = 1; if (in->pressed[K_ESC]) act = 2; if (in->pressed[K_ENTER]) act = 3;
  if (act == 0) { G.shop_car = (c + NCARS - 1) % NCARS; msfx(S_MENU2, 0.7f); }
  if (act == 1) { G.shop_car = (c + 1) % NCARS; msfx(S_MENU2, 0.7f); }
  if (act == 2) { msfx(S_MENU1, 0.7f); if (PF.car >= 0) go(GS_WEAPONSHOP); else go(GS_MENU); }
  if (act == 3) {
    if (PF.car == c) { go(GS_WEAPONSHOP); }
    else if (!avail) show_message("Dieser Wagen ist noch nicht erh\xe4ltlich.", GS_CARSHOP);
    else {
      int tradein = 0;   // M.EXE: the full price of the new car, the old one is not taken back
      if (!can_pay(CARS[c].price - tradein)) show_message("Sorry, nicht genug Geld", GS_CARSHOP);
      else {
        int first = PF.car < 0;
        pay(CARS[c].price - tradein); debug_purchase("Vehicle purchased",CARS[c].name,CARS[c].price-tradein); PF.car = c; PF.field[0] = c; PF.damage = 0;
        msfx(S_MENU3, 0.8f);
        if (first) default_loadout(&PF.lo);
        save_game(); go(GS_WEAPONSHOP);
      }
    }
  }
  draw_cursor();
  canvas_draw(W, H);
  draw_turntable(W, H, c, G.turn, 130, 100, 380, 200, TRACKS[PF.active ? PF.track : 0].set);
}

// ------------------------------------------------------------------ weapon & repair shop
// icons of the shelf (ICON.SPR 0..10): Hi-Trax, Boomer, Kard, mine, oil, shield, L9, L6, L3, mine launcher, oil launcher
static const int SHOP_ITEMS[11] = {W_HITRAX, W_BOOMER, W_KARD, W_MINE, W_OIL, W_SHIELD, -W_HITRAX, -W_BOOMER, -W_KARD, -W_MINE, -W_OIL};   // negative = launcher
static const char *SHOP_MODELS[11] = {"*OL_MIS.M3D", "*OL_MIS1.M3D", "*OL_MIS2.M3D", "*OL_MINE.M3D", "*OL_OIL.M3D", "*ARMA03.M3D", "*GLANCER.M3D", "*MLANCER.M3D", "*PLANCE01.M3D", "*DROPMINE.M3D", "*DROPOIL.M3D"};
static int shop_icon_of(int item) { for (int i = 0; i < 11; i++) if (SHOP_ITEMS[i] == item) return i; return 0; }
static const int SHOP_ICON_DY = 224;   // the shelf is at the bottom of the screen in the original
static void shop_icon_rect(int index,const Sprite &s,int *x,int *y,int *w,int *h){
  if(G.gfx_mode==0&&g_revived_ui){*x=17+index*56;*y=196+SHOP_ICON_DY;*w=50;*h=41;}
  else{*x=s.x;*y=s.y+SHOP_ICON_DY;*w=s.w;*h=s.h;}
}
static int lo_find(const Loadout &lo, int w, int front) { for (int i = 0; i < 2; i++) if ((front ? lo.front[i] : lo.rear[i]) == w) return i; return -1; }
static void shop_buy(int item) {
  Loadout &lo = PF.lo;
  if (item == W_SHIELD) {   // buy (1500, full shield) or sell (750) the shield
    if (lo.shield) { PF.money += WEAPONS[W_SHIELD].launcher_price / 2; lo.shield = 0; debug_purchase("Upgrade sold","Shield",WEAPONS[W_SHIELD].launcher_price/2); msfx(S_MENU2, 0.7f); return; }
    if (!can_pay(WEAPONS[W_SHIELD].launcher_price)) { show_message("Sorry, nicht genug Geld", GS_WEAPONSHOP); return; }
    pay(WEAPONS[W_SHIELD].launcher_price); lo.shield = 100; debug_purchase("Upgrade purchased","Shield",WEAPONS[W_SHIELD].launcher_price); msfx(S_MENU3, 0.8f); return;
  }
  int launcher = item < 0; int w = launcher ? -item : item; int front = w <= W_HITRAX;
  int slot = lo_find(lo, w, front);
  if (launcher) {
    if (slot >= 0) {   // sell the launcher (half price, the ammunition is lost)
      PF.money += WEAPONS[w].launcher_price / 2;
      if (front) { lo.front[slot] = W_NONE; lo.fammo[slot] = 0; } else { lo.rear[slot] = W_NONE; lo.rammo[slot] = 0; }
      debug_purchase("Weapon launcher sold",WEAPONS[w].launcher,WEAPONS[w].launcher_price/2);
      msfx(S_MENU2, 0.7f); return;
    }
    int free = front ? (lo.front[0] == W_NONE ? 0 : lo.front[1] == W_NONE ? 1 : -1) : (lo.rear[0] == W_NONE ? 0 : lo.rear[1] == W_NONE ? 1 : -1);
    if (free < 0) { show_message(front ? "Beide Pl\xe4tze f\xfcr Frontwaffen sind belegt.\nZuerst einen Werfer verkaufen." : "Beide Pl\xe4tze f\xfcr Heckwaffen sind belegt.\nZuerst einen Werfer verkaufen.", GS_WEAPONSHOP); return; }
    if (!can_pay(WEAPONS[w].launcher_price)) { show_message("Sorry, nicht genug Geld", GS_WEAPONSHOP); return; }
    pay(WEAPONS[w].launcher_price);
    if (front) { lo.front[free] = w; lo.fammo[free] = LAUNCHER_FILL[w]; } else { lo.rear[free] = w; lo.rammo[free] = LAUNCHER_FILL[w]; }   // M.EXE fill: 12 / 10 / 4 / 4 / 4
    debug_purchase("Weapon launcher purchased",WEAPONS[w].launcher,WEAPONS[w].launcher_price);
    msfx(S_MENU3, 0.8f);
  } else {
    if (slot < 0) { show_message("Zuerst den passenden Werfer kaufen.", GS_WEAPONSHOP); return; }
    int &ammo = front ? lo.fammo[slot] : lo.rammo[slot];
    if (ammo >= WEAPONS[w].capacity) { show_message("Der Werfer ist voll.", GS_WEAPONSHOP); return; }
    if (!can_pay(WEAPONS[w].ammo_price)) { show_message("Sorry, nicht genug Geld", GS_WEAPONSHOP); return; }
    pay(WEAPONS[w].ammo_price); ammo++; debug_purchase("Ammunition purchased",WEAPONS[w].name,WEAPONS[w].ammo_price); msfx(S_MENU4, 0.6f);
  }
}
static void shop_info(char *t, int item) {   // the texts of the original info panel
  int w = item < 0 ? -item : item; t[0] = 0;
  if (item == W_SHIELD) { str_cat(t, ui_translate("Schutzschild SH-24\nSuperschutzschild\nModell Sh-24"), 512); return; }
  if (item < 0) {
    str_cat(t, ui_translate(WEAPONS[w].launcher), 512); str_cat(t, ui_translate(".\nKapazit\xe4t : "), 512); str_int(t, 512, WEAPONS[w].capacity);
    str_cat(t, ui_translate("\nMunition  : "), 512); str_cat(t, ui_translate(WEAPONS[w].name), 512); return;
  }
  if (w <= W_HITRAX) {
    str_cat(t, ui_translate(WEAPONS[w].name), 512); str_cat(t, ui_translate("\nVerfolgung   : "), 512); str_cat(t, ui_translate(WEAPONS[w].homing ? "Ja" : "Nein"), 512);
    str_cat(t, ui_translate("\nWerfer       : TRax "), 512); str_cat(t, w == W_KARD ? "L3" : w == W_BOOMER ? "L6" : "L9", 512);
    str_cat(t, ui_translate("\nSprengstoff  : "), 512); str_cat(t, ui_translate(WEAPONS[w].explosive), 512); str_cat(t, ui_translate("\nSchaden      : "), 512); str_cat(t, ui_translate(WEAPONS[w].dmgtxt), 512);
  } else if (w == W_MINE) str_cat(t, ui_translate(".Mine Th-1.\nWerfer       : MKII\nSprengstoff  : Thorium\nSchaden      : Hoch"), 512);
  else str_cat(t, ui_translate(".\xd6l Sk-N.\nWerfer : \xf6lwerfer Od.V\nDauer  : 2 Runden"), 512);
}
static int shop_price(int item) { int w = item < 0 ? -item : item; return item < 0 || item == W_SHIELD ? WEAPONS[w].launcher_price : WEAPONS[w].ammo_price; }
static const char *shop_tip(int item) { int w = item < 0 ? -item : item; return item < 0 ? WEAPONS[w].lshort : WEAPONS[w].shortname; }

static void screen_weaponshop(int W, int H, const Input *in, double dt) {
  cv_pic(G.bg, 0, 0, 1);
  G.turn=turntable_advance(G.turn,dt);
  char t[512];
  // shelf at the bottom
  int hover = -1;
  for (int i = 0; i < 11 && i < G.icon_spr.n; i++) { const Sprite &s = G.icon_spr.f[i];int x,y,w,h;shop_icon_rect(i,s,&x,&y,&w,&h);if(hit(x,y,w,h))hover=i; }
  if (G.click && hover >= 0) { G.wsel = hover; msfx(S_MENU2, 0.6f); }
  if (in->pressed[K_LEFT]) G.wsel = (G.wsel + 10) % 11; if (in->pressed[K_RIGHT]) G.wsel = (G.wsel + 1) % 11;
  for (int i = 0; i < 11 && i < G.icon_spr.n; i++){const Sprite &s=G.icon_spr.f[i];if(G.gfx_mode==0&&g_revived_ui){int x,y,w,h;shop_icon_rect(i,s,&x,&y,&w,&h);char key[512];texture_sprite_key(key,s.texture_source,s.frame,G.bg.pal);if(g_dump_active)texture_dump_sprite(s,G.bg.pal);if(texture_canvas_image(key,x,y,w,h))continue;}cv_spr(s,G.bg.pal,0,SHOP_ICON_DY,1);}
  { const Sprite &s = G.icon_spr.f[G.wsel < G.icon_spr.n ? G.wsel : 0]; uint32_t c = rgba(200, 170, 90, 255);int x,y,w,h;shop_icon_rect(G.wsel,s,&x,&y,&w,&h);x-=3;y-=3;w+=6;h+=6;
    cv_rect(x, y, w, 2, c); cv_rect(x, y + h - 2, w, 2, c); cv_rect(x, y, 2, h, c); cv_rect(x + w - 2, y, 2, h, c); }
  // equipment: launchers with their ammunition (front box), shield (middle box), rear box
  Loadout &lo = PF.lo;
  for (int i = 0; i < 2; i++) {
    if (lo.front[i] && G.icon_spr.n > 21) { cv_spr(spr_at(G.icon_spr.f[11 + shop_icon_of(-lo.front[i])], 351, 21 + i * 35), G.bg.pal, 0, 0, 1); t[0] = 0; str_int(t, 512, lo.fammo[i]); cv_text(g_font_small, 398, 31 + i * 35, t, ORANGE, 0, 1); }
    if (lo.rear[i] && G.icon_spr.n > 21) { cv_spr(spr_at(G.icon_spr.f[11 + shop_icon_of(-lo.rear[i])], 351, 148 + i * 35), G.bg.pal, 0, 0, 1); t[0] = 0; str_int(t, 512, lo.rammo[i]); cv_text(g_font_small, 398, 158 + i * 35, t, ORANGE, 0, 1); }
  }
  if (lo.shield && G.icon_spr.n > 21) cv_spr(spr_at(G.icon_spr.f[16], 395, 100), G.bg.pal, 0, 0, 1);
  // info panel
  int item = SHOP_ITEMS[G.wsel];
  shop_info(t, item); cv_text(g_font_small, 191, 249, t, BEIGE, 0, 1);
  cv_text(g_font_small, 191, 364, "Preis:", BEIGE, 0, 1);
  t[0] = 0; str_int(t, 512, shop_price(item)); cv_text(g_font_small, 301, 364, t, ORANGE, 0, 1);
  cv_text(g_font_small, 191, 384, shop_tip(hover >= 0 ? SHOP_ITEMS[hover] : item), ORANGE, 0, 1);
  t[0] = 0; str_int(t, 512, money_shown()); cv_text(g_font_mid, 126, 255, t, WHITE, 2, 1);
  // repair (M.EXE): the hull is kept as 0..256 between races; each step adds 4 and costs 169.
  // yellow = state of the car now, green = after the planned repair; cost in the box below
  int hull = (int)((100 - PF.damage) * 2.56 + 0.5); if (hull > 256) hull = 256; if (hull < 0) hull = 0;
  int maxsteps = (256 - hull + 3) / 4;
  if (G.repair_amount > maxsteps) G.repair_amount = maxsteps;
  int after = hull + 4 * G.repair_amount; if (after > 256) after = 256;
  { int h0 = 57, y0 = 256;
    int hy = h0 * hull / 256, hg = h0 * after / 256;
    cv_rect(492, y0, 22, h0, rgba(10, 10, 10, 255)); cv_rect(523, y0, 24, h0, rgba(10, 10, 10, 255));
    cv_rect(492, y0 + h0 - hy, 22, hy, rgba(250, 210, 10, 255)); cv_rect(523, y0 + h0 - hg, 24, hg, rgba(20, 160, 30, 255)); }
  t[0] = 0; str_int(t, 512, G.repair_amount * 169); cv_text(g_font_mid, 566, 372, t, WHITE, 2, 1);
  // buttons (SHOP2.SPR): 0 buy/sell, 1 back, 2 repair less, 3 repair more, 4 repair, 5 car shop
  int b = -1; for (int i = 0; i < 6 && i < G.shop2_spr.n; i++) if (spr_hit(G.shop2_spr.f[i])) b = i;
  if (b >= 0 && in->lmb) cv_spr(G.shop2_spr.f[b], G.bg.pal, 0, 0, 1);
  int act = G.click ? b : -1;
  if (in->pressed[K_ENTER]) act = 0; if (in->pressed[K_ESC]) act = 1; if (in->pressed[K_R]) act = 4;
  if (in->pressed[K_UP]) act = 3; if (in->pressed[K_DOWN]) act = 2;
  if (act == 0) shop_buy(item);
  if (act == 1) { msfx(S_MENU1, 0.7f); if (PF.active) save_game(); go(GS_MENU); }
  if (act == 3) { G.repair_amount += in->down[K_UP] && G.t > 0 ? 1 : 1; if (G.repair_amount > maxsteps) G.repair_amount = maxsteps; }
  if (act == 2) { G.repair_amount -= 1; if (G.repair_amount < 0) G.repair_amount = 0; }
  if (act == 4) {
    int cost = G.repair_amount * 169;
    if (cost <= 0) {}
    else if (!can_pay(cost)) show_message("Sorry, nicht genug Geld", GS_WEAPONSHOP);
    else { pay(cost); PF.damage = 100 - after / 2.56; if (PF.damage < 0.01) PF.damage = 0; G.repair_amount = 0; debug_purchase("Vehicle repaired",CARS[PF.car].name,cost); msfx(S_MENU3, 0.8f); }
  }
  if (act == 5) { msfx(S_MENU1, 0.7f); go(GS_CARSHOP); }
  draw_cursor();
  canvas_draw(W, H);
  draw_turntable(W, H, PF.car, G.turn, 20, 20, 300, 170, TRACKS[PF.active ? PF.track : 0].set);
  int mi = G.wsel; if (!G.item_models[mi]) G.item_models[mi] = load_model_from("FAC", SHOP_MODELS[mi]);
  draw_model_box(W, H, G.item_models[mi], G.t * 1.2, 250, 296, 210, 70);
}

// Both exports and track loading use the same original progress bar and colors.
static void draw_loading_bar(float fraction,const char *label){
  if(fraction<0)fraction=0;if(fraction>1)fraction=1;
  const int bx=170,by=420,bw=300,bh=14;
  cv_rect(bx-2,by-2,bw+4,bh+4,rgba(150,140,120,255));cv_rect(bx,by,bw,bh,rgba(20,18,16,255));
  cv_rect(bx,by,(int)(bw*fraction),bh,ORANGE);cv_text(g_font_small,320,by-18,label,BEIGE,1,1);
}
static void texture_dump_finished_sound(){msfx(S_MENU2,0.7f);sys_log("[event] Texture export finished; Apply sound played (if menu effects are enabled).");}
static int texture_dump_frame_tick(){texture_dump_step();return A.export_textures && g_dump_complete;}
static void texture_dump_overlay_draw(int W,int H){
  if(!g_dump_active && !g_dump_overlay_until)return;
  if(!g_dump_active && sys_time()>=g_dump_overlay_until){g_dump_overlay_until=0;return;}
  texture_canvas_reset();mem_zero(g_cv,CW*CH*4);g_cv_dirty=1;screen_compute(W,H);
  float fraction=g_dump_complete?1.0f:g_dump_assets.n?(float)g_dump_index/g_dump_assets.n:0;
  char label[160]="";str_cpy(label,g_dump_complete?"Texture export complete: ":"Dumping textures: ",sizeof label);str_int(label,sizeof label,(int)(fraction*100));str_cat(label,"%  |  PNGs: ",sizeof label);str_int(label,sizeof label,g_dump_pngs);
  if(g_dump_errors){str_cat(label,"  |  errors: ",sizeof label);str_int(label,sizeof label,g_dump_errors);}
  cv_rect(100,396,440,44,rgba(10,10,12,235));draw_loading_bar(fraction,label);canvas_draw(W,H);
}

// loading screen of the track with a loading bar (the track video is read in steps)
static IsoStream g_ld; static int g_ld_state;   // 0 start, 1 reading, 2 done (start the race next frame)
static const double LOAD_MIN_TIME = 3.5;   // the loading screen stays at least this long (so the bar can be seen)
static void screen_trackinfo(const Input *in) {
  if (G.bg.px) cv_pic(G.bg, 0, 0, G.bg.w <= 320 ? 2 : 1); else cv_clear(rgba(0, 0, 0, 255));
  int ti = PF.single ? G.single_track : PF.track;
  char t[200] = ""; str_cpy(t, ui_translate("Fertigmachen f\xfcrs Rennen: "), 200); str_cat(t, ui_translate(TRACKS[ti].name), 200);
  if (!PF.single) { str_cat(t, ui_translate(" , Runde : "), 200); str_int(t, 200, PF.heat + 1); }
  cv_text(g_font_small, 320, 458, t, ORANGE, 1, 1);
  if (g_ld_state < 0) g_ld_state = 0;
  float rf = 1;   // part of the track video that has been read
  if (g_ld_state == 0) {
    if (G.t > 0.05) {
      if (TK.loaded && TK.idx == ti) g_ld_state = 2;
      else {
        char p[128]; str_cpy(p, TRACKS[ti].cd, 128); str_cat(p, ".UBB", 128);
        sys_free(g_preload_ubb); g_preload_ubb = 0; g_preload_track = -1;
        g_ld_state = iso_stream_begin(p, &g_ld) ? 1 : 2;
      }
    }
    rf = 0;
  } else if (g_ld_state == 1) {
    int r = iso_stream_step(&g_ld, 3u << 20);
    rf = g_ld.size ? (float)g_ld.done / g_ld.size : 0;
    if (r == 1) { g_preload_ubb = g_ld.buf; g_preload_n = g_ld.size; g_preload_track = ti; g_ld.buf = 0; g_ld_state = 2; }
    else if (r < 0) g_ld_state = 2;
  } else if (g_ld_state == 2) { if (G.t >= LOAD_MIN_TIME) g_ld_state = 3; }
  else { g_ld_state = -1; start_race(); return; }
  float tf = (float)(G.t / LOAD_MIN_TIME); if (tf > 1) tf = 1;
  float frac = rf < tf ? rf : tf;
  // information panel: car, money, weapons, upgrades
  {
    int x = 150, y = 300, w = 340, h = 96;
    cv_rect(x, y, w, h, rgba(10, 10, 12, 190));
    cv_rect(x, y, w, 2, rgba(150, 140, 120, 255)); cv_rect(x, y + h - 2, w, 2, rgba(150, 140, 120, 255)); cv_rect(x, y, 2, h, rgba(150, 140, 120, 255)); cv_rect(x + w - 2, y, 2, h, rgba(150, 140, 120, 255));
    const Loadout &lo0 = PF.lo; Loadout lo = lo0; if (cheats_cur().weapons) allweapons_loadout(&lo);
    char a[160] = ""; str_cpy(a, ui_translate("Wagen : "), 160); str_cat(a, PF.car >= 0 ? CARS[PF.car].name : "-", 160);
    str_cat(a, ui_translate("     Geld : "), 160); if (cheats_cur().money) str_cat(a, ui_translate("unbegrenzt"), 160); else { str_int(a, 160, PF.money); str_cat(a, " $", 160); }
    cv_text(g_font_small, x + 12, y + 8, a, BEIGE, 0, 1);
    int ly = y + 28;
    for (int k = 0; k < 2; k++) {
      int fw = lo.front[k], rw = lo.rear[k];
      char b[160] = "";
      if (fw) { str_cat(b, ui_translate(WEAPONS[fw].name), 160); str_cat(b, " : ", 160); if (cheats_cur().ammo) str_cat(b, ui_translate("unbegrenzt"), 160); else str_int(b, 160, lo.fammo[k]); }
      cv_text(g_font_small, x + 12, ly + k * 18, b, ORANGE, 0, 1);
      b[0] = 0;
      if (rw) { str_cat(b, rw == W_MINE ? "Minen" : "\xd6l", 160); str_cat(b, " : ", 160); if (cheats_cur().ammo) str_cat(b, ui_translate("unbegrenzt"), 160); else str_int(b, 160, lo.rammo[k]); }
      cv_text(g_font_small, x + 190, ly + k * 18, b, ORANGE, 0, 1);
    }
    char c[160] = ""; str_cpy(c, ui_translate("Upgrades : "), 160); int any = 0;
    if (lo.shield) { str_cat(c, ui_translate("Schutzschild SH-24"), 160); any = 1; }
    if (cheats_cur().nodamage) { str_cat(c, any ? ", " : "", 160); str_cat(c, ui_translate("kein Schaden"), 160); any = 1; }
    if (!any) str_cat(c, ui_translate("keine"), 160);
    if (!cheats_cur().nodamage && PF.damage > 0) { str_cat(c, ui_translate("    Schaden : "), 160); str_int(c, 160, (int)PF.damage); str_cat(c, " %", 160); }
    cv_text(g_font_small, x + 12, ly + 40, c, BEIGE, 0, 1);
  }
  draw_loading_bar(frac,"Strecke wird geladen ...");
  if (in->pressed[K_ESC] && g_ld_state >= 1 && g_ld_state <= 2) { sys_free(g_ld.buf); g_ld.buf = 0; g_ld_state = -1; if (PF.single) single_end(); go(GS_MENU); }
}

// classification after a race, laid out like the original (DATA640\CLASS.HLZ grid: name column,
// two heat columns, total; green names, orange points; the player is marked with ">")
static void screen_results(const Input *in) {
  cv_pic(G.bg, 0, 0, 1);
  const uint32_t CGREEN = rgba(40, 170, 100, 255), CORANGE = rgba(235, 165, 95, 255);
  char t[160];
  int heat = PF.single ? 0 : PF.heat;
  cv_text(g_font_big, 38, 8, "Klassifikation", CGREEN, 0, 1);
  t[0] = 0; str_cat(t, ui_translate("Runde : "), 160); str_int(t, 160, heat + 1); cv_text(g_font_big, 38, 28, t, CGREEN, 0, 1);
  int h0 = heat >= 2 ? 1 : 0;   // the two heat columns: heats 1+2, in the third heat 2+3
  for (int k = 0; k < 2; k++) { t[0] = 0; str_cat(t, ui_translate("Runde "), 160); str_int(t, 160, h0 + k + 1); cv_text(g_font_big, k ? 338 : 228, 34, t, CGREEN, 0, 1); }
  cv_text(g_font_big, 452, 34, "Insgesamt", CGREEN, 0, 1);
  int idx[8], n = RC.ncar, pts[8][3]; mem_zero(pts, sizeof pts);
  for (int i = 0; i < n; i++) idx[i] = i;
  for (int i = 0; i < n; i++) for (int h = 0; h < 3; h++) pts[i][h] = PF.single ? 0 : PF.pts[i][h];
  if (PF.single) for (int p2 = 0; p2 < n; p2++) { int ci = RC.order[p2]; pts[ci][0] = RC.car[ci].out ? 0 : POINTS[p2]; }
  auto total = [&](int i) { return pts[i][0] + pts[i][1] + pts[i][2]; };
  for (int i = 1; i < n; i++) { int k = idx[i], j = i - 1; while (j >= 0 && total(idx[j]) < total(k)) { idx[j + 1] = idx[j]; j--; } idx[j + 1] = k; }
  // the player always after the others when he is out
  for (int r = 0; r < n; r++) {
    int ci = idx[r]; RCar &c = RC.car[ci];
    int y = 74 + r * 46 + 4;
    t[0] = 0; str_int(t, 160, r + 1); cv_text(g_font_big, 8, y, t, CORANGE, 0, 1);
    t[0] = 0; if (c.player) str_cat(t, ">", 160); str_cat(t, ".", 160); str_cat(t, c.name, 160); str_cat(t, ".", 160);
    cv_text(g_font_big, 40, y, t, CGREEN, 0, 1);
    int gone = c.out && c.player;
    for (int k = 0; k < 2; k++) {
      int h = h0 + k;
      if (gone && h >= heat) { cv_text(g_font_big, k ? 404 : 288, y, "...", CORANGE, 2, 1); continue; }
      t[0] = 0; str_int(t, 160, h <= heat ? pts[ci][h] : 0); cv_text(g_font_big, k ? 412 : 300, y, t, CORANGE, 2, 1);
    }
    if (gone) { cv_text(g_font_small, 452, y + 6, G.aborted ? "Verlassen" : "Zerst\xf6rt", CGREEN, 0, 1); }
    else { t[0] = 0; str_int(t, 160, total(ci)); cv_text(g_font_big, 452, y, t, CORANGE, 0, 1); }
    if (c.player) cv_text(g_font_small, 534, y + 6, "( Spieler )", rgba(150, 230, 120, 255), 0, 1);
  }
  t[0] = 0; str_cat(t, ui_translate("Treffer: "), 160); str_int(t, 160, player().hits); str_cat(t, ui_translate("    Gewonnen: "), 160); str_int(t, 160, player().earned + (RC.car[0].out || PF.single ? 0 : PF.heat < 2 ? PF.pts[0][PF.heat] / 2 : player().place == 1 ? 6000 : 0)); str_cat(t, ui_translate(" $    Geld: "), 160); str_int(t, 160, money_shown()); str_cat(t, " $", 160);
  cv_text(g_font_small, 320, 452, t, CGREEN, 1, 1);
  if (G.t > 0.8 && (G.click || in->pressed[K_ENTER] || in->pressed[K_SPACE] || in->pressed[K_ESC])) { voice_stop(); results_continue(); }
}

static void screen_prize(const Input *in) {
  cv_pic(G.bg, 0, 0, 1);
  static int chosen = -1; static char prize[96]; static double since;
  if (G.t < 0.05) chosen = -1;
  int door = -1;
  for (int i = 0; i < 3; i++) if (hit(108 + i * 140, 105, 120, 230)) door = i;
  if (in->pressed[K_LEFT]) G.sel = (G.sel + 2) % 3; if (in->pressed[K_RIGHT]) G.sel = (G.sel + 1) % 3;
  if (chosen < 0 && in->pressed[K_ENTER]) door = G.sel;
  if (chosen < 0 && door >= 0 && (G.click || in->pressed[K_ENTER])) {
    chosen = door; since = G.t; int r = irnd(4);
    prize[0] = 0;
    if (r == 0) { int m = 8000 + irnd(5) * 2000; PF.money += m; str_cat(prize, ui_translate("Geld: "), 96); str_int(prize, 96, m); str_cat(prize, " $", 96); }
    else if (r == 1) { PF.lo.shield = 100; str_cat(prize, ui_translate("Schutzschild SH-24"), 96); }
    else if (r == 2) { PF.damage = 0; str_cat(prize, ui_translate("Kostenlose Reparatur"), 96); }
    else { PF.unlocked = PF.unlocked + 1 > NCARS ? NCARS : PF.unlocked + 1; PF.money += 5000; str_cat(prize, ui_translate("Neuer Wagen im Laden + 5000 $"), 96); }
    msfx(S_MENU3, 0.9f);
  }
  if (chosen >= 0) {
    cv_rect(0, 390, 640, 50, rgba(0, 0, 0, 160));
    cv_text(g_font_h14, 320, 400, prize, GOLD, 1, 1);
    if (G.t - since > 0.6 && (G.click || in->pressed[K_ENTER])) { since = 0; next_track(); return; }
  }
  draw_cursor();
}

static void screen_gameover(const Input *in) {
  cv_pic(G.bg, 0, 0, G.bg.w <= 320 ? 2 : 1);
  if (G.t > 0.5 && (G.click || in->pressed[K_ENTER] || in->pressed[K_ESC] || in->pressed[K_SPACE])) go(GS_HISCORE);
}

// ------------------------------------------------------------------ high scores
static void screen_hiscore(const Input *in) {
  cv_pic(G.bg, 0, 0, 1);
  for (int i = 0; i < 9; i++) {
    const HiEntry &e = G.hi[i];
    uint32_t col = i == G.hi_edit ? rgba(240, 40, 30, 255) : GOLD;   // the new entry is red until it is confirmed
    char t[96]; str_cpy(t, e.name, 96);
    if (i == G.hi_edit && ((int)(G.t * 3) & 1)) str_cat(t, "_", 96);
    cv_text_raw(g_font_h14, 15, 34 + i * 48, t, col, 0, 1);
    t[0] = 0; str_cat(t, ui_translate("Score : "), 96); str_int(t, 96, e.score); str_cat(t, ", ", 96); str_cat(t, ui_translate(TRACKS[e.track % 6].name), 96); str_cat(t, ui_translate(", Runde: "), 96); str_int(t, 96, e.heat + 1);
    cv_text(g_font_h10, 15, 58 + i * 48, t, col, 0, 1);
  }
  if (G.hi_edit >= 0) {   // typing the name (Backspace deletes, Enter confirms)
    HiEntry &e = G.hi[G.hi_edit]; int n = (int)str_len(e.name);
    for (int k = 'A'; k <= 'Z'; k++) if (in->pressed[k] && n < 17) { e.name[n] = (char)(n == 0 || e.name[n - 1] == ' ' ? k : k + 32); e.name[++n] = 0; }
    for (int k = '0'; k <= '9'; k++) if (in->pressed[k] && n < 17) { e.name[n] = (char)k; e.name[++n] = 0; }
    if (in->pressed[K_SPACE] && n < 17 && n > 0) { e.name[n] = ' '; e.name[++n] = 0; }
    if (in->pressed[K_BACKSPACE] && n > 0) e.name[--n] = 0;
    if (in->pressed[K_ENTER]) { if (!e.name[0]) str_cpy(e.name, "Player", 20); G.hi_edit = -1; hiscore_save(); G.t = 0; msfx(S_MENU3, 0.7f); }
    if (in->pressed[K_ESC]) { if (!e.name[0]) str_cpy(e.name, "Player", 20); G.hi_edit = -1; hiscore_save(); go(GS_MENU); }
    return;
  }
  if (G.t > 0.5 && (G.click || in->pressed[K_ENTER] || in->pressed[K_ESC])) { hiscore_save(); go(GS_MENU); }
  draw_cursor();
}

// ------------------------------------------------------------------ MEGARACE II SETUP
static int g_audio_drag=-1;
static void audio_volume_set(int bus,int value){
  if(value<0)value=0;if(value>100)value=100;
  if(g_audio_percent[bus]==value)return;
  g_audio_percent[bus]=value;setup_apply();
  if(sys_debug_enabled()){static const char *names[]={"Sounds","Engines","Race music","Menu music","Menu sounds"};char event[120]="[event] Volume: ";str_cat(event,names[bus],sizeof event);str_cat(event," = ",sizeof event);str_int(event,sizeof event,value);str_cat(event,"%",sizeof event);sys_log(event);}
  if(bus==AUDIO_MENU_MUSIC && value)menu_music_start();
}
static void screen_retro_options(const Input *in);
static void screen_options(const Input *in) {
  if(G.gfx_mode==1){screen_retro_options(in);return;}
  cv_pic(G.bg,0,0,1);
  // Two columns keep every option visible against the original menu artwork.
  cv_text(g_font_h14,320,6,"MEGARACE II SETUP",ORANGE,1,1);
  cv_text(g_font_h14,28,46,"Grafikmodi",ORANGE,0,1);cv_text(g_font_h14,338,46,"Ger\xe4usche",ORANGE,0,1);
  cv_text(g_font_h14,28,188,"Rennen",ORANGE,0,1);cv_text(g_font_h14,28,348,"Steuerung",ORANGE,0,1);
  cv_text(g_font_small,610,350,g_pad.connected?"(Controller verbunden)":"(kein Controller gefunden)",g_pad.connected?GREEN:GREY,2,1);
  int hover=-1;
  auto selected=[&](int i){return G.sel==i&&G.kbd?WHITE:BEIGE;};
  auto item=[&](int i,int y,const char *txt,int on,int button){
    const SprFile &sp=button?G.exit_spr:on?G.chk_on:G.chk_off;
    if(sp.n)cv_spr(spr_at(sp.f[0],28,y-2),G.bg.pal,0,0,1);
    cv_text(g_font_h10,button?80:58,y+2,txt,selected(i),0,1);
    if(hit(25,y-4,276,28))hover=i;
  };
  item(0,76,"Revived",G.gfx_mode==0,0);item(1,105,"Retro Mode",G.gfx_mode==1,0);
  item(2,142,"Advanced graphics",0,1);
  item(3,222,"Lance Boyle videos",G.opts_clips,0);
  char laps[96]="";str_cpy(laps,ui_translate("Runden"),sizeof laps);str_cat(laps,":  < ",sizeof laps);
  if(G.opts_laps)str_int(laps,sizeof laps,G.opts_laps);else str_cat(laps,"5/5/3",sizeof laps);
  str_cat(laps," >",sizeof laps);item(4,262,laps,0,1);
  cv_text(g_font_small,340,320,"Audio off hint",GREY,0,1);
  static const char *labels[]={"Sounds","Engines","Race music","Menu music","Menu sounds"};
  for(int bus=0;bus<AUDIO_COUNT;bus++){
    int y=77+bus*47,sel=5+bus,value=g_audio_percent[bus];
    cv_text(g_font_h10,340,y,labels[bus],selected(sel),0,1);
    char percent[16]="";str_int(percent,sizeof percent,value);str_cat(percent,"%",sizeof percent);cv_text(g_font_h10,610,y,percent,selected(sel),2,1);
    cv_rect(340,y+24,270,4,GREY);cv_rect(340,y+24,270*value/100,4,ORANGE);
    cv_rect(337+270*value/100,y+19,7,14,selected(sel));
    if(hit(334,y-3,282,40))hover=sel;
  }
  auto action=[&](int i,int x,int y,const char *txt){
    if(G.exit_spr.n)cv_spr(spr_at(G.exit_spr.f[0],x,y-2),G.bg.pal,0,0,1);
    cv_text(g_font_h10,x+52,y+2,txt,selected(i),0,1);
    if(hit(x-3,y-4,284,26))hover=i;
  };
  action(10,28,377,"Controller belegen");action(11,28,405,"Tasten");action(12,338,377,"Debugmen\xfc");
  action(13,426,447,"\xdc" "bernehmen");
  cv_text(g_font_small,28,448,"Volume controls hint",GREY,0,1);
  if(in->pressed[K_DOWN]){G.sel=(G.sel+1)%14;G.kbd=1;}
  if(in->pressed[K_UP]){G.sel=(G.sel+13)%14;G.kbd=1;}
  int dir=in->pressed[K_RIGHT]?1:in->pressed[K_LEFT]?-1:0;
  if(dir){G.kbd=1;if(G.sel>=5&&G.sel<=9)audio_volume_set(G.sel-5,g_audio_percent[G.sel-5]+dir*5);else if(G.sel==4){int i=G.opts_laps==3?1:G.opts_laps==5?2:0;static const int v[]={0,3,5};G.opts_laps=v[(i+dir+3)%3];}else G.sel=(G.sel+dir+14)%14;}
  if(in->mouse_dx||in->mouse_dy)G.kbd=0;
  if(!in->lmb)g_audio_drag=-1;
  if(G.click&&hover>=5&&hover<=9){G.sel=hover;g_audio_drag=hover-5;}
  if(g_audio_drag>=0 && (in->lmb||G.click))audio_volume_set(g_audio_drag,((G.mx-340)*100+135)/270);
  int act=G.click?hover:(in->pressed[K_ENTER]||in->pressed[K_SPACE])?G.sel:-1;
  if(in->pressed[K_ESC])act=13;
  if(act>=5&&act<=9){if(!G.click)audio_volume_set(act-5,g_audio_percent[act-5]>=100?0:g_audio_percent[act-5]+5);}
  else if(act>=0){
    msfx(S_MENU2,0.7f);
    if(act==13){g_audio_drag=-1;setup_save();options_close();return;}
    if(act<=1)graphics_mode_set(act);
    if(act==3)G.opts_clips^=1;
    if(act==4){G.opts_laps=G.opts_laps==0?3:G.opts_laps==3?5:0;}
    if(act==2||act==10||act==12){setup_save();G.state=act==2?GS_GFX:act==10?GS_PADMAP:GS_DEBUG;G.sel=0;G.t=0;G.pad_wait=-1;G.menu_click=-1;g_audio_drag=-1;return;}
    if(act==11)show_message("Tasten im Rennen:\nPfeil hoch: Gas     Pfeil runter: Bremse\nLinks / Rechts: lenken\nLeertaste: Frontwaffe   Tab: wechseln\nEingabe: Heckwaffe   /: wechseln\nEsc: Pause",GS_OPTIONS);
    setup_apply();
  }
  draw_cursor();
}

#include "g_retro_menu.inc"

// controller assignment: choose an action, then press the new button
static void screen_padmap(const Input *in) {
  cv_pic(G.bg, 0, 0, 1);
  cv_text(g_font_h14, 330, 2, "CONTROLLER BELEGEN", ORANGE, 1, 1);
  cv_text(g_font_small, 320, 30, g_pad.connected ? "Xbox-Controller verbunden" : "Kein Controller gefunden - bitte anschlie\xdf" "en", g_pad.connected ? GREEN : GREY, 1, 1);
  int n = PA_COUNT + 2;   // actions, "Standardbelegung", "Zurueck"
  int hover = -1;
  for (int i = 0; i < n; i++) {
    int y = 70 + i * 34; if (i >= PA_COUNT) y += 14;
    uint32_t col = i == G.sel ? WHITE : BEIGE;
    if (i < PA_COUNT) {
      cv_text(g_font_h14, 60, y, PA_NAME[i], col, 0, 1);
      const char *b = G.pad_wait == i ? (((int)(G.t * 3) & 1) ? "Taste dr\xfc" "cken ..." : "") : pad_bit_name(G.pad_map[i]);
      cv_text(g_font_h14, 360, y, b, G.pad_wait == i ? GOLD : ORANGE, 0, 1);
    } else cv_text(g_font_h14, 60, y, i == PA_COUNT ? "Standardbelegung" : "Zur\xfc" "ck", col, 0, 1);
    if (i == G.sel) cv_rect(44, y + 4, 8, 13, WHITE);
    if (hit(40, y - 4, 560, 30)) hover = i;
  }
  cv_text(g_font_small, 320, 462, "Lenken geht immer auch mit dem Steuerkreuz.  Esc: Abbrechen", GREY, 1, 1);
  if (G.pad_wait >= 0) {   // waiting for a button
    if (in->pressed[K_ESC]) { G.pad_wait = -1; return; }
    for (int b = 0; b < 26; b++) if (pad_is(g_pad_pressed, b) && b != 10 && b != 11) {
      for (int a = 0; a < PA_COUNT; a++) if (a != G.pad_wait && G.pad_map[a] == b) G.pad_map[a] = G.pad_map[G.pad_wait];   // swap with the action that had it
      G.pad_map[G.pad_wait] = b; G.pad_wait = -1; msfx(S_MENU3, 0.7f); setup_save(); break;
    }
    draw_cursor(); return;
  }
  if (in->pressed[K_DOWN]) G.sel = (G.sel + 1) % n; if (in->pressed[K_UP]) G.sel = (G.sel + n - 1) % n;
  if (hover >= 0 && (in->mouse_dx || in->mouse_dy)) G.sel = hover;
  int act = (G.click && hover >= 0) ? hover : in->pressed[K_ENTER] ? G.sel : -1;
  if (in->pressed[K_ESC]) act = n - 1;
  if (act >= 0) {
    msfx(S_MENU2, 0.7f);
    if (act < PA_COUNT) { G.pad_wait = act; G.t = 0; }
    else if (act == PA_COUNT) { for (int a = 0; a < PA_COUNT; a++) G.pad_map[a] = PA_DEFAULT[a]; setup_save(); }
    else { setup_save(); go(GS_OPTIONS); G.sel = G.gfx_mode==1?9:10; }
  }
  draw_cursor();
}

// ------------------------------------------------------------------ debug menu: technical options and championship cheats
// a list of check boxes; returns the item that was activated (or -1)
struct CheckItem { const char *txt; int *val; };
static int check_list(const Input *in, const CheckItem *it, int n, int x, int y0, int dy, int col2_from, int x2) {
  int hover = -1;
  for (int i = 0; i < n; i++) {
    int xx = i >= col2_from ? x2 : x, yy = y0 + (i >= col2_from ? i - col2_from : i) * dy;
    if (it[i].val) { const SprFile &sp = *it[i].val ? G.chk_on : G.chk_off; if (sp.n) cv_spr(spr_at(sp.f[0], xx - 3, yy - 2), G.bg.pal, 0, 0, 1); }
    else if (G.chk_off.n) cv_spr(spr_at(G.chk_off.f[0], xx - 3, yy - 2), G.bg.pal, 0, 0, 1);
    cv_text(g_font_h14, xx + 28, yy, it[i].txt, i == G.sel && G.kbd ? WHITE : BEIGE, 0, 1);
    if (hit(xx - 3, yy - 4, 30 + cv_text_w(g_font_h14, it[i].txt, 1), dy - 2)) hover = i;
  }
  if (in->pressed[K_DOWN]) { G.sel = (G.sel + 1) % n; G.kbd = 1; } if (in->pressed[K_UP]) { G.sel = (G.sel + n - 1) % n; G.kbd = 1; }
  if (in->mouse_dx || in->mouse_dy) G.kbd = 0;
  int act = (G.click && hover >= 0) ? hover : (in->pressed[K_ENTER] || in->pressed[K_SPACE]) ? G.sel : -1;
  if (act >= 0) { msfx(S_MENU2, 0.7f); if (it[act].val) *it[act].val ^= 1; }
  return act;
}
// graphics settings
static void screen_gfx(const Input *in) {
  if(G.gfx_mode==1){screen_retro_gfx(in);return;}
  graphics_options_apply();
  cv_pic(G.bg, 0, 0, 1);
  cv_text(g_font_h14, 330, 6, "ERWEITERTE GRAFIK", ORANGE, 1, 1);
  static const int MS[4] = {0, 2, 4, 8};
  int msi = G.gfx_msaa >= 8 ? 3 : G.gfx_msaa >= 4 ? 2 : G.gfx_msaa >= 2 ? 1 : 0;
  char sc[96] = ""; str_cpy(sc, ui_translate("Skalierung (Retro Mode): "), 96); str_int(sc, 96, G.gfx_scale); str_cat(sc, "x", 96);
  char ms[40] = ""; str_cpy(ms, ui_translate("MSAA Kantengl\xe4ttung: "), 40); if (msi) { str_int(ms, 40, MS[msi]); str_cat(ms, "x", 40); } else str_cat(ms, ui_translate("Aus"), 40);
  static const char *FPSM[2] = {"Original", "60fps"};
  char fo[64] = ""; str_cpy(fo, ui_translate("Bildausgabe im Rennen: "), sizeof fo); str_cat(fo, ui_translate(FPSM[G.dbg_fpsmode % 2]), sizeof fo);
  char vf[96]="";str_cpy(vf,ui_translate("Video filter: "),sizeof vf);str_cat(vf,VIDEO_FILTER_NAMES[G.video_filter%4],sizeof vf);
  struct Row { const char *txt; int kind; int *val; } rows[] = {
    {"Revived (hochaufl\xf6send)", 1, 0}, {"Retro Mode", 2, 0},
    {"Texturen gefiltert (nur Revived)", 4, 0}, {ms, 5, 0}, {"FXAA Kantengl\xe4ttung", 6, &G.gfx_fxaa}, {"Vollbild", 7, &G.gfx_full}, {fo, 10, 0}, {"Enable custom textures",11,&G.custom_textures}, {"Video upscaling",13,&G.video_upscale}, {vf,14,0}, {"Revived Effects",12,0}, {"\xdc" "bernehmen", 8, 0}};
  int n = (int)(sizeof rows / sizeof rows[0]);
  if(G.sel<0||G.sel>=n)G.sel=0;
  int hover = -1;
  for (int i = 0; i < n; i++) {
    int locked=graphics_option_locked(rows[i].kind);
    int x = 40, y = 46 + i * 30; if (i == n - 1) { x = 443; y = 443; }
    int on = rows[i].kind == 1 ? G.gfx_mode == 0 : rows[i].kind == 2 ? G.gfx_mode == 1 : rows[i].kind == 4 ? G.opts_gfx == 0 : rows[i].kind == 5 ? msi > 0 : rows[i].val ? *rows[i].val : 0;
    const SprFile &sp = (rows[i].kind == 8 || rows[i].kind == 12 || rows[i].kind == 14) ? G.exit_spr : (rows[i].kind == 3 || rows[i].kind == 10 ? G.chk_off : on ? G.chk_on : G.chk_off);
    u8 dimmed[768];if(locked)for(int c=0;c<256;c++){u8 v=(u8)((G.bg.pal[c*3]+G.bg.pal[c*3+1]+G.bg.pal[c*3+2])/5);dimmed[c*3]=dimmed[c*3+1]=dimmed[c*3+2]=v;}
    if (sp.n) cv_spr(spr_at(sp.f[0], x - 3, y - 2), locked?dimmed:G.bg.pal, 0, 0, 1);
    int tx = rows[i].kind == 8 ? 496 : (rows[i].kind == 12 || rows[i].kind == 14) ? x + 53 : x + 28;
    cv_text(g_font_h14, tx, y, rows[i].txt, locked ? GREY : i == G.sel && G.kbd ? WHITE : BEIGE, 0, 1);
    if (!locked && hit(x - 3, y - 4, tx - x + 3 + cv_text_w(g_font_h14, rows[i].txt, 1), 28)) hover = i;
  }
  cv_text(g_font_small,40,372,"Revived graphics hint",GREY,0,1);
  if (in->pressed[K_DOWN]) {do{G.sel=(G.sel+1)%n;}while(graphics_option_locked(rows[G.sel].kind));G.kbd=1;}
  if (in->pressed[K_UP]) {do{G.sel=(G.sel+n-1)%n;}while(graphics_option_locked(rows[G.sel].kind));G.kbd=1;}
  if (in->mouse_dx || in->mouse_dy) G.kbd = 0;
  int act = (G.click && hover >= 0) ? hover : (in->pressed[K_ENTER] || in->pressed[K_SPACE]) ? G.sel : -1;
  int dir = in->pressed[K_RIGHT] ? 1 : in->pressed[K_LEFT] ? -1 : 0;
  if (dir && (rows[G.sel].kind == 3 || rows[G.sel].kind == 5 || rows[G.sel].kind == 10 || rows[G.sel].kind==14)) act = G.sel; else if (!dir) dir = 1;
  if (in->pressed[K_ESC]) act = n - 1;
  if(act>=0 && graphics_option_locked(rows[act].kind))act=-1;
  if (act >= 0) {
    msfx(S_MENU2, 0.7f);
    switch (rows[act].kind) {
      case 1: graphics_mode_set(0); break; case 2: graphics_mode_set(1); break;
      case 3: G.gfx_scale = (G.gfx_scale - 1 + 4 + dir) % 4 + 1; break;
      case 4: G.opts_gfx = G.opts_gfx == 0 ? 1 : 0; break;
      case 5: msi = (msi + 4 + dir) % 4; G.gfx_msaa = MS[msi]; break;
      case 6: G.gfx_fxaa ^= 1; break; case 7: G.gfx_full ^= 1; break; case 9: G.gfx_intscale ^= 1; break;
      case 12: setup_save();G.state=GS_REVIVE_EFFECTS;G.sel=0;return;
      case 11: G.custom_textures^=1;texture_options_apply();break;
      case 13: G.video_upscale^=1;texture_options_apply();break;
      case 14: G.video_filter=(G.video_filter+4+dir)%4;texture_options_apply();break;
      case 10: G.dbg_fpsmode = (G.dbg_fpsmode + 2 + dir) % 2; debug_apply(); break;
      case 8: setup_save(); go(GS_OPTIONS); G.sel = 2; return;
    }
    setup_save();
  }
  draw_cursor();
}

static int g_effect_drag=-1;
static int *effect_switches(int index){int *values[]={&G.real_time_shadows,&G.vehicle_lights,&G.enhanced_oil,&G.modern_explosions,&G.damage_effects,&G.missile_effects};return values[index];}
static void effect_intensity_set(int index,int value){
  value=value<0?0:value>200?200:value;if(index<0||index>=6||G.effect_intensity[index]==value)return;G.effect_intensity[index]=value;debug_apply();
  if(sys_debug_enabled()){const char *names[]={"Shadows","Vehicle lights","Oil slicks","Explosions","Damage effects","Missile exhaust"};char message[120]="[event] Revived effect intensity: ";str_cat(message,names[index],sizeof message);str_cat(message," = ",sizeof message);str_int(message,sizeof message,value);str_cat(message,"%",sizeof message);sys_log(message);}
}
static void effect_defaults(){for(int i=0;i<6;i++){G.effect_intensity[i]=100;*effect_switches(i)=1;}debug_apply();setup_save();if(sys_debug_enabled())sys_log("[event] Revived effects reset to enabled, with their original 100% intensities.");}
static void screen_revive_effects(const Input *in){
  if(G.gfx_mode!=0){g_effect_drag=-1;go(GS_OPTIONS);return;}
  cv_pic(G.bg,0,0,1);cv_text(g_font_h14,320,6,"Revived Effects",ORANGE,1,1);
  const char *labels[]={"Real-time shadows","Vehicle lights","Enhanced oil slicks","Modern explosion effects","Damage effects","Missile effects"};
  if(G.sel<0||G.sel>7)G.sel=0;int hover=-1,slider=-1;
  for(int i=0;i<6;i++){int y=64+i*43;const SprFile &sp=*effect_switches(i)?G.chk_on:G.chk_off;uint32_t color=G.sel==i?WHITE:BEIGE;if(sp.n)cv_spr(spr_at(sp.f[0],34,y-2),G.bg.pal,0,0,1);cv_text(g_font_h14,66,y,labels[i],color,0,1);
    int value=G.effect_intensity[i];cv_rect(365,y+12,180,4,GREY);cv_rect(365,y+12,180*value/200,4,ORANGE);cv_rect(362+180*value/200,y+7,7,14,color);
    char text[16]="";str_int(text,sizeof text,value);str_cat(text,"%",sizeof text);cv_text(g_font_h10,626,y+2,text,color,2,1);
    if(hit(31,y-4,600,30))hover=i;if(hit(355,y-4,198,30))slider=i;
  }
  cv_text(g_font_small,34,330,"Revived effects hint",GREY,0,1);cv_text(g_font_small,34,373,"Effect intensity hint",GREY,0,1);
  if(G.exit_spr.n){cv_spr(spr_at(G.exit_spr.f[0],34,441),G.bg.pal,0,0,1);cv_spr(spr_at(G.exit_spr.f[0],443,441),G.bg.pal,0,0,1);}
  cv_text(g_font_h14,87,443,"Reset effects",G.sel==6?WHITE:BEIGE,0,1);if(hit(31,436,275,35))hover=6;
  cv_text(g_font_h14,496,443,"\xdc" "bernehmen",G.sel==7?WHITE:BEIGE,0,1);if(hit(440,436,195,35))hover=7;
  if(in->pressed[K_DOWN])G.sel=(G.sel+1)%8;if(in->pressed[K_UP])G.sel=(G.sel+7)%8;if(hover>=0&&(in->mouse_dx||in->mouse_dy))G.sel=hover;
  if(!in->lmb&&g_effect_drag>=0){setup_save();g_effect_drag=-1;}
  if(G.click&&slider>=0){G.sel=slider;g_effect_drag=slider;}
  if(g_effect_drag>=0&&(in->lmb||G.click))effect_intensity_set(g_effect_drag,((G.mx-365)*200+90)/180);
  if(G.sel<6){int dir=in->pressed[K_RIGHT]?1:in->pressed[K_LEFT]?-1:0;if(dir){effect_intensity_set(G.sel,G.effect_intensity[G.sel]+dir*5);setup_save();}}
  int action=G.click&&hover>=0&&slider<0?hover:in->pressed[K_ENTER]||in->pressed[K_SPACE]?G.sel:-1;if(in->pressed[K_ESC])action=7;
  if(action>=0){msfx(S_MENU2,.7f);if(action<6){*effect_switches(action)^=1;debug_apply();setup_save();}else if(action==6){g_effect_drag=-1;effect_defaults();}else{g_effect_drag=-1;setup_save();G.state=GS_GFX;G.sel=10;return;}}
  draw_cursor();
}

static void screen_debug(const Input *in) {
  if(G.gfx_mode==1){screen_retro_debug(in);return;}
  cv_pic(G.bg, 0, 0, 1);
  cv_text(g_font_h14, 330, 6, "DEBUGMEN\xdc", ORANGE, 1, 1);
  cv_text(g_font_h14, 17, 40, "Erweiterte Debug Optionen", ORANGE, 0, 1);
  cv_text(g_font_h14, 17, 314, "Cheats (nur Meisterschaft)", ORANGE, 0, 1);
  Cheats &c = G.cheat_career;
  const CheckItem it[] = {
    {"FPS anzeigen", &G.dbg_fps}, {"Renn-Infos (Videobild, Streckenpunkt, Tempo)", &G.dbg_info}, {"Fahrbahn-Markierungen (H\xfctchen)", &G.dbg_road}, {"Ohne Gegner fahren", &G.dbg_noopp},
    {"Bild 4:3 gestreckt (statt Original 640x400)", &G.dbg_stretch}, {"Alte gegl\xe4ttete Kamera (nicht original)", &G.dbg_camrot}, {"Alle Videos \xfc" "berspringen (auch Intro)", &G.dbg_noclips},
    {"KI-Notfallhilfe (modern, nicht original)", &G.ai_rescue},
    {"Revived-Fahrmodell (unterteilte Physik)", &G.sim_model},
#ifdef _WIN32
    {"Debug-Konsole (CMD)", &G.dbg_console},
#else
    {"Debug-Ausgabe (Terminal)", &G.dbg_console},
#endif
    {"Dump textures", 0},
    {"Unbegrenzt Geld", &c.money}, {"Unbegrenzt Munition", &c.ammo}, {"Alle Waffen und Wagen", &c.weapons}, {"Kein Schaden", &c.nodamage},
    {"Zur\xfc" "cksetzen", 0}, {"\xdc" "bernehmen", 0}};
  int n = (int)(sizeof it / sizeof it[0]);
  if(G.sel<0||G.sel>n)G.sel=0;
  // layout: technical options in two columns, cheats below, buttons at the bottom
  int hover = -1;
  struct P { int x, y; } pos[17] = {{40, 66}, {40, 88}, {40, 110}, {40, 132}, {40, 154}, {40, 176}, {40, 198}, {40, 220}, {40, 242}, {40, 264}, {40, 286}, {40, 342}, {40, 370}, {320, 342}, {320, 370}, {40, 443}, {443, 443}};
  for (int i = 0; i < n; i++) {
    int xx = pos[i].x, yy = pos[i].y;
    if (it[i].val) { const SprFile &sp = *it[i].val ? G.chk_on : G.chk_off; if (sp.n) cv_spr(spr_at(sp.f[0], xx - 3, yy - 2), G.bg.pal, 0, 0, 1); }
    else if (i == 10 && G.exit_spr.n) cv_spr(spr_at(G.exit_spr.f[0],xx-3,yy-2),G.bg.pal,0,0,1);
    else if (i == n - 1 && G.exit_spr.n) cv_spr(spr_at(G.exit_spr.f[0], xx, yy - 2), G.bg.pal, 0, 0, 1);
    else if (G.chk_off.n) cv_spr(spr_at(G.chk_off.f[0], xx - 3, yy - 2), G.bg.pal, 0, 0, 1);
    int tx = i == n - 1 ? 496 : xx + (i==10?60:28);
    cv_text(g_font_h14, tx, yy, it[i].txt, i==10 && g_dump_active?GREY:i == G.sel && G.kbd ? WHITE : BEIGE, 0, 1);
    if (hit(xx - 3, yy - 4, tx - xx + 3 + cv_text_w(g_font_h14, it[i].txt, 1), 26)) hover = i;
  }
  cv_text(g_font_small, 40, 400, "Die Cheats f\xfcr das Einzelrennen stellst du im Einzelrennen-Men\xfc ein.", GREY, 0, 1);
  if (in->pressed[K_DOWN] || in->pressed[K_RIGHT]) { G.sel = (G.sel + 1) % (n+1); G.kbd = 1; } if (in->pressed[K_UP] || in->pressed[K_LEFT]) { G.sel = (G.sel + n) % (n+1); G.kbd = 1; }
  if (in->mouse_dx || in->mouse_dy) G.kbd = 0;
  int act = (G.click && hover >= 0) ? hover : (in->pressed[K_ENTER] || in->pressed[K_SPACE]) ? G.sel : -1;
  if (in->pressed[K_ESC]) act = n - 1;
  if(G.exit_spr.n)cv_spr(spr_at(G.exit_spr.f[0],440,284),G.bg.pal,0,0,1);cv_text(g_font_h14,493,286,"Extras",G.sel==n?WHITE:BEIGE,0,1);
  if(hit(440,286,185,27)&&G.click){go(GS_EXTRAS);return;}
  if(G.sel==n){if(in->pressed[K_ENTER]||in->pressed[K_SPACE]){go(GS_EXTRAS);return;}}
  if(act==10){texture_dump_start();act=-1;}
  if (act >= 0 && act<n) {
    msfx(S_MENU2, 0.7f);
    if (it[act].val) *it[act].val ^= 1;
    else if (act == n - 2) { G.dbg_console = G.dbg_fps = G.dbg_info = G.dbg_road = G.dbg_noopp = G.dbg_stretch = G.dbg_camrot = G.dbg_noclips = 0; G.ai_rescue = 1; G.sim_model = 0; mem_zero(&G.cheat_career, sizeof G.cheat_career); }
    debug_apply(); setup_save();
    if (act == n - 1) { go(GS_OPTIONS); G.sel = 12; return; }
  }

  draw_cursor();
}

// ------------------------------------------------------------------ load / save (20 slots)
static void profile_text(char *t, const Profile &p) {
  t[0] = 0; str_cat(t, ui_translate(TRACKS[p.track % 6].name), 256); str_cat(t, ui_translate(" , Runde : "), 256); str_int(t, 256, p.heat + 1);
  if (p.car >= 0) { str_cat(t, ui_translate("\nWagen : "), 256); str_cat(t, ui_translate(CARS[p.car].name), 256); }
  str_cat(t, ui_translate("\nGeld  : "), 256); str_int(t, 256, p.money); str_cat(t, " $", 256);
}
static void list_cursor(int x, int y) { cv_rect(x, y, 8, 13, WHITE); }
static void screen_saveload(const Input *in) {
  cv_pic(G.bg, 0, 0, 1);
  char t[256];
  Profile info[RP_SLOTS]; int has[RP_SLOTS];
  for (int i = 0; i < RP_SLOTS; i++) has[i] = load_from(i, &info[i]);
  int hover = -1;
  for (int i = 0; i < RP_SLOTS; i++) {
    int y = 43 + i * 20;
    if (hit(136, y - 2, 190, 20)) hover = i;
    if (has[i]) { t[0] = 0; str_cat(t, ui_translate(TRACKS[info[i].track % 6].name), 256); str_cat(t, " ", 256); str_int(t, 256, info[i].heat + 1); }
    else str_cpy(t, ui_translate("Nicht speichern"), 256);
    cv_text(g_font_mid, 141, y, t, i == G.slot ? WHITE : ORANGE, 0, 1);
    if (i == G.slot) list_cursor(141 + cv_text_w(g_font_mid, t, 1) + 2, y);
  }
  cv_text(g_font_h14, 367, 8, "Rennen laden", WHITE, 0, 1);
  if (has[G.slot]) profile_text(t, info[G.slot]); else str_cpy(t, ui_translate("Nicht speichern"), 256);
  cv_text(g_font_small, 403, 87, t, GOLD, 0, 1);
  cv_text(g_font_h14, 367, 278, "Dein Rennen", WHITE, 0, 1);
  if (PF.active && PF.car >= 0) profile_text(t, PF); else str_cpy(t, ui_translate("Nicht speichern"), 256);
  cv_text(g_font_small, 403, 354, t, GOLD, 0, 1);
  // buttons: 0 back, 1 load (disk <-), 2 save (disk ->)
  int b = -1; for (int i = 0; i < 3 && i < G.save_spr.n; i++) if (spr_hit(G.save_spr.f[i])) b = i;
  if (b >= 0 && in->lmb) cv_spr(G.save_spr.f[b], G.bg.pal, 0, 0, 1);
  if (G.click && hover >= 0) { G.slot = hover; msfx(S_MENU2, 0.5f); }
  if (in->pressed[K_DOWN]) G.slot = (G.slot + 1) % RP_SLOTS; if (in->pressed[K_UP]) G.slot = (G.slot + RP_SLOTS - 1) % RP_SLOTS;
  int act = G.click ? b : -1;
  if (in->pressed[K_ESC]) act = 0; if (in->pressed[K_L] || in->pressed[K_ENTER]) act = 1; if (in->pressed[K_S]) act = 2;
  if (act == 0) { go(GS_MENU); return; }
  if (act == 1) { if (has[G.slot]) { PF = info[G.slot]; G.loaded = 1; save_game(); msfx(S_MENU3, 0.8f); show_message("Rennen geladen", GS_MENU); } else show_message("Keine Daten auffindbar", GS_SAVELOAD); }
  if (act == 2) { if (!PF.active || PF.car < 0) show_message("Keine Daten auffindbar", GS_SAVELOAD); else { save_to(G.slot, PF); msfx(S_MENU3, 0.8f); } }
  draw_cursor();
}

// ------------------------------------------------------------------ replays (20 slots)
static Race g_rc_backup; static TrackObj g_obj_backup[MAXOBJ];
static void replay_start(int ret) {
  G.rp_return = ret; RPL.t = 0; RPL.paused = 0;
  if (ret == GS_RACE) { mem_cpy(&g_rc_backup, &RC, sizeof RC); mem_cpy(g_obj_backup, TK.obj, sizeof g_obj_backup); race_end_sounds(); }
  else {
    if (!track_load(RPL.h.track, RPL.h.heat)) { show_message("Strecke konnte nicht geladen werden.", GS_REPLAYS); return; }
    mem_zero(&RC, sizeof RC); RC.ncar = RPL.h.ncar; RC.laps = RPL.h.laps; RC.gain = 1;
    for (int i = 0; i < RC.ncar; i++) { RC.car[i].car = RPL.h.cars[i]; RC.car[i].player = i == 0; str_cpy(RC.car[i].name, CARS[RPL.h.cars[i] % NCARS].name, 24); }
    menu_music_stop(); race_music_start();
  }
  G.state = GS_REPLAY; G.t = 0;
}
static void replay_end(void) {
  if (G.rp_return == GS_RACE) {
    mem_cpy(&RC, &g_rc_backup, sizeof RC); mem_cpy(TK.obj, g_obj_backup, sizeof g_obj_backup);
    RC.eng_voice = car_engine_play(player().car,0,mexe_engine_frequency((int)(player().v/V_RAW))/22050.f); RC.eng2_voice = -1;
    G.state = GS_RACE; RC.paused = 1;
  } else { music_stop(); go(GS_REPLAYS); }
}
static void t_hud_replay(void) {
  char t[96] = ""; str_cpy(t, ui_translate("Position "), 96); str_int(t, 96, RC.car[0].place); str_cat(t, "   ", 96); str_int(t, 96, (int)RPL.t / 60); str_cat(t, ":", 96); if ((int)RPL.t % 60 < 10) str_cat(t, "0", 96); str_int(t, 96, (int)RPL.t % 60);
  cv_text(g_font_h10, 320, 60, t, WHITE, 1, 1);
  cv_text(g_font_small, 320, 458, "Esc: Ende   Leertaste: Pause   Links/Rechts: spulen", GREY, 1, 1);
  if (RPL.paused) cv_text(g_font_big, 320, 200, "PAUSE", WHITE, 1, 1);
}
static void screen_replays(const Input *in) {
  cv_pic(G.bg, 0, 0, 1);
  char t[256];
  RpHeader h[RP_SLOTS]; int has[RP_SLOTS];
  for (int i = 0; i < RP_SLOTS; i++) has[i] = replay_slot_info(i, &h[i]);
  int hover = -1;
  for (int i = 0; i < RP_SLOTS; i++) {
    int y = 43 + i * 20;
    if (hit(136, y - 2, 190, 20)) hover = i;
    t[0] = 0; str_cat(t, ui_translate("Wiedergabe Nr.: "), 256); str_int(t, 256, i + 1);
    cv_text(g_font_mid, 141, y, t, i == G.slot ? GOLD : WHITE, 0, 1);
    if (i == G.slot) list_cursor(141 + cv_text_w(g_font_mid, t, 1) + 2, y);
  }
  cv_text(g_font_h14, 367, 8, "Wiedergabe laden", WHITE, 0, 1);
  if (has[G.slot]) {
    t[0] = 0; str_cat(t, ui_translate(TRACKS[h[G.slot].track % 6].name), 256); str_cat(t, ui_translate(" , Runde : "), 256); str_int(t, 256, h[G.slot].heat + 1);
    str_cat(t, ui_translate("\nWagen : "), 256); str_cat(t, CARS[h[G.slot].cars[0] % NCARS].name, 256); str_cat(t, ui_translate("\nPlatz : "), 256); str_int(t, 256, h[G.slot].place);
    str_cat(t, ui_translate("\nDauer : "), 256); { int sec = h[G.slot].magic[6] == '2' ? h[G.slot].size / 1000 : h[G.slot].nframes / 25; str_int(t, 256, sec / 60); str_cat(t, ui_translate(" Min. "), 256); str_int(t, 256, sec % 60); } str_cat(t, ui_translate(" Sek."), 256);
  } else str_cpy(t, ui_translate("Wiedergabe leer"), 256);
  cv_text(g_font_small, 403, 87, t, GOLD, 0, 1);
  // buttons: 0 back, 1 play, 2 delete
  int b = -1; for (int i = 0; i < 3 && i < G.repl_spr.n; i++) if (spr_hit(G.repl_spr.f[i])) b = i;
  if (b >= 0 && in->lmb) cv_spr(G.repl_spr.f[b], G.bg.pal, 0, 0, 1);
  if (G.click && hover >= 0) { G.slot = hover; msfx(S_MENU2, 0.5f); }
  if (in->pressed[K_DOWN]) G.slot = (G.slot + 1) % RP_SLOTS; if (in->pressed[K_UP]) G.slot = (G.slot + RP_SLOTS - 1) % RP_SLOTS;
  int act = G.click ? b : -1;
  if (in->pressed[K_ESC]) act = 0; if (in->pressed[K_ENTER]) act = 1; if (in->pressed[K_D]) act = 2;
  if (act == 0) { go(GS_MENU); return; }
  if (act == 1) { if (has[G.slot] && replay_open_slot(G.slot)) { replay_start(GS_REPLAYS); return; } else show_message("Wiedergabe leer", GS_REPLAYS); }
  if (act == 2 && has[G.slot]) { char p[512]; replay_path(p, G.slot); sys_write_file(p, "", 0); msfx(S_MENU2, 0.6f); }
  draw_cursor();
}

// black flag of the main menu: "Auswahl markieren" - new game / quit / delete (as in the original) + single race
static void screen_quitdlg(const Input *in) {
  cv_pic(G.bg, 0, 0, G.bg.w <= 320 ? 2 : 1);
  for (int i = 0; i < 8; i++) if (G.menu_spr[i].n) cv_spr(G.menu_spr[i].f[0], G.bg.pal, 0, 0, 1);
  if (PF.active) { char t[160] = ""; str_cpy(t, ui_translate("Fertigmachen f\xfcrs Rennen: "), 160); str_cat(t, ui_translate(TRACKS[PF.track].name), 160); str_cat(t, ui_translate(" , Runde : "), 160); str_int(t, 160, PF.heat + 1);
    str_cat(t, ui_translate(" , Position : "), 160); str_int(t, 160, PF.heat > 0 ? PF.last_place : 0); cv_text(g_font_small, 320, 458, t, ORANGE, 1, 1); }
  int count=sys_game_is_gog()?5:4;if(G.sel>=count)G.sel=0;
  int x0 = 165, y0 = count==5?158:172, w = 310, h = count==5?176:150;
  cv_rect(x0, y0, w, h, rgba(8, 26, 18, 225));
  cv_rect(x0, y0, w, 2, rgba(40, 150, 80, 255)); cv_rect(x0, y0 + h - 2, w, 2, rgba(40, 150, 80, 255));
  cv_rect(x0, y0, 2, h, rgba(40, 150, 80, 255)); cv_rect(x0 + w - 2, y0, 2, h, rgba(40, 150, 80, 255));
  cv_text(g_font_h14, 320, y0 + 8, "Auswahl markieren", WHITE, 1, 1);
  const char *it[5] = {"Neues Spiel starten", "Spiel beenden", "Spielstand zur\xfc" "cksetzen", "Einzelrennen", "Run DOS Version"};
  int hover = -1; for (int i = 0; i < count; i++) if (hit(x0, y0 + 40 + i * 26, w, 24)) hover = i;
  if (in->pressed[K_DOWN]) { G.sel = (G.sel + 1) % count; G.kbd = 1; } if (in->pressed[K_UP]) { G.sel = (G.sel + count - 1) % count; G.kbd = 1; }
  if (hover >= 0 && (in->mouse_dx || in->mouse_dy)) G.sel = hover;
  for (int i = 0; i < count; i++) cv_text(g_font_h14, 320, y0 + 42 + i * 26, it[i], i == G.sel ? GOLD : ORANGE, 1, 1);
  int act = (G.click && hover >= 0) ? hover : in->pressed[K_ENTER] ? G.sel : -1;
  if (in->pressed[K_ESC]) { go(GS_MENU); return; }
  if (act == 0) { msfx(S_MENU1, 0.8f); profile_new(); G.loaded = 0; save_game(); go(GS_CARSHOP); }
  if (act == 1) credits_play_then(GS_QUIT);
  if (act == 2) {
    char p[512]; cfg_path(p, "openmr2_save.dat"); sys_write_file(p, "", 0);
    mem_zero(&PF, sizeof PF); PF.car = -1; G.loaded = 0;
    show_message("Spielstand gel\xf6scht", GS_MENU);
  }
  if (act == 3) go(GS_TRACKSELECT);
  if(act==4&&count==5){char error[600]={};if(sys_run_dos(g_gamedir,error,sizeof error)){sys_log("[event] Original GOG DOS version launched; closing OpenMR2.");setup_save();race_end_sounds();snd_stop_all_sfx();music_stop();voice_stop();sys_audio_flush();G.state=GS_QUIT;}else{sys_log(error);show_message("DOS launch failed",GS_MENU);}}
  draw_cursor();
}

// single race: car, track, laps and own cheats (all kept apart from the championship)
static void check_box(int x, int y, int on) {   // simple check box for screens without the setup palette
  uint32_t fr = rgba(150, 140, 120, 255);
  cv_rect(x, y, 20, 20, rgba(20, 18, 16, 220)); cv_rect(x, y, 20, 2, fr); cv_rect(x, y + 18, 20, 2, fr); cv_rect(x, y, 2, 20, fr); cv_rect(x + 18, y, 2, 20, fr);
  if (on) for (int k = 0; k < 10; k++) { cv_rect(x + 4 + k, y + 5 + (k < 4 ? k : 7 - (k - 4) * 3 / 2 > 0 ? 7 - (k - 4) : 1), 3, 3, rgba(220, 40, 30, 255)); }
}
static void single_start(void) {
  G.career_pf = PF; G.in_single = 1;
  int car = G.single_car;
  mem_zero(&PF, sizeof PF); PF.single = 1; PF.car = car; PF.nfield = 8; PF.field[0] = car; PF.money = 0;
  default_loadout(&PF.lo);
  int pool[NCARS], n = 0; for (int i = 0; i < NCARS; i++) if (i != car) pool[n++] = i;
  for (int i = n - 1; i > 0; i--) { int j = irnd(i + 1); int x = pool[i]; pool[i] = pool[j]; pool[j] = x; }
  for (int i = 1; i < 8; i++) PF.field[i] = pool[i - 1];
  single_save();
  go(GS_TRACKINFO);
}
static void screen_trackselect(int W, int H, const Input *in, double dt) {
  cv_pic(G.bg, 0, 0, 1);
  G.turn=turntable_advance(G.turn,dt);
  title_box(320, 6, "Einzelrennen");
  Cheats &c = G.cheat_single;
  enum { R_CAR, R_TRACK, R_LAPS, R_C1, R_C2, R_C3, R_C4, R_BACK, R_START, R_N };
  int hover = -1;
  // car (left) and track (right)
  int car = G.single_car, t = G.single_track;
  cv_text(g_font_h14, 170, 44, "Wagen", ORANGE, 1, 1);
  cv_text(g_font_h14, 170, 222, CARS[car].name, G.sel == R_CAR ? WHITE : BEIGE, 1, 1);
  cv_text(g_font_h14, 40, 222, "<", ORANGE, 1, 1); cv_text(g_font_h14, 300, 222, ">", ORANGE, 1, 1);
  char tx[96] = ""; str_int(tx, 96, CARS[car].mph); str_cat(tx, " Mph  -  ", 96); str_cat(tx, CARS[car].engine, 96); cv_text(g_font_small, 170, 246, tx, GREY, 1, 1);
  if (hit(20, 40, 300, 225)) hover = R_CAR;
  cv_text(g_font_h14, 480, 44, "Strecke", ORANGE, 1, 1);
  const SprFile &sp = G.track_spr[TRACK_SPRITE[t]];
  if (sp.n) { const Sprite &f = sp.f[0]; cv_spr(f, G.bg.pal, 480 - (f.x + f.w / 2), 135 - (f.y + f.h / 2), 1); }
  cv_text(g_font_h14, 480, 222, TRACKS[t].name, G.sel == R_TRACK ? WHITE : BEIGE, 1, 1);
  cv_text(g_font_h14, 350, 222, "<", ORANGE, 1, 1); cv_text(g_font_h14, 610, 222, ">", ORANGE, 1, 1);
  if (hit(330, 40, 300, 225)) hover = R_TRACK;
  // laps
  char lp[32] = ""; str_cpy(lp, ui_translate("Runden : "), 32); str_int(lp, 32, G.single_laps);
  cv_text(g_font_h14, 320, 272, lp, G.sel == R_LAPS ? WHITE : BEIGE, 1, 1);
  if (hit(220, 268, 200, 24)) hover = R_LAPS;
  // cheats
  cv_text(g_font_h14, 40, 304, "Cheats (nur Einzelrennen)", ORANGE, 0, 1);
  struct CI { const char *txt; int *val; int x, y; } ci[4] = {{"Unbegrenzt Geld", &c.money, 60, 334}, {"Unbegrenzt Munition", &c.ammo, 60, 364},
                                                            {"Alle Waffen", &c.weapons, 350, 334}, {"Kein Schaden", &c.nodamage, 350, 364}};
  for (int i = 0; i < 4; i++) {
    check_box(ci[i].x - 2, ci[i].y - 1, *ci[i].val);
    cv_text(g_font_h14, ci[i].x + 28, ci[i].y, ci[i].txt, G.sel == R_C1 + i ? WHITE : BEIGE, 0, 1);
    if (hit(ci[i].x - 3, ci[i].y - 4, 30 + cv_text_w(g_font_h14, ci[i].txt, 1), 26)) hover = R_C1 + i;
  }
  // buttons
  cv_text(g_font_h14, 40, 443, "<< Zur\xfc" "ck", G.sel == R_BACK ? WHITE : BEIGE, 0, 1);
  if (hit(34, 436, 160, 30)) hover = R_BACK;
  cv_text(g_font_h14, 600, 443, "START >>", G.sel == R_START ? WHITE : GOLD, 2, 1);
  if (hit(440, 436, 200, 30)) hover = R_START;
  // input
  if (in->pressed[K_DOWN]) G.sel = (G.sel + 1) % R_N; if (in->pressed[K_UP]) G.sel = (G.sel + R_N - 1) % R_N;
  if (hover >= 0 && (in->mouse_dx || in->mouse_dy)) G.sel = hover;
  int dir = in->pressed[K_RIGHT] ? 1 : in->pressed[K_LEFT] ? -1 : 0;
  if (G.click && (hover == R_CAR || hover == R_TRACK)) dir = (hover == R_CAR ? G.mx > 170 : G.mx > 480) ? 1 : -1;
  int row = (G.click && hover >= 0) ? hover : G.sel;
  if (dir && (row == R_CAR || row == R_TRACK || row == R_LAPS)) {
    if (row == R_CAR) G.single_car = (car + NCARS + dir) % NCARS;
    else if (row == R_TRACK) G.single_track = (t + 6 + dir) % 6;
    else G.single_laps = G.single_laps == 3 ? 5 : 3;
    msfx(S_MENU2, 0.6f); single_save();
  }
  int act = (G.click && hover >= 0) ? hover : in->pressed[K_ENTER] ? G.sel : -1;
  if (in->pressed[K_ESC]) act = R_BACK;
  if (act == R_LAPS && !dir) { G.single_laps = G.single_laps == 3 ? 5 : 3; msfx(S_MENU2, 0.6f); single_save(); }
  if (act >= R_C1 && act <= R_C4) { *ci[act - R_C1].val ^= 1; msfx(S_MENU2, 0.6f); single_save(); }
  if (act == R_BACK) { single_save(); go(GS_MENU); return; }
  if (act == R_START || (act == R_CAR && in->pressed[K_ENTER]) || (act == R_TRACK && in->pressed[K_ENTER])) { msfx(S_MENU1, 0.8f); single_start(); return; }
  draw_cursor();
  canvas_draw(W, H);
  draw_turntable(W, H, G.single_car, G.turn, 40, 60, 260, 150, TRACKS[G.single_track].set);
}

#include "g_extras.inc"

// ------------------------------------------------------------------ main game frame
static int game_frame(int W, int H, double dt, const Input *in) {
  texture_canvas_reset();
  if(g_texture_reload){texture_cache_clear();g_texture_reload=0;}
  mem_zero(g_cv,CW*CH*4);g_cv_dirty=1;
  screen_compute(W, H);
  static Input pin; pin = *in; pad_apply(&pin, dt, W, H); in = &pin;   // game controller
  mouse_virtual(in, H, &G.mx, &G.my);
  G.click = in->lmb_pressed;
  G.t += dt; G.anim_t += dt;
  if (G.state == GS_QUIT) return 0;
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (G.state == GS_CLIP) return clip_frame(W, H, dt, in);
  if (G.state == GS_REPLAY) {
    if (in->pressed[K_SPACE]) RPL.paused ^= 1;
    if (in->pressed[K_LEFT]) RPL.t -= 5; if (in->pressed[K_RIGHT]) RPL.t += 5; if (RPL.t < 0) RPL.t = 0;
    if (!RPL.paused) RPL.t += dt;
    int f = replay_frame_at(RPL.t);
    if (in->pressed[K_ESC] || in->pressed[K_ENTER] || f >= RPL.h.nframes) { replay_end(); return 1; }
    replay_apply(f);
    g_replay_mode = 1; race_draw(W, H); g_replay_mode = 0;
    return 1;
  }
  if (G.state == GS_RACE) {
    if ((in->pressed[K_ESC] || in->pressed[K_P]) && RC.phase != PH_FLYBY) { RC.paused ^= 1; RC.pause_sel = 0; RC.pause_anim = 0; }
    g_gray = RC.paused ? 1.0f : 0.0f;
    if (RC.paused) {
      scrape_stop();
      RC.pause_anim += dt;
      RC.pause_slide += dt / 0.35; if (RC.pause_slide > 1) RC.pause_slide = 1;   // scroll down
      int old = RC.pause_sel;
      if (in->pressed[K_LEFT]) RC.pause_sel = (RC.pause_sel + 2) % 3; if (in->pressed[K_RIGHT]) RC.pause_sel = (RC.pause_sel + 1) % 3;
      int over = -1; if (G.my < 96 && RC.pause_slide >= 1) for (int i = 0; i < 3; i++) { const SprFile &d = g_pausedrum[i]; if (d.n && G.mx >= d.f[0].x * 2 && G.mx < (d.f[0].x + d.f[0].w) * 2) over = i; }
      if (over >= 0 && (in->mouse_dx || in->mouse_dy)) RC.pause_sel = over;
      if (old != RC.pause_sel) RC.pause_anim = 0;
      if (in->pressed[K_ENTER] || (G.click && over >= 0)) {
        if (RC.pause_sel == 0) RC.paused = 0;
        else if (RC.pause_sel == 1) { g_gray = 0; if (replay_open_current()) { replay_start(GS_RACE); return 1; } }
        else {   // abort: in the championship this counts as a lost race (game over, as in the original)
          RC.paused = 0; g_gray = 0; race_end_sounds(); music_stop(); RC.crashed = 0;
          if (PF.single) { single_end(); go(GS_MENU); }
          else {   // the classification is shown first, then Lance Boyle's game over clip
            RC.car[0].out = 1; RC.crashed = 1; G.aborted = 1; race_finished();
          }
          return 1;
        }
      }
      Voice *v = snd_voice(RC.eng_voice); if (v) v->vol = 0;
      Voice *v2 = snd_voice(RC.eng2_voice); if (v2) v2->vol = 0;
    } else {
      // the race runs in original updates (virtual 200 Hz clock, FIX/MOB interval); drawing never advances it
      race_advance(in, dt);
      if (RC.pause_slide > 0) { RC.pause_slide -= dt / 0.25; if (RC.pause_slide < 0) RC.pause_slide = 0; }   // scroll back up
    }
    race_draw(W, H);
    g_gray = 0;
    if ((RC.phase == PH_DONE && RC.done_t > 3.0) || (RC.crashed && RC.done_t > 3.5)) race_finished();
    return 1;
  }
  switch (G.state) {
    case GS_MENU: screen_menu(in); draw_cursor(); break;
    case GS_CARSHOP: screen_carshop(W, H, in, dt); return 1;
    case GS_WEAPONSHOP: screen_weaponshop(W, H, in, dt); return 1;
    case GS_TRACKINFO: screen_trackinfo(in); break;
    case GS_RESULTS: screen_results(in); draw_cursor(); break;
    case GS_PRIZE: screen_prize(in); break;
    case GS_GAMEOVER: screen_gameover(in); break;
    case GS_HISCORE: screen_hiscore(in); break;
    case GS_OPTIONS: screen_options(in); break;
    case GS_RETRO_SOUND: screen_retro_sound(in); break;
    case GS_PADMAP: screen_padmap(in); break;
    case GS_DEBUG: screen_debug(in); break;
    case GS_GFX: screen_gfx(in); break;
    case GS_REVIVE_EFFECTS: screen_revive_effects(in);break;
    case GS_EXTRAS: screen_extras(in);break;
    case GS_SAVELOAD: screen_saveload(in); break;
    case GS_REPLAYS: screen_replays(in); break;
    case GS_TRACKSELECT: screen_trackselect(W, H, in, dt); return 1;
    case GS_QUITDLG: screen_quitdlg(in); break;
    case GS_FONTTEST: {
      cv_clear(rgba(30, 30, 60, 255));
      char t[64]; int k = 0; for (int c = 0x7b; c < 0x96; c++) t[k++] = (char)c; t[k] = 0;
      cv_text(g_font_big, 10, 10, t, WHITE, 0, 1); cv_text(g_font_h14, 10, 60, t, WHITE, 0, 1); cv_text(g_font_h10, 10, 110, t, WHITE, 0, 1);
      cv_text(g_font_small, 10, 160, t, WHITE, 0, 1); cv_text(g_font_mid, 10, 210, t, WHITE, 0, 1);
      cv_text(g_font_h14, 10, 260, "\xc4\xd6\xdc \xe4\xf6\xfc \xdf  |  \xc0\xc9\xc8\xca \xc7 \xe9\xe8\xea \xe0\xe7\xef\xf4\xf9\xfb", WHITE, 0, 1);
      break; }
    case GS_MESSAGE: {
      if(G.msg_next==GS_CARSHOP){Input idle={};int click=G.click;G.click=0;screen_carshop(W,H,&idle,0);G.click=click;texture_canvas_reset();mem_zero(g_cv,CW*CH*4);g_cv_dirty=1;}else if(G.bg.px)cv_pic(G.bg,0,0,1);
      int mh=G.msg_next==GS_CARSHOP?76:160;int my=G.msg_next==GS_CARSHOP?202:160;
      cv_rect(105,my,430,mh,rgba(12,14,12,145));
      cv_text(g_font_small,320,my+14,G.msg,WHITE,1,1);
      cv_text(g_font_small,320,my+mh-25, "Taste dr\xfc" "cken", GREY, 1, 1);
      if (G.t > 0.3 && (G.click || in->pressed[K_ENTER] || in->pressed[K_ESC] || in->pressed[K_SPACE])) go(G.msg_next);
      draw_cursor();
      break;
    }
  }
  canvas_draw(W, H);
  return 1;
}

static void game_start(void) {
  // intro as in the original: Mindscape logo, Megarace 2 intro, Lance Boyle
  clip_queue("BOYLE/MIND.UBB"); clip_queue("BOYLE/L320_400.UBB"); clip_queue("BOYLE/INTRO25.UBB"); clip_queue("BOYLE/BOYLE01.UBB");
  // the game always starts in the main menu; a saved championship is continued from there
  int cont = load_game() && PF.active && PF.car >= 0;
  if (cont) G.loaded = 1; else { profile_new(); G.loaded = 0; }
  go(GS_MENU);
  clip_play_then(GS_MENU);
}
