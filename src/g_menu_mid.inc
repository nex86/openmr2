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
  cv_rect(x, y, w, h, G.gfx_mode==1?rgba(0,0,0,200):rgba(40,38,34,235));
  cv_rect(x, y, w, 2, rgba(150, 140, 120, 255)); cv_rect(x, y + h - 2, w, 2, rgba(70, 64, 56, 255));
  cv_rect(x, y, 2, h, rgba(150, 140, 120, 255)); cv_rect(x + w - 2, y, 2, h, rgba(70, 64, 56, 255));
  cv_text(g_font_h14, cx, y + 4, s, G.gfx_mode==1?rgba(231,154,0,255):ORANGE, 1, 1);
}
static Sprite spr_at(const Sprite &s, int x, int y) { Sprite r = s; r.x = x; r.y = y; return r; }

// DOS shop preview uses the opposite horizontal camera basis to the OpenGL viewer.
static M4 turntable_projection(float aspect){M4 p=m4_persp(32.0f*PI/180.0f,aspect,0.1f,200);p.m[0]=-p.m[0];return p;}
static const double TURNTABLE_RADIANS_PER_SECOND=2.1; // 3.5 times the former 0.6 rad/s
static double turntable_advance(double angle,double dt){double a=angle-dt*TURNTABLE_RADIANS_PER_SECOND;const double tau=6.283185307179586;while(a>=tau)a-=tau;while(a<0)a+=tau;return a;}
// rotating car (3D on the GPU) in a rectangle of the 640x480 screen
static void draw_turntable(int W, int H, int car, double angle, float x, float y, float w, float h, const char *set) {
  if (car < 0) return;
  if(A.graphics_test==14||A.graphics_test==15)angle=A.effect_age_ms*.001;
  Model *m = track_car_model(car);
  if (!m) { static Model *cache[NCARS]; if (!cache[car]) cache[car] = load_model_from(set, CARS[car].model); m = cache[car]; }
  if (!m) return;
  // Register the model with the pedestal, rather than the bottom of the old
  // preview rectangle. Use virtual coordinates in both presentation modes.
  int vx, vy, vw, vh; vrect_viewport(W, H, x, y-h*.08f, w, h, &vx, &vy, &vw, &vh);
  glViewport(vx, vy, vw, vh);
  glEnable(GL_DEPTH_TEST); glClear(GL_DEPTH_BUFFER_BIT);
  V3 ext = m->bmax - m->bmin; float r = m_sqrt(dot(ext, ext)) * 0.5f;
  M4 proj = turntable_projection(w/h);
  V3 eye = v3(0, r * 0.9f, r * 2.9f);
  M4 view = m4_look(eye, v3(0, ext.y * 1.1f, 0), v3(0, 1, 0));
  M4 vp = m4_mul(proj, view);
  V3 c = (m->bmin + m->bmax) * 0.5f;
  M4 base = m4_mul(m4_roty((float)angle),m4_trans(v3(-c.x,-m->bmin.y,-c.z)));
  int old_hide=g_hide_model_shadows;g_hide_model_shadows=1;
  preview_shadow_draw(*m,vp,base,(float)(angle*3));
  model_shader_begin(vp,1);glUniform1i(R.um_lamps,g_vehicle_lights&&(!g_tex_nearest));glUniform1i(R.um_rear_only,A.graphics_test==15);glUniform1f(R.um_brake,A.graphics_test==15&&A.test_damage?1.f:0.f);
  glUniformMatrix4fv(R.um_vp, 1, GL_FALSE, vp.m); glUniform3f(R.um_cam, eye.x, eye.y, eye.z);
  glUniform1i(R.um_tex, 0); glUniform1f(R.um_lit, 1.0f); glUniform1f(R.um_gain, 1.0f); glUniform1f(R.um_gray, 0.0f);
  draw_model_mv(*m, base, (float)(angle * 3), 0);
  preview_lights_draw(*m,vp,base,eye,(float)(angle*3),W,H);
  g_hide_model_shadows=old_hide;
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

