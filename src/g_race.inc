#include "original_drive.h"
#include "original_body.h"
// ===================================================================================================
// The race: car physics in road coordinates (the original steers by rotating the car, which then
// drives in the direction it is facing), camera that follows the car through the pre-rendered video,
// computer opponents, weapons, road objects, laps, positions, HUD and sound.
// ===================================================================================================

// analog values of the game controller (set by the menu module every frame)
static float g_pad_steer, g_pad_gas, g_pad_brake; static int g_pad_analog;
static int g_cam_rot_interp;
static int g_sim_model = 0;          // 0 Original (one update per step), 1 Revived (subdivided physics)
// Counters that the original advances once per simulation step (weapon locks, oil, zigzag, recovery) run in
// steps in the Original model (40 ms units = SIM_STEP each step), in the Revived model in seconds.
static double cnt_dt(double dt) { return g_sim_model == 0 ? 0.04 : dt; }
static int g_cheat_ammo, g_cheat_nodmg;   // cheats of the running race (set by the menu module per mode)
static int g_dbg_noopp, g_dbg_info, g_dbg_fps; static int g_test_wrongway;      // debug: opponents stand still, race info on screen   // debug: interpolate the camera orientation too (and turn the video picture)

enum { W_NONE, W_KARD, W_BOOMER, W_HITRAX, W_MINE, W_OIL, W_SHIELD };
// prices and texts as shown in the original weapon shop
struct WeaponDef { const char *name, *launcher; int launcher_price, ammo_price, capacity; double damage; int homing; const char *explosive, *dmgtxt, *shortname, *lshort; };
static const WeaponDef WEAPONS[7] = {
  {"-", "-", 0, 0, 0, 0, 0, "", "", "", ""},
  {"Kard Missile", "TRax L3 Missilewerfer", 1000, 250, 25, 2666 / 655.36, 0, "Nitroglyzerin", "Niedrig", "Kleines Missile", "Werfer fuer kleine Missiles"},
  {"Boomer Missile", "TRax L6 Missilewerfer", 1500, 375, 20, 6666 / 655.36, 0, "Spaltmaterial", "Mittel", "Mittleres Missile", "Werfer fuer mittlere Missiles"},
  {"Hi-Trax Missile", "TRax L9 Missilewerfer", 2000, 500, 10, 16666 / 655.36, 1, "Thorium", "Hoch", "Grosses Missile", "Werfer fuer grosse Missiles"},
  {"Mine Th-1", "Minenwerfer MKII", 1000, 200, 10, 6666 / 655.36, 0, "Thorium", "Hoch", "Minen", "Minenwerfer"},
  {"Oil Sk-N", "Oelwerfer Od.V", 1000, 200, 10, 0, 0, "", "", "Oel", "Oelwerfer"},
  {"Schutzschild SH-24", "Schutzschild SH-24", 1500, 0, 4, 0, 0, "", "", "Schutzschild", "Schutzschild"},
};

// fill when a launcher is bought (M.EXE): L3 12, L6 10, L9 4, mine 4, oil 4
static const int LAUNCHER_FILL[7] = {0, 12, 10, 4, 4, 4, 0};
struct Loadout { int front[2], fammo[2], rear[2], rammo[2]; int fsel, rsel; int shield; };

struct RCar {
  int car, player,braking; char name[24];
  PPos pp; double x, psi, v, vmax, acc, hwm;   // psi: physical heading relative to the road; v: drive speed; hwm: half width of the model
  OriginalDrive fixed;int fixed_ready;int fixed_commit;
  double vlat, vlong;                 // actual motion (road frame, world units/s): follows the heading with grip / slip
  double prog, lastM;                 // unwrapped position along the main loop (points), for laps and ranking
  double W[3], F[3], Rt[3], U[3];     // world position and road frame (updated every step); U = the road's up axis
  double damage, shield; int out;     // damage 0..100 % of the hull (65536), shield 0..100 % of 65535
  int finished; double finish_t; int place;
  double stun, oil_t, flash, wall_cd, hit_cd,spark_cd; int wall_scraping;
  double steer_st;                    // steering state (original units, -32768..32768); + = right
  double yaw;                         // current yaw rate (rad/s), for the visual roll
  int profile;                        // driving profile 1..3
  int autopilot, spawn_slot;          // rolling start: computer control until the car crosses the line
  // computer driver
  double lane, base_lane, desired_v;  // desired lane (-256..256 of the half width), target speed (world units/s)
  int look;                           // look-ahead in road links
  int tactic_mask, tactic_active; double tactic_t, recover_t, bad_t, avoid_t;
  int fork_node, fork_side;
  double front_cd, rear_cd, boomer_cd; int hitrax_live;   // weapon locks (original: 12 / 25 / 30 steps of 40 ms)
  double ai_fire_t;
  double stuck_t, stuck_prog;          // KI-Notfallhilfe: time without new best progress, best progress so far
  int pose_gen;                        // changes when the car is set to a new place (no interpolation across it)
  double wheel;                       // unwrapped wheel rotation in radians, shared by rendering and replays
  Loadout lo; int hits, kills, earned;
};
struct Proj { int kind, owner, target, alive; PPos pp; double x, psi, v, life, vlat, vlong; int gen; };   // gen: new for every shot (slot reuse)
static int g_proj_gen;
struct Hazard { int kind, owner, alive; PPos pp; double x, life, arm, vlong, vlat; double W[3]; int hit_mask; };
struct Fx { int model, alive; double W[3]; double t, scale; };

enum { PH_FLYBY, PH_GRID, PH_RACE, PH_DONE };
enum { HIST = 4096 };
static int g_replay_mode;       // race_draw: use the recorded camera

struct Race {
  int active; RCar car[8]; int ncar;
  Proj proj[32]; Hazard haz[48]; Fx fx[24];
  double t; int laps; int phase; double phase_t;
  // camera: follows the player along the road points the player has passed (history)
  int hist[HIST]; int hn;
  double cam_D, cam_v, ply_D;         // travelled distance (world units) of camera and player along the player's path
  PPos campp; int frame; double vcam[12]; int reverse_view;   // vcam: camera interpolated between two video frames (objects move smoothly)
  char msg[64]; double msg_t; uint32_t msg_col;
  int paused, pause_sel, quit; double wrong_t; double done_t, pause_anim; double pause_slide;   // pause_slide: 0 hidden .. 1 menu bar fully down
  float gain; int eng_voice, eng2_voice, scrape_voice;
  int order[8];
  int crashed; int last_lap;
  int heat, final_race;               // heat 0..2; the final has no weapons and no special tactics
  int zig_counter, zig_sign; double zig_t; int fire_counter;   // global AI counters (as in the original)
  // original camera controller and race cadence (M.EXE _MOB_/_FIX_CALCUL_FRAME_CAMERA, _CALCUL_TEMPS_CAMERA)
  int cam_mob, cam_just, cam_cnt, cam_frame, cam_speed, delta_units;
  long long timer3d, cam_old;          // virtual 200 Hz clock (256 units per 5 ms tick) and time of the last update
  double clock_ms;                     // real race time that drives the virtual clock
  int pkg_id, pkg_repeat, late_n, upd_n; // presentation package: id, how often it was shown, late updates, updates
  int pkg_ubb;                         // UBB seek offset of the package's frame
};
static Race RC;

// race sounds (SFX folder of the game)
enum { S_ENGINE, S_ENGINE2, S_CHOC1, S_CHOC3, S_CHOC5, S_FROTTE, S_EXPLO1, S_EXPLO2, S_EXPLO3, S_MISSILE2, S_MISSILE3, S_MISSILE4,
       S_MINE, S_OIL, S_DERAPOIL, S_DERAP, S_BIDON, S_PLOT, S_MENU1, S_MENU2, S_MENU3, S_MENU4, S_SCREAM, S_TURBINE, S_F1, S_ENGINE3, S_COUNT };
static const char *SFX_FILES[S_COUNT] = {"TURBINE", "M_2", "CHOC1", "CHOC3", "CHOC5", "FROTTE2", "EXPLO1", "EXPLO2", "EXPLO3", "MISSIL2", "MISSIL3", "MISSIL4",
                                         "MINE", "OIL", "DERAPOIL", "DERAP_A", "BIDON", "PLOT3", "MENU1", "MENU2", "MENU3", "MENU4", "SCREAM", "TURBINE", "F1", "MOT3"};
static Sound SFX[S_COUNT];
static int sfx_bus(int i){return i==S_ENGINE||i==S_ENGINE2||i==S_ENGINE3||i==S_TURBINE?AUDIO_ENGINES:i>=S_MENU1&&i<=S_SCREAM?AUDIO_MENU_SOUNDS:AUDIO_SOUNDS;}
static Sound *sfx_sample(int i,int bus){
  if(i<0||i>=S_COUNT||!audio_enabled(bus))return 0;
  if(!SFX[i].pcm){ char p[512]; str_cpy(p, "SFX/", 512); str_cat(p, SFX_FILES[i], 512); str_cat(p, ".WAV", 512); char full[512]; game_path(full, p); sound_load_wav(&SFX[i], full); }
  return &SFX[i];
}
static void sfx_load_all(void) { for(int i=0;i<S_COUNT;i++)sfx_sample(i,sfx_bus(i)); }
static int engine_play(int id,float vol,float pitch){return snd_play(sfx_sample(id,AUDIO_ENGINES),vol,pitch,1,0,AUDIO_ENGINES);}
static int car_engine_id(int car){int level=0,profile=mexe_engine_profile(car,&level);static const int ids[4]={S_ENGINE,S_ENGINE2,S_ENGINE3,S_ENGINE2};return profile>=0?ids[profile]:-1;}
static float car_engine_gain(int car){int level=0;mexe_engine_profile(car,&level);return level/20.f;}
static int car_engine_play(int car,float vol,float rate){int id=car_engine_id(car);if(id<0)return -1;return engine_play(id,vol*car_engine_gain(car),rate);}
static void car_engine_select(int *handle,int car,float vol,float rate){
  int id=car_engine_id(car);Voice *voice=snd_voice(*handle);
  if(id<0){snd_stop(*handle);*handle=-1;return;}
  if(voice&&voice->snd!=&SFX[id]){snd_stop(*handle);voice=0;}
  if(!voice)*handle=car_engine_play(car,vol,rate);
}
static void sfx(int id, float vol) { snd_play(sfx_sample(id,AUDIO_SOUNDS), vol, 1.0f, 0, 0); }
static void scrape_stop(){snd_stop(RC.scrape_voice);RC.scrape_voice=-1;}
static int projectile_model(int kind){return kind==W_KARD?OM_MISSILE:kind==W_BOOMER?OM_MISSILE_MID:kind==W_HITRAX?OM_MISSILE_BIG:-1;}
static double dist_pts(const double a[3], const double b[3]) { double dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2]; return d_sqrt(dx * dx + dy * dy + dz * dz) / 6900.0; }
static void sfx_at(int id, const double W[3], float vol);   // attenuated by the distance to the player
static void sfx_at_pts(int id, double d, float vol) {
  d = d_abs(d); if (d > 40) return;
  snd_play(sfx_sample(id,AUDIO_SOUNDS), (float)(vol * (1.0 - d / 45.0)), (float)frnd_fx(0.95, 1.05), 0, 0);
}


// ---------------------------------------------------------------------------------------------------
// Units: positions in original world units (road half width at the Foundry start ~37590, Thunder V.
// 12362 wide). Speeds: the original raw drive speed (player limit 1036) is converted with V_RAW, which
// comes from a measurement in the original (top speed ~29.5 road points per second).
// Values marked "M.EXE" are taken from the original program (see the specification).
// ---------------------------------------------------------------------------------------------------
static const double PT = 6900.0;                  // nominal distance between two road points
static const double V_RAW = 200.0;                // world units per second per raw speed unit: the MOB camera moves one
                                                  // node (0x1B00) per 0x1B0000/speed units of 1/51200 s (M.EXE 0x2A7B1)
static const double V_PLAYER = 1036 * V_RAW, V_AI = 1140 * V_RAW, V_BOOST = 1290 * V_RAW, V_REVERSE = -691 * V_RAW, V_START = 518 * V_RAW;   // M.EXE
static const double MPH_TO_UNITS = V_PLAYER / 120.0;   // only for the speed shown in the debug console
static const double SIM_STEP = 0.04;              // original simulation step: 2048 / 51200 s = 40 ms (25 steps per second)
static const double ACCEL = V_PLAYER / 5.0;       // gas (command 400): measured in the original, ~5 s to the limit
static const double CAR_SCALE = 1.0;              // models are drawn at their original size (the projection is now the original one)
static const double GRID_S = 17.0;                // (old fixed grid, only for tests with --frame)
static double g_grid_v;
static const unsigned char TACTIC_MASKS[6][8] = {   // M.EXE: per track, per driver slot (slot 0 = player)
  {0x00, 0x20, 0x00, 0x00, 0, 0, 0, 0}, {0x00, 0x02, 0x20, 0x00, 0, 0, 0, 0}, {0x00, 0x10, 0x40, 0x20, 0, 0, 0, 0},
  {0x00, 0x06, 0x20, 0x00, 0, 0, 0, 0}, {0x00, 0x10, 0x08, 0x02, 0, 0, 0, 0}, {0x00, 0x60, 0x02, 0x00, 0, 0, 0, 0}};
enum { T_FREINE = 1, T_POISSON = 2, T_ZIGZAG = 4, T_ALIGN = 8, T_BARRAGE = 16, T_BOOSTER = 32, T_COLLE = 64 };

static RCar &player(void) { return RC.car[0]; }
static void sfx_at(int id, const double W[3], float vol) { sfx_at_pts(id, dist_pts(W, player().W), vol); }
static int g_impact_last=-1;
static void impact_sound(const double *W,float volume){
  if(!audio_enabled(AUDIO_SOUNDS)||dist_pts(W,player().W)>40)return;
  static const int sounds[]={S_CHOC1,S_CHOC3,S_CHOC5};
  int index=irnd_fx(g_impact_last<0?3:2);
  if(g_impact_last>=0&&index>=g_impact_last)index++;
  g_impact_last=index;
  sfx_at(sounds[index],W,volume);
}
// how far the car's centre may move from the road centre: PST +0x48 is the distance from the centre to the
// boundary (M.EXE); the side of the car touches the boundary
static void scrape_update(){
  const RCar &c=RC.car[0];
  if(!audio_enabled(AUDIO_SOUNDS)||!c.wall_scraping||c.out||c.finished||RC.paused||RC.phase!=PH_RACE){scrape_stop();return;}
  Voice *voice=snd_voice(RC.scrape_voice);
  if(!voice){RC.scrape_voice=snd_play(sfx_sample(S_FROTTE,AUDIO_SOUNDS),0,1,1,0,AUDIO_SOUNDS);voice=snd_voice(RC.scrape_voice);}
  if(voice){voice->vol=MX.sfx_vol*(float)d_clamp(d_abs(c.vlong)/V_PLAYER,0.1,1)*0.75f;voice->pan=c.x>=0?0.45f:-0.45f;}
}
#include "original_wall.h"
#include "original_road.h"
#include "original_contact.h"
static int contact_raw(double v){return (int)(v<0?v-.5:v+.5);}
static double car_hw(const RCar &c) {
  int angle=contact_raw(-c.psi*1024/6.2831853071795864769)&1023;
  double half=pp_halfw(c.pp),hull=ORIGINAL_HULL_EXTENT[angle];
  return half-hull>half*.3?half-hull:half*.3;
}
static int hard_wall_impact(double normal){return normal>0&&normal*normal>=0xe600*V_RAW*V_RAW;}

static void car_update_world(RCar &c) {
  double p[3]; pp_frame(c.pp, p, c.Rt, c.U, c.F, 0);
  for (int k = 0; k < 3; k++) c.W[k] = p[k] + c.Rt[k] * c.x;
}
// relative position of world point w in the road frame of car c: along (forward) and lateral
static void rel_to(const RCar &c, const double w[3], double *along, double *lat) {
  double d[3] = {w[0] - c.W[0], w[1] - c.W[1], w[2] - c.W[2]};
  *along = d[0] * c.F[0] + d[1] * c.F[1] + d[2] * c.F[2];
  *lat = d[0] * c.Rt[0] + d[1] * c.Rt[1] + d[2] * c.Rt[2];
}
// height of world point w above the road surface at car c (along the road's own up axis)
static double height_at(const RCar &c, const double w[3]) { return (w[0] - c.W[0]) * c.U[0] + (w[1] - c.W[1]) * c.U[1] + (w[2] - c.W[2]) * c.U[2]; }
static int graph_links(int a, int b, int maxn);
// are two road nodes on the same driveable road within maxn links (forwards or backwards)?
static int nodes_connected(int a, int b, int maxn) { return a == b || graph_links(a, b, maxn) >= 0 || graph_links(b, a, maxn) >= 0; }
// Is world point w (which belongs to road node `node`, -1 = unknown) on the same road part as car c?
// Three separate tests: lateral (not behind the barrier of a parallel lane), height (not on a bridge above or a
// road below) and road graph (not a separate road at the same height). The limits depend on the purpose:
// perception reaches further than body contact.
enum { NEAR_CONTACT = 0, NEAR_PERCEIVE = 1 };
static int same_road_at(const RCar &c, const double w[3], int node, int purpose) {
  double a, l; rel_to(c, w, &a, &l);
  if (d_abs(c.x + l) > pp_halfw(c.pp) + 4000) return 0;
  if (d_abs(height_at(c, w)) > (purpose == NEAR_CONTACT ? 5000 : 9000)) return 0;
  if (node >= 0 && !nodes_connected(c.pp.i, node, purpose == NEAR_CONTACT ? 6 : 20)) return 0;
  return 1;
}
static int same_road(const RCar &c, const double w[3]) { return same_road_at(c, w, -1, NEAR_PERCEIVE); }

static void fx_add(int model, const PPos &q, double x, double h, double scale) {
  for (int i = 0; i < 24; i++) if (!RC.fx[i].alive) { Fx &f = RC.fx[i]; f.model = model; pp_world(q, x, h, f.W); f.t = 0; f.scale = scale; f.alive = 1; return; }
}
static void msg_show(const char *m, double t, uint32_t col) { str_cpy(RC.msg, m, 64); RC.msg_t = t; RC.msg_col = col; }

// damage in % of the hull. The shield (65535) takes the hit first, an overflow goes on to the hull (M.EXE).
static void damage_car(RCar &c, double dmg, int by) {
  if (c.out || c.finished || dmg <= 0) return;
  if (c.player && g_cheat_nodmg) return;
  if (c.shield > 0) { c.shield -= dmg; if (c.shield < 0) { dmg = -c.shield; c.shield = 0; } else dmg = 0; }
  c.damage += dmg; c.flash = 0.6;
  if (by >= 0 && RC.car[by].player && !c.player) player().hits++;
  if (c.damage >= 100) {
    c.damage = 100; c.out = 1; c.v = 0; c.vlong = c.vlat = 0;
    if(g_tex_nearest||g_damage_effects){fx_add(OM_EXP_BIG,c.pp,c.x,2500,g_tex_nearest?1.6:2.4);sfx_at(S_EXPLO3,c.W,1.0f);}
    if (c.player) { RC.crashed = 1; msg_show("Du hast einen Autounfall!", 4, rgba(255, 255, 255, 255)); }
    else if (by >= 0 && RC.car[by].player) player().kills++;
  }
}

// ------------------------------------------------------------------ player history (the camera drives along it)
static void hist_push(int pt) { if (RC.hn > 0 && RC.hist[(RC.hn - 1) % HIST] == pt) return; RC.hist[RC.hn % HIST] = pt; RC.hn++; }
static void on_player_enter(int pt, int) { hist_push(pt); }
static void hist_sync(void) {   // after the player moved backwards over a point
  RCar &P = player();
  if (RC.hn >= 2 && RC.hist[(RC.hn - 1) % HIST] != P.pp.i && RC.hist[(RC.hn - 2) % HIST] == P.pp.i) RC.hn--;
  else if (RC.hn > 0 && RC.hist[(RC.hn - 1) % HIST] != P.pp.i) hist_push(P.pp.i);
}
// position "back" world units behind the player along his path
static PPos cam_from_back(double back, int *hidx) {
  RCar &P = player();
  PPos q = P.pp;
  double d_in = q.t * seg_len(q.i, q.nx);
  int k = RC.hn - 1;
  if (back <= d_in) { q.t = (d_in - back) / seg_len(q.i, q.nx); *hidx = k; return q; }
  back -= d_in;
  int cur = q.i;
  for (int g = 0; g < 200; g++) {
    int prev = (k - 1 >= 0 && RC.hn - k < HIST - 1) ? RC.hist[(k - 1) % HIST] : RP(cur).pv[0];
    double L = seg_len(prev, cur);
    if (back <= L) { q.i = prev; q.nx = cur; q.t = 1 - back / L; *hidx = k - 1; return q; }
    back -= L; cur = prev; k--;
  }
  q.i = cur; q.nx = RP(cur).nx[0]; q.t = 0; *hidx = k; return q;
}

static void original_motion_reset();
// ------------------------------------------------------------------ setup
// Starting grid (M.EXE, _POSITIONNE_CAR_START): start slot r goes 52 + 5*r road links back from point 0
// (link at PST +0x6C), even slots +10000 (right), odd slots -10000 (left), lane target +-128, speed 518,
// look-ahead 8, autopilot on until the car crosses the start line.
static int grid_point(int r) {
  int i = 0;
  for (int k = 0; k < 52 + 5 * r; k++) { int j = RP(i).pv[1]; i = (j >= 0 && j < TK.nroad) ? j : RP(i).pv[0]; }
  return i;
}
static void race_cam_init(void);
static void race_end_sounds(void);
static void effects_reset();
static void lamp_history_reset();
static void race_setup(int car_for_player, const Loadout &lo, double dmg, int opp_cars[7], int nopp, int start_rank, int laps) {
  original_motion_reset();
  effects_reset();lamp_history_reset();
  race_end_sounds(); // release old handles before resetting RC, including repeated test starts
  g_impact_last=-1;
  int heat = RC.heat;
  mem_zero(&RC, sizeof RC);
  RC.heat = heat; RC.final_race = heat >= 2;
  RC.active = 1; RC.laps = laps; RC.phase = PH_GRID; RC.gain = 1;
  if(nopp<0)nopp=0;if(nopp>7)nopp=7;
  RC.ncar = A.objtest ? 1 : 1 + nopp;   // --objtest: the player alone and standing (object placement test)
  if (start_rank < 0 || start_rank >= RC.ncar) start_rank = RC.ncar - 1;
  for (int i = 0; i < RC.ncar && i < 8; i++) {
    RCar &c = RC.car[i];
    c.player = i == 0;
    c.car = i == 0 ? car_for_player : opp_cars[i - 1];
    str_cpy(c.name, ui_translate(CARS[c.car].name), 24);
    c.profile = CARS[c.car].profile;
    c.vmax = c.player ? V_PLAYER : V_AI;
    c.acc = ACCEL;
    { Model *mm = track_car_model(c.car); c.hwm = mm ? (mm->bmax.x - mm->bmin.x) * 0.5 / MODEL_SCALE * CAR_SCALE : 6000; }
    c.damage = c.player ? dmg : 0;
    int r = c.player ? start_rank : (i - 1 < start_rank ? i - 1 : i);
    c.spawn_slot = r;
    if (A.start_frame > 0) { double s0 = A.start_frame - r * 1.8; int si = (int)s0; c.pp = pp_make(si, s0 - si); }
    else c.pp = pp_make(grid_point(r), 0);
    c.x = (r % 2 == 0) ? 10000 : -10000;
    c.lane = c.base_lane = (r % 2 == 0) ? 128 : -128;
    c.look = 8;
    c.lastM = pp_M(c.pp); c.prog = wrapM(c.lastM);   // behind the line: negative progress
    c.stuck_prog = c.prog;
    c.v = c.vlong = V_START; c.desired_v = V_START; c.fork_node = -1;
    c.autopilot = A.start_frame > 0 ? 0 : 1;
    if (A.objtest) { c.v = c.vlong = c.desired_v = 0; c.autopilot = 0; }
    c.tactic_mask = (!c.player && !RC.final_race && i < 8) ? TACTIC_MASKS[TK.idx % 6][i] : 0;
    if (c.player) { c.lo = lo; c.shield = lo.shield == 1 ? 100 : lo.shield; }   // remaining shield from the last race (1 = full: older saves)
    else if (!RC.final_race) {
      mem_zero(&c.lo, sizeof c.lo);   // opponents carry the original start equipment
      c.lo.front[0] = W_KARD; c.lo.fammo[0] = 12; c.lo.front[1] = W_BOOMER; c.lo.fammo[1] = 20; c.lo.rear[0] = W_MINE; c.lo.rammo[0] = 2;
      if (irnd(2)) { c.lo.rear[1] = W_OIL; c.lo.rammo[1] = 4; }
      c.ai_fire_t = frnd(2, 5);
    }
    car_update_world(c);
  }
  g_grid_v = V_START;
  // camera history: the road behind the player's start
  { int pts[64], n = 0, cur = player().pp.i; for (; n < 60; n++) { pts[n] = cur; cur = RP(cur).pv[0]; }
    for (int k = n - 1; k >= 0; k--) hist_push(pts[k]); }
  RC.ply_D = 0; RC.cam_D = -7.5 * PT; RC.cam_v = player().v;
  RC.frame = 0; RC.zig_sign = 1;
  race_cam_init();
  {  // M.EXE: "FERTIG ZUM RENNEN" / "Runde Nr. " (number of the race in the round) / " - Keine Waffen" in the final
    char m[64] = ""; str_cpy(m, ui_translate("FERTIG ZUM RENNEN\nRunde Nr. "), 64); str_int(m, 64, RC.heat + 1); if (RC.final_race) str_cat(m, ui_translate(" - Keine Waffen"), 64);
    msg_show(m, 1e9, rgba(255, 255, 255, 255)); }   // stays during the whole rolling start (START01-08), replaced by START !!!
  g_engine_suppressed=0;
  RC.eng_voice = car_engine_play(player().car,0.5f,mexe_engine_frequency((int)(player().v/V_RAW))/22050.f);
  RC.eng2_voice = -1;
  RC.scrape_voice = -1;
  sys_log("[event] Race initialized; engine loops started.");
}
static void race_end_sounds(void) {
  g_engine_suppressed=1;
  if(snd_voice(RC.eng_voice)||snd_voice(RC.eng2_voice)||snd_voice(RC.scrape_voice))sys_log("[event] Race engine/scrape loops stopped.");
  snd_stop(RC.eng_voice); snd_stop(RC.eng2_voice); snd_stop(RC.scrape_voice);
  // A replay/state reset can lose a handle. Retire the actual race-loop samples too,
  // without stopping unrelated voices which have reused an old mixer slot.
  for(int i=0;i<NVOICES;i++)if(MX.v[i].bus==AUDIO_ENGINES || MX.v[i].snd==&SFX[S_ENGINE] || MX.v[i].snd==&SFX[S_ENGINE2] || MX.v[i].snd==&SFX[S_ENGINE3] || MX.v[i].snd==&SFX[S_FROTTE])MX.v[i].active=0;
  RC.eng_voice = RC.eng2_voice = RC.scrape_voice = -1;
}

// ------------------------------------------------------------------ weapons
// Projectile drive values (M.EXE): small 2850, medium 2280, large 1710 (raw speed units); new motion =
// (old + 3 * new) / 4 per step, the large one halves. Spawn 15000 in front of / behind the car.
// Locks: front 12 steps, medium missile another 25, rear 30 steps; one large missile at a time.
static void fire_front(RCar &c, int idx) {
  if (RC.final_race || c.front_cd > 0) return;
  Loadout &lo = c.lo; int sl = lo.fsel;
  if (lo.front[sl] == W_NONE || lo.fammo[sl] <= 0) { sl ^= 1; if (lo.front[sl] == W_NONE || lo.fammo[sl] <= 0) return; }
  int kind = lo.front[sl];
  if (kind == W_BOOMER && c.boomer_cd > 0) return;
  if (kind == W_HITRAX && c.hitrax_live) return;
  for (int i = 0; i < 32; i++) if (!RC.proj[i].alive) {
    Proj &p = RC.proj[i]; p.alive = 1; p.kind = kind; p.owner = idx; p.target = -1; p.gen = ++g_proj_gen;
    p.pp = c.pp; p.x = c.x; p.psi = c.psi; p.life = 4.0;
    p.vlong = c.vlong; p.vlat = c.vlat;   // starts with the car's motion
    p.v = (kind == W_KARD ? 2850 : kind == W_BOOMER ? 2280 : 1710) * V_RAW;
    double fx = 15000 * d_cos(c.psi); pp_move(p.pp, p.x, fx); p.x += 15000 * d_sin(c.psi);
    if (!(c.player && g_cheat_ammo)) lo.fammo[sl]--;
    c.front_cd = 12 * SIM_STEP; if (kind == W_BOOMER) c.boomer_cd = 25 * SIM_STEP; if (kind == W_HITRAX) c.hitrax_live = 1;
    sfx_at(kind == W_KARD ? S_MISSILE2 : kind == W_BOOMER ? S_MISSILE3 : S_MISSILE4, c.W, 0.8f);
    return;
  }
}
static void drop_rear(RCar &c, int idx) {
  if (RC.final_race || c.rear_cd > 0) return;
  Loadout &lo = c.lo; int sl = lo.rsel;
  if (lo.rear[sl] == W_NONE || lo.rammo[sl] <= 0) { sl ^= 1; if (lo.rear[sl] == W_NONE || lo.rammo[sl] <= 0) return; }
  for (int i = 0; i < 48; i++) if (!RC.haz[i].alive) {
    Hazard &h = RC.haz[i]; h.alive = 1; h.kind = lo.rear[sl]; h.owner = idx; h.hit_mask = 0;
    h.pp = c.pp; h.x = c.x; pp_move(h.pp, h.x, -15000);
    if (h.kind == W_MINE) { h.vlong = c.vlong * 0.75; h.vlat = c.vlat * 0.75; } else h.vlong = h.vlat = 0;   // mine: 3/4 of the car's motion, then damped
    pp_world(h.pp, h.x, 0, h.W);
    h.life = h.kind == W_OIL ? 2.2 * TK.L / 29.5 : 120; h.arm = 0.3;   // oil: "2 Runden" (shop text)
    if (!(c.player && g_cheat_ammo)) lo.rammo[sl]--;
    c.rear_cd = 30 * SIM_STEP;
    sfx_at(h.kind == W_MINE ? S_MINE : S_OIL, c.W, 0.7f);
    return;
  }
}

// ------------------------------------------------------------------ driving
// Same core for player and computer cars (M.EXE _CALCUL_VITESSE_UNE_VOITURE): drive command (gas 400,
// coast -200, brake -500) integrated into the drive speed, limits 1036 (player) / 1140 (computer) /
// 1290 (booster) / -691 (reverse); a steering state that builds up with VitTurn, is limited by AnglMax and
// returns with VitRet; yaw rate = state * (1 - q*100/256) * speed / 1140 / 16 (1024 units per turn);
// the motion follows the heading with grip (half mix per 40 ms step) and slips under hard braking or oil.
static void contact_sparks(const double *W,const double *up,const double *velocity,float intensity,int grinding=0);
#include "g_original_motion.inc"
static void car_physics(RCar &c,int drive,double steer,double dt,double vlimit){
  if(c.stun>0)vlimit*=.6;
  if(c.v>vlimit&&drive>0)drive=-1;
  c.braking=drive<0&&c.v>0&&!c.out;
  if(c.out){mem_zero(&c.fixed,sizeof c.fixed);c.fixed.oil=-1;c.fixed.angle=contact_raw(-c.psi*1024/6.2831853071795864769)&1023;c.fixed_ready=1;g_original_old_angle[&c-RC.car]=c.fixed.angle;}
  else original_car_drive(c,drive,steer,dt);
  original_car_move(c,dt);
}

// ------------------------------------------------------------------ computer driver
// next fork within a few points ahead (following the first link), or -1
static int fork_ahead(int i, int maxpts) {
  for (int g = 0; g < maxpts; g++) { const RoadPt &r = RP(i); if (r.nx[0] != r.nx[1]) return i; i = r.nx[0]; }
  return -1;
}
static int next_link(RCar &c, int i) {   // the link the driver takes at point i (fork choice)
  const RoadPt &r = RP(i);
  if (r.nx[0] == r.nx[1]) return r.nx[0];
  if (c.fork_node != i) {   // Decide_Aiguillage: lanes without video are never taken; otherwise follow the player now and then
    c.fork_node = i; c.fork_side = irnd(2);
    int v0 = lane_has_video(r.nx[0]), v1 = lane_has_video(r.nx[1]);
    if (v0 != v1) c.fork_side = v0 ? 0 : 1;
    else if (!c.player && irnd(2)) { double a = d_abs(lat_at(r.nx[0], player().W)), b = d_abs(lat_at(r.nx[1], player().W)); c.fork_side = a <= b ? 0 : 1; }
  }
  return r.nx[c.fork_side];
}
// Steering controller (M.EXE _CALCUL_PILOTAGE_VOITURE): aim point `look` links ahead at
// lane/256 of the way from the centre to the boundary, error in the car frame, then
// e = (errX * -600) >> 16, response = (w * errZ) >> 16, gain player -60 / computer (-120 - extra) >> 1,
// command = clamp(e + (response * gain) >> 8, -128, 128); a negative command turns right.
static double pilot_steer(RCar &c){
  drive_trig_load();int look=c.look;if(c.desired_v<0)look=-look;bool backward=look<0;if(look<0)look=-look;if(look>11)look=11;if(look<1)look=1;
  int car=(int)(&c-RC.car);if(g_road_plane_car[car]<0||g_road_plane_frames[g_road_plane_car[car]].node!=c.pp.i)road_plane_frames_build();
  int frame_index=g_road_plane_car[car],node=c.pp.i;OriginalPlane own=g_road_plane_frames[frame_index].pose,frame=own;
  for(int i=0;i<look;i++){const PhysicsRoadFrame &f=g_road_plane_frames[frame_index];int branch=(f.flags&7)==(backward?6:5)&&c.lane>=0?1:0;int next=backward?f.prev[branch]:f.next[branch];if(next<0)break;frame_index=next;frame=g_road_plane_frames[frame_index].pose;node=g_road_plane_frames[frame_index].node;}
  int width=contact_raw(RP(node).w),cosine=g_drive_trig[frame.heading],sine=g_drive_trig[frame.heading+256];
  int edge_x=frame.x+(int)(((int64_t)width*cosine)>>15),edge_z=frame.z+(int)(((int64_t)width*sine)>>15);
  int aim_x=frame.x+(op_product(edge_x-frame.x,contact_raw(c.lane))>>8),aim_z=frame.z+(op_product(edge_z-frame.z,contact_raw(c.lane))>>8);
  int ex,ez;original_plane_local(own,aim_x,aim_z,ex,ez,g_drive_trig,g_drive_trig+256);ex-=contact_raw(c.x);ez-=contact_raw(c.pp.t*physics_length(c.pp.i,c.pp.nx));
  int heading=contact_raw(-c.psi*1024/6.2831853071795864769)&1023;
  int command=original_pilot_command(ex,ez,heading,-contact_raw(c.steer_st),c.player,g_drive_trig,g_drive_trig+256);
  return -command/128.0;
}

// target speed and lane of a computer driver (Gere_Posx, Gere_Vitesse, Gere_Seq - reconstructed rules)
static int g_ai_rescue = 1;   // option: KI-Notfallhilfe (modern)
// put a stuck computer car straight onto its lane target (the same lane contract the controller uses:
// lane / 256 of the half width), on a free spot, with all contact / recovery states reset at once
static void ai_rescue(RCar &c, int idx) {
  double half = pp_halfw(c.pp), lim = car_hw(c);
  double cand[3] = {c.base_lane / 256.0 * half, 0, -c.base_lane / 256.0 * half};
  double x = cand[0];
  for (int t = 0; t < 3; t++) {
    double cx = d_clamp(cand[t], -lim, lim); int free = 1;
    for (int k = 0; k < RC.ncar; k++) {
      if (k == idx || RC.car[k].out) continue;
      double a, l; rel_to(c, RC.car[k].W, &a, &l);
      if (d_abs(a) < 15000 && d_abs(c.x + l - cx) < 10000 && same_road_at(c, RC.car[k].W, RC.car[k].pp.i, NEAR_CONTACT)) { free = 0; break; }
    }
    if (free) { x = cx; break; }
  }
  c.x = x; c.psi = 0; c.steer_st = 0; c.yaw = 0;
  c.v = c.vlong = 0.3 * V_PLAYER; c.vlat = 0;
  c.recover_t = 0; c.bad_t = 0; c.wall_cd = 0; c.avoid_t = 0; c.lane = c.base_lane;
  c.fixed_ready=0;c.stuck_t = 0; c.stuck_prog = c.prog; c.pose_gen++;   // pose_gen: the presentation must not interpolate across this jump
  car_update_world(c);
  if (sys_debug_enabled() && A.demo_time > 0) { char m[64] = ""; str_cpy(m, ui_debug_text("KI-Notfallhilfe: Wagen "), 64); str_int(m, 64, idx); sys_log(m); }
}
static void ai_decide(RCar &c, int idx, double dt) {
  RCar &P = player();
  double hw = pp_halfw(c.pp);
  // Gere_Posx: keep the own lane; avoid hazards, track objects and slower cars ahead on the same road part
  double lane = c.base_lane;
  if (c.avoid_t > 0) { c.avoid_t -= dt; lane = c.lane; }
  else {
    auto avoid_at = [&](double ox, double along) {
      if (along < 3000 || along > 7 * PT) return;
      double olane = ox / hw * 256;
      if (d_abs(olane - lane) < 90) { lane = olane > 0 ? olane - 150 : olane + 150; if (lane > 200) lane = 200; if (lane < -200) lane = -200; c.avoid_t = 0.6; }
    };
    for (int i = 0; i < 48; i++) { Hazard &h = RC.haz[i]; if (!h.alive || h.owner == idx) continue; double a, l; rel_to(c, h.W, &a, &l); if (same_road_at(c, h.W, h.pp.i, NEAR_PERCEIVE)) avoid_at(c.x + l, a); }
    for (int i = 0; i < TK.nobj; i++) { TrackObj &o = TK.obj[i]; if (!o.alive) continue; const double *w = o.W; double a, l; rel_to(c, w, &a, &l); if (o.hz < 6000 && same_road_at(c, w, o.node, NEAR_PERCEIVE)) avoid_at(c.x + l, a); }
    for (int k = 0; k < RC.ncar; k++) { if (k == idx || RC.car[k].out) continue; RCar &o = RC.car[k]; double a, l; rel_to(c, o.W, &a, &l); if (o.vlong < c.vlong - 0.05 * V_PLAYER && a < 4 * PT && same_road_at(c, o.W, o.pp.i, NEAR_PERCEIVE)) avoid_at(c.x + l, a); }
  }
  c.lane = lane; c.look = 8;
  // Gere_Vitesse: tempo against the player (rubber band); computer limit 1140
  double rel = c.prog - P.prog;   // + = ahead of the player
  double vt = V_PLAYER * (0.985 - 0.004 * idx);
  if (rel < -14) vt = V_AI;                        // far behind: up to the computer limit
  else if (rel > 25) vt = V_PLAYER * 0.93;         // far ahead: a little slower
  c.desired_v = vt;
  if (d_cos(c.psi) < 0.3) c.desired_v = vt < 0.35 * V_PLAYER ? vt : 0.35 * V_PLAYER;   // turned round: slowly, with a tight turn
  // KI-Notfallhilfe (modern safety net of the port, not from M.EXE; option "ki_notfallhilfe"): a computer car
  // whose best progress has not grown by one road point for 4 s of simulation time (wedged at a wall, turned
  // round) is set straight onto its own lane again. Not during the rolling start, while finished or out, and not
  // while it blocks on purpose (barrage).
  if (g_ai_rescue && !c.out && !c.finished && !c.autopilot && RC.phase != PH_GRID && c.tactic_active != T_BARRAGE) {
    if (c.prog > c.stuck_prog + 1.0) { c.stuck_prog = c.prog; c.stuck_t = 0; }
    else c.stuck_t += dt;
    if (c.stuck_t >= 4.0) ai_rescue(c, idx);
  } else { c.stuck_t = 0; if (c.prog > c.stuck_prog) c.stuck_prog = c.prog; }
  // recovery: pointing the wrong way for 50 steps -> reverse for 14 steps (M.EXE: 50 / 14)
  if (c.recover_t > 0) { c.recover_t -= cnt_dt(dt); c.desired_v = V_REVERSE * 0.5; c.lane = 0; }
  else if ((d_cos(c.psi) < 0.2 || c.vlong < 0.08 * V_PLAYER) && !c.out) { c.bad_t += cnt_dt(dt); if (c.bad_t > 50 * SIM_STEP - 1e-9) { c.bad_t = 0; c.recover_t = 14 * SIM_STEP; } }   // M.EXE: 50 / 14 steps
  else c.bad_t = 0;
  // Gere_Seq: special tactics of this driver slot; the lowest active bit wins (M.EXE dispatch with bsf)
  int act = 0;
  if (c.tactic_mask && !RC.final_race && !P.out) {
    double plane = P.x / pp_halfw(P.pp) * 256;
    if ((c.tactic_mask & T_POISSON) && d_abs(rel) < 10) act |= T_POISSON;
    if (c.tactic_mask & T_ZIGZAG) act |= T_ZIGZAG;
    if ((c.tactic_mask & T_ALIGN) && d_abs(rel) < 20) act |= T_ALIGN;
    if ((c.tactic_mask & T_BARRAGE) && rel > 2 && rel < 12) act |= T_BARRAGE;
    if ((c.tactic_mask & T_BOOSTER) && rel < -6) act |= T_BOOSTER;
    if ((c.tactic_mask & T_COLLE) && d_abs(rel) < 6) act |= T_COLLE;
    int bit = act & -act;
    c.tactic_active = bit;
    switch (bit) {
      case T_POISSON: case T_ALIGN: c.lane = d_clamp(plane, -200, 200); break;
      case T_ZIGZAG: c.lane = 80 * RC.zig_sign; break;
      case T_BARRAGE: c.lane = d_clamp(plane, -200, 200); c.desired_v = P.vlong > 0 ? P.vlong * 0.9 : V_PLAYER * 0.5; break;   // blocks the player's line
      case T_BOOSTER: c.desired_v = V_BOOST; break;
      case T_COLLE: c.lane = d_clamp(plane, -200, 200); c.desired_v = P.vlong; break;
    }
  } else c.tactic_active = 0;
  // weapons: front at a car ahead on the same lane, rear at a car close behind (throttled by a global counter)
  if (!RC.final_race && !c.out) {
    c.ai_fire_t -= dt;
    if (c.ai_fire_t <= 0 && ((RC.fire_counter++ & 63) % 8) == idx % 8) {
      c.ai_fire_t = frnd(1.5, 4.0);
      for (int k = 0; k < RC.ncar; k++) {
        if (k == idx || RC.car[k].out) continue;
        double a, l; rel_to(c, RC.car[k].W, &a, &l); double dl = (c.x + l) - c.x;
        if (!same_road_at(c, RC.car[k].W, RC.car[k].pp.i, NEAR_PERCEIVE)) continue;
        if (a > 2 * PT && a < 14 * PT && d_abs(dl) < 9000 && d_cos(c.psi) > 0.9) { fire_front(c, idx); break; }
        if (a < -1 * PT && a > -6 * PT && d_abs(dl) < 12000) { drop_rear(c, idx); break; }
      }
    }
  }
}
// drive command of the controller: gas below the target speed, brake above (no coasting)
static int pilot_drive(const RCar &c) { return c.v >= c.desired_v ? -1 : 1; }

// ------------------------------------------------------------------ camera and video frame
// Driving the wrong way: the frames after the last road point are a flight over the track that looks
// backwards (as in the original, which uses them for this). There are only a few of them (one every
// ~40 road points), so the view is a fixed camera that the car drives away from until the next one.
static int choose_reverse_frame(void) {
  RCar &P = player();
  int best = -1; double bd = 1e300;
  for (int f = TK.nroad; f < TK.vid.nframes && f < TK.ncam; f++) {
    const double *c = TK.cam + f * 12;
    double d[3] = {P.W[0] - c[0], P.W[1] - c[1], P.W[2] - c[2]};
    double r[3][3];
    for (int k = 0; k < 3; k++) { double l = d_sqrt(c[3 + k * 3] * c[3 + k * 3] + c[4 + k * 3] * c[4 + k * 3] + c[5 + k * 3] * c[5 + k * 3]); for (int q = 0; q < 3; q++) r[k][q] = l > 0 ? c[3 + k * 3 + q] / l : 0; }
    double ex = r[0][0] * d[0] + r[0][1] * d[1] + r[0][2] * d[2], ey = r[1][0] * d[0] + r[1][1] * d[1] + r[1][2] * d[2], ez = r[2][0] * d[0] + r[2][1] * d[1] + r[2][2] * d[2];
    if (ez < 4 * PT || ez > 70 * PT) continue;                 // in front of the camera, not too far
    if (d_abs(ex / ez) > 0.33 || d_abs(ey / ez) > 0.4) continue;   // inside the picture
    // the camera must look against the driving direction of the road at the car
    double dot = r[2][0] * P.F[0] + r[2][1] * P.F[1] + r[2][2] * P.F[2];
    if (dot > -0.3) continue;
    if (ez < bd) { bd = ez; best = f; }
  }
  return best;
}
static void choose_frame(void) {
  RCar &P = player();
  {   // wrong way: camera from the backwards fly-over
    double cp = d_cos(P.psi);
    if (RC.reverse_view ? (cp < 0.1) : (cp < -0.4 && P.v > 0.05 * P.vmax)) {
      int f = choose_reverse_frame();
      if (f >= 0) {
        RC.reverse_view = 1; RC.frame = f;
        for (int k = 0; k < 12; k++) RC.vcam[k] = TK.cam[f * 12 + k];
        for (int r = 0; r < 3; r++) { double *v = RC.vcam + 3 + r * 3; double l = d_sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) for (int k = 0; k < 3; k++) v[k] /= l; }
        for (int q = 0; q < 3; q++) g_shift[q] = 0;
        return;
      }
    }
    RC.reverse_view = 0;
  }
  double back = RC.ply_D - RC.cam_D;
  int hidx; PPos cq = cam_from_back(back, &hidx);
  RC.campp = cq;
  double pc[3]; pp_world(cq, 0, 0, pc);
  // frame k belongs to road point k; its camera is some points behind k. Among the points of the player's
  // path ahead of the camera, take the frame whose camera is closest to our camera position.
  int best = -1; double bd = 1e300;
  int k = hidx; int pt = cq.i; int steps = 0;
  while (steps++ < 28) {
    if (pt >= 0 && pt < TK.nframes && TK.fok[pt]) {
      PPos fq; fq.i = TK.fpt[pt]; fq.nx = TK.fnx[pt]; fq.t = TK.ft[pt];
      double pf[3]; pp_world(fq, 0, 0, pf);
      double dx = pf[0] - pc[0], dy = pf[1] - pc[1], dz = pf[2] - pc[2], d2 = dx * dx + dy * dy + dz * dz;
      if (d2 < bd) { bd = d2; best = pt; }
    }
    // next point along the player's path
    if (k + 1 < RC.hn) { k++; pt = RC.hist[k % HIST]; }
    else if (pt == P.pp.i) pt = P.pp.nx;
    else pt = RP(pt).nx[0];
  }
  if (best < 0) best = RC.frame;
  RC.frame = best;
  // The video can only show whole frames. To keep cars from jerking (especially in curves, where the
  // camera turns from frame to frame) everything 3D is drawn with a camera interpolated between this
  // frame and its neighbour at the exact camera position.
  auto fpos = [&](int f, double out[3]) { PPos fq; fq.i = TK.fpt[f]; fq.nx = TK.fnx[f]; fq.t = TK.ft[f]; pp_world(fq, 0, 0, out); };
  double p0[3]; fpos(best, p0);
  int f0 = best, f1 = best; double t = 0;
  for (int dir = 1; dir >= -1; dir -= 2) {
    int fn = best + dir; if (fn < 0 || fn >= TK.nframes || TK.frun[fn] != TK.frun[best] || !TK.fok[fn]) continue;
    double p1[3]; fpos(fn, p1);
    double d[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]}, L2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    if (L2 < 1) continue;
    double u = ((pc[0] - p0[0]) * d[0] + (pc[1] - p0[1]) * d[1] + (pc[2] - p0[2]) * d[2]) / L2;
    if (u > 0) { f0 = best; f1 = fn; t = d_clamp(u, 0, 1); break; }
  }
  // Catmull-Rom through the neighbouring frame cameras: a straight interpolation has a kink at every
  // frame, which shows as a slight shaking of the cars in curves
  if (f1 < f0) { int x = f0; f0 = f1; f1 = x; t = 1 - t; }
  auto okf = [&](int f) { return f >= 0 && f < TK.nframes && TK.fok[f] && TK.frun[f] == TK.frun[best]; };
  int fa = okf(f0 - 1) ? f0 - 1 : f0, fb = okf(f1 + 1) ? f1 + 1 : f1;
  const double *a = TK.cam + f0 * 12, *b = TK.cam + f1 * 12, *a0 = TK.cam + fa * 12, *b1 = TK.cam + fb * 12;
  double t2 = t * t, t3 = t2 * t;
  for (int k = 0; k < 12; k++) {
    if (f0 == f1 || A.camoff) { RC.vcam[k] = a[k] + (b[k] - a[k]) * t; continue; }
    double m0 = 0.5 * (b[k] - a0[k]) * (fa == f0 ? 2 : 1), m1 = 0.5 * (b1[k] - a[k]) * (fb == f1 ? 2 : 1);
    RC.vcam[k] = (2 * t3 - 3 * t2 + 1) * a[k] + (t3 - 2 * t2 + t) * m0 + (-2 * t3 + 3 * t2) * b[k] + (t3 - t2) * m1;
  }
  // The orientation is always the one of the video frame on screen: then cars and background turn
  // together in curves and on hills (an interpolated or warped orientation made either the cars or
  // the picture shake). Only the position is interpolated, so the cars do not pump in size.
  if (!g_cam_rot_interp) for (int k = 3; k < 12; k++) RC.vcam[k] = TK.cam[best * 12 + k];
  for (int r = 0; r < 3; r++) { double *v = RC.vcam + 3 + r * 3; double l = d_sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) for (int k = 0; k < 3; k++) v[k] /= l; }
  for (int q = 0; q < 3; q++) g_shift[q] = 0;
}

// ------------------------------------------------------------------ original camera controller (reference path)
// Reconstructed from M.EXE (debug EXE 0x2A7AF-0x2AB6B). Two modes:
//  FIX: the picture is a fixed key camera: the forward anchor (PST +0x08) of the node up to four links ahead of
//       the player (stopping at a fork), or the reverse anchor (PST +0x0A, fly-over frames) when the car moves
//       backwards. Time step 2048 units (40 ms).
//  MOB: started on a key node (PST +0x00 bit 15) at raw speed >= 345: the camera frame starts at the player's
//       node and moves exactly one node (= one video frame) per update; the update interval is
//       0x1B0000 / cameraSpeed units, the camera speed follows the player with a P/D regulator (limits 172..1382,
//       leaves MOB below 414 or 12 nodes away).
// The 3D camera of a frame is always CAM[frame] itself: picture, camera and visibility belong together.
// player speed for the camera (M.EXE 0x33EE8), taken as the motion along the road: a car driving the wrong
// way must not start the forward-moving MOB camera
static int cam_raw_speed(void) { return (int)(player().vlong / V_RAW); }
// links from node a forward to node b (-1: not within max links)
static int graph_links(int a, int b, int maxn) {
  int q[64], dq[64], qn = 0, qh = 0; q[qn] = a; dq[qn++] = 0;
  int seen[64], ns = 0;
  while (qh < qn) {
    int n = q[qh], d = dq[qh++];
    if (n == b) return d;
    if (d >= maxn) continue;
    const RoadPt &r = RP(n);
    for (int k = 0; k < 2; k++) {
      int m = r.nx[k]; if (k == 1 && m == r.nx[0]) continue;
      int dup = 0; for (int j = 0; j < ns; j++) if (seen[j] == m) dup = 1;
      if (dup || qn >= 64 || ns >= 64) continue;
      seen[ns++] = m; q[qn] = m; dq[qn++] = d + 1;
    }
  }
  return -1;
}
// distance from the camera frame's node to the player in raw units (0x1B00 per link, as M.EXE 0x2A821)
static long cam_player_dist(void) {
  RCar &P = player();
  long local = (long)(P.pp.t * seg_len(P.pp.i, P.pp.nx));
  int k = graph_links(RC.cam_frame, P.pp.i, 13);
  if (k >= 0) return k * 0x1b00L + local;
  k = graph_links(P.pp.i, RC.cam_frame, 13);
  if (k >= 0) return -k * 0x1b00L + local;
  return 0x7fffffffL;
}
static int cam_regulate(void) {   // 0x2A7E9: returns 0 when the camera leaves MOB
  long long dist = cam_player_dist();
  if (dist >= 0x14400 || dist <= -0x14400) { RC.cam_mob = 0; return 0; }
  long long b = (dist * 0x28) >> 16;
  long long a = ((long long)(cam_raw_speed() - RC.cam_speed) * 0x1194) >> 16;
  a += b; if (a > 0x400) a = 0x400; if (a < -0x400) a = -0x400;
  a += RC.cam_speed; if (a > 0x566) a = 0x566; if (a < 0xac) a = 0xac;
  RC.cam_speed = (int)a;
  if (a < 0x19e) { RC.cam_mob = 0; return 0; }
  return 1;
}
static int cam_try_mob(void) {     // 0x2A8B6
  RCar &P = player();
  if (cam_raw_speed() < 0x159 || !RP(P.pp.i).mobflag) return 0;
  RC.cam_frame = P.pp.i; RC.cam_speed = cam_raw_speed(); RC.cam_mob = 1; RC.cam_just = 1;
  return 1;
}
static void cam_mob_frame(void) {  // 0x2A90B: one node per update along the links +0x5C / +0x70
  if (RC.cam_just) { RC.cam_just = 0; RC.cam_cnt = 0; return; }
  const RoadPt &r = RP(RC.cam_frame);
  int nx = r.nx[0];
  if (r.kind == 5 && r.nx[1] != r.nx[0]) {   // fork: the branch the player has taken (the original uses the player's side)
    int took = -1;
    for (int k = RC.hn - 1; k >= 1 && k >= RC.hn - 40; k--) if (RC.hist[(k - 1) % HIST] == RC.cam_frame) { took = RC.hist[k % HIST]; break; }
    nx = (took == r.nx[0] || took == r.nx[1]) ? took : pick_next(RC.cam_frame, player().W);
  }
  RC.cam_frame = nx; RC.cam_cnt = 0;
}
static void cam_fix_frame(void) {  // 0x2AA1D
  RCar &P = player();
  int node = P.pp.i;
  int sv = (int)(P.vlong / V_RAW);   // motion along the road (raw speed units)
  int rev;
  if ((RC.cam_cnt != 0 && sv <= 0x22) || sv <= -207) {
    if (RC.cam_cnt > 0) { RC.cam_cnt--; rev = 0; } else { RC.cam_cnt = -20; rev = 1; }
  } else if (RC.cam_cnt < 0) { RC.cam_cnt++; rev = 1; }
  else rev = 0;
  if (rev) {
    int a = RP(node).anc_r;
    if (a != 0xffff && a < TK.ncam) { RC.cam_frame = a; RC.cam_just = 0; return; }
  }
  int n = node;
  for (int k = 0; k < 4; k++) { if (RP(n).kind == 5) break; n = RP(n).nx[0]; }
  int a = RP(n).anc_f & 0xffff;
  RC.cam_frame = a < TK.ncam ? a : node; RC.cam_just = 0; RC.cam_cnt = 0;
}
// one camera step: frame of the next package and the length of the next interval
// Two phases per update, in the order of the reference: first mode / camera speed and the time step
// (_CALCUL_TEMPS_CAMERA), then the simulation with this step, then the camera frame (_CALCUL_FRAME_CAMERA).
static void cam_time_phase(void) {
  if (RC.cam_mob) cam_regulate();
  if (!RC.cam_mob) cam_try_mob();
  RC.delta_units = RC.cam_mob ? 0x1b0000 / (RC.cam_speed > 0 ? RC.cam_speed : 1) : 0x800;
}
static void cam_frame_phase(void) { if (RC.cam_mob) cam_mob_frame(); else cam_fix_frame(); }
static void cam_step(void) { cam_time_phase(); cam_frame_phase(); }
// ---- poses of two consecutive packages (for the debug output modes 60 FPS: the 3D picture is
// interpolated between the previous and the latest update; the Original mode shows the packages as they are)
struct PoseSnap { int valid, frame; double cam[12]; double pos[8][3], R[8][3], U[8][3], F[8][3], wheel[8]; int cgen[8];
                 double ppos[32][3], pR[32][3], pU[32][3], pF[32][3]; int palive[32], pgen[32];
                 double opos[MAXOBJ][3], oR[MAXOBJ][3], oU[MAXOBJ][3], oF[MAXOBJ][3]; int oalive[MAXOBJ]; };
static PoseSnap g_snap[2];   // 0 previous, 1 latest
static void pose_of(const PPos &q, double x, double h, double psi, double pos[3], double R[3], double U[3], double F[3]) {
  double p[3], rt[3], up[3], fw[3], w; pp_frame(q, p, rt, up, fw, &w);
  double cs = d_cos(psi), sn = d_sin(psi);
  for (int k = 0; k < 3; k++) { F[k] = fw[k] * cs + rt[k] * sn; R[k] = rt[k] * cs - fw[k] * sn; U[k] = up[k]; pos[k] = p[k] + rt[k] * x + up[k] * h; }
}
// drawing pose of a road object: its own PST frame, turned (yaw) and tumbling (pitch) about the middle of the model
static void obj_pose(const TrackObj &o, double pos[3], double R[3], double U[3], double F[3]) {
  const RoadPt &q = RP(o.node);
  int kind = o.type == 1 ? OM_OBJ1 : o.type == 2 ? OM_BARREL : OM_CONE;
  const Model *m = TK.obj_models[kind];
  double cy = d_cos(o.yaw), sy = d_sin(o.yaw), cp = d_cos(o.pitch), sp = d_sin(o.pitch), F0[3];
  for (int k = 0; k < 3; k++) { F0[k] = q.fw[k] * cy + q.rt[k] * sy; R[k] = q.rt[k] * cy - q.fw[k] * sy; }
  for (int k = 0; k < 3; k++) { F[k] = F0[k] * cp + q.up[k] * sp; U[k] = q.up[k] * cp - F0[k] * sp; }
  double lift = m ? (m->bmin.y + m->bmax.y) * 0.5 / MODEL_SCALE : 0;   // middle of the model (bounding box)
  for (int k = 0; k < 3; k++) pos[k] = o.W[k] + q.up[k] * lift - U[k] * lift;
}
static void snap_take(void) {
  g_snap[0] = g_snap[1];
  PoseSnap &n = g_snap[1];
  n.valid = 1; n.frame = RC.frame; for (int k = 0; k < 12; k++) n.cam[k] = RC.vcam[k];
  for (int i = 0; i < RC.ncar && i < 8; i++) { const RCar &c = RC.car[i]; pose_of(c.pp, c.x, 0, c.psi, n.pos[i], n.R[i], n.U[i], n.F[i]); n.cgen[i] = c.pose_gen; n.wheel[i] = c.wheel; }
  for (int i = 0; i < 32; i++) { const Proj &q = RC.proj[i]; n.palive[i] = q.alive; n.pgen[i] = q.gen; if (q.alive) pose_of(q.pp, q.x, 0, q.psi, n.ppos[i], n.pR[i], n.pU[i], n.pF[i]); }
  for (int i = 0; i < TK.nobj && i < MAXOBJ; i++) { const TrackObj &o = TK.obj[i]; n.oalive[i] = o.alive; if (o.alive) obj_pose(o, n.opos[i], n.oR[i], n.oU[i], n.oF[i]); }
  if (!g_snap[0].valid) g_snap[0] = n;
}
static void lerp3(const double a[3], const double b[3], double t, double o[3], int norm) {
  for (int k = 0; k < 3; k++) o[k] = a[k] + (b[k] - a[k]) * t;
  if (norm) { double l = d_sqrt(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]); if (l > 1e-9) for (int k = 0; k < 3; k++) o[k] /= l; }
}
static double car_render_wheel(int index,double alpha){
  if(alpha<0||!g_snap[0].valid||!g_snap[1].valid||g_snap[0].cgen[index]!=g_snap[1].cgen[index])return RC.car[index].wheel;
  return g_snap[0].wheel[index]+(g_snap[1].wheel[index]-g_snap[0].wheel[index])*alpha;
}
static void choose_frame(void);
// publish the presentation package of this update: frame, its own camera, visibility, lighting
static void package_publish(void) {
  if (g_cam_rot_interp) choose_frame();   // debug: the old smoothed camera (not original)
  else {
    int f = RC.cam_frame; if (f < 0) f = 0; if (f >= TK.ncam) f = TK.ncam - 1;
    if (f >= TK.vid.nframes) f = TK.vid.nframes - 1;
    RC.frame = f;
    for (int k = 0; k < 12; k++) RC.vcam[k] = TK.cam[f * 12 + k];
    for (int r = 0; r < 3; r++) { double *v = RC.vcam + 3 + r * 3; double l = d_sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) for (int k = 0; k < 3; k++) v[k] /= l; }
    for (int q = 0; q < 3; q++) g_shift[q] = 0;
  }
  RC.reverse_view = RC.frame >= TK.nroad;
  RC.pkg_ubb = TK.cam_ubb && RC.frame < TK.ncam ? (int)TK.cam_ubb[RC.frame] : 0;
  video_decode(&TK.vid, RC.frame);
  {  // lighting of the cars follows the brightness of the scenery (once per package)
    long sum = 0; int cnt = 0;
    for (int y = TK.vid.h / 3; y < TK.vid.h; y += 9) for (int x = 0; x < TK.vid.w; x += 9) { const uint8_t *c = TK.vid.rgba + (y * TK.vid.w + x) * 4; sum += c[0] * 3 + c[1] * 6 + c[2]; cnt++; }
    float lum = cnt ? sum / (cnt * 10.0f * 255.0f) : 0.3f;
    float target = m_clamp(0.5f + lum * 1.2f, 0.55f, 1.1f);
    RC.gain += (target - RC.gain) * 0.25f;
  }
  RC.pkg_id++; RC.pkg_repeat = 0;
  snap_take();
}
static void race_update(const Input *in, double dt);
static void rec_capture(void);
// ---- cadence: the virtual 200 Hz clock decides when the next original update is due
static int g_out_mode;                 // output: 0 Original (one picture per update), 1 60 FPS
static double g_upd_rate, g_upd_acc; static int g_upd_cnt;
static void race_cam_init(void) {
  g_race_tex_frame = -1;   // new race (maybe another track): the cached track pictures are invalid
  RC.cam_mob = 0; RC.cam_cnt = 0; RC.cam_just = 0; RC.cam_frame = player().pp.i; RC.cam_speed = cam_raw_speed();
  cam_step();
  RC.timer3d = 0; RC.cam_old = 0; RC.clock_ms = 0;
  g_snap[0].valid = g_snap[1].valid = 0;
  package_publish();
}
// one original update: simulation over the elapsed interval, then the camera, then the new package
static void original_update(const Input *in) {
  cam_time_phase();                       // mode, camera speed and the time step of this update
  double dt = RC.delta_units / 51200.0;
  race_update(in,dt);
  cam_frame_phase();                      // the camera frame after the movement
  package_publish();
  RC.upd_n++; g_upd_cnt++;
  if (sys_debug_enabled() && A.trace) {   // canonical state per original update (for comparisons with original traces; --trace)
    static uint8_t *tbd; static size_t tbn, tbc; static int head;
    auto tput = [&](const char *x) { size_t k = str_len(x); if (tbn + k > tbc) { tbc = (tbn + k) * 2 + 65536; tbd = (uint8_t *)sys_realloc(tbd, tbc); } mem_cpy(tbd + tbn, x, k); tbn += k; };
    char l[512];
    if (!head) { head = 1; tput("upd;t_ms;delta;mob;camspeed;frame;rng;car;node;t_seg_e4;x;psi_e4;v_e2;vlong_e2;vlat_e2;steer;lane;desired_v;tactic;recover_ms;prog_e3\n"); }
    for (int i = 0; i < RC.ncar; i++) { const RCar &c = RC.car[i]; l[0] = 0;
      auto num = [&](double v, int sc) { str_int(l, 512, (long)(v * sc)); str_cat(l, ";", 512); };
      num(RC.upd_n, 1); num(RC.t, 1000); num(RC.delta_units, 1); num(RC.cam_mob, 1); num(RC.cam_speed, 1); num(RC.frame, 1); num(g_rng, 1); num(i, 1);
      num(c.pp.i, 1); num(c.pp.t, 10000); num(c.x, 1); num(c.psi, 10000); num(c.v / V_RAW, 100); num(c.vlong / V_RAW, 100); num(c.vlat / V_RAW, 100);
      num(c.steer_st, 1); num(c.lane, 1); num(c.desired_v / V_RAW, 1); num(c.tactic_active, 1); num(c.recover_t, 1000); str_int(l, 512, (long)(c.prog * 1000)); str_cat(l, "\n", 512);
      tput(l); }
    if (RC.upd_n % 25 == 0 || RC.phase == PH_DONE) { char pth[512]; cfg_path(pth, "openmr2_trace.csv"); sys_write_file(pth, tbd, tbn); }
  }
  if (sys_debug_enabled() && A.demo_time > 0 && RC.upd_n % 50 == 0) {   // test: state of the simulation per update number (output rate independent)
    char m[256] = "upd "; str_int(m, 256, RC.upd_n); str_cat(m, " frame ", 256); str_int(m, 256, RC.frame); str_cat(m, " du ", 256); str_int(m, 256, RC.delta_units);
    for (int i = 0; i < RC.ncar; i++) { str_cat(m, " ", 256); str_int(m, 256, (long)(RC.car[i].prog * 1000)); }
    sys_log(m);
  }
  rec_capture();
}
// advance the race by real time; returns the number of updates done
static int race_advance(const Input *in, double real_dt) {
  RC.clock_ms += real_dt * 1000.0;
  RC.timer3d = (long long)(RC.clock_ms / 5.0) * 256;
  g_upd_acc += real_dt; if (g_upd_acc >= 0.5) { g_upd_rate = g_upd_cnt / g_upd_acc; g_upd_acc = 0; g_upd_cnt = 0; }
  int done = 0;
  static Input pend; static int pend_ok;
  if (!pend_ok) { mem_zero(&pend, sizeof pend); pend_ok = 1; }
  // key presses are kept until the next update uses them
  for (int k = 0; k < K_COUNT; k++) if (in->pressed[k]) pend.pressed[k] = 1;
  while (RC.timer3d >= RC.cam_old + RC.delta_units) {
    long long target = RC.cam_old + RC.delta_units;
    Input use = *in; for (int k = 0; k < K_COUNT; k++) use.pressed[k] = pend.pressed[k];
    mem_zero(pend.pressed, sizeof pend.pressed);
    original_update(&use);
    // ATTENTE_AFFICHAGE: the wait ends on the first 5 ms tick at or after the target; that tick is the new base
    long long tick = (target + 255) / 256 * 256;
    if (RC.timer3d - tick > 256 * 20) { RC.late_n++; RC.cam_old = RC.timer3d; }   // late (load): resynchronise
    else RC.cam_old = tick;
    if (++done >= 8) { RC.cam_old = RC.timer3d; RC.late_n++; break; }
    if (!RC.active) break;
  }
  return done;
}
// seconds until the next update is due (for the Original output mode)
static double race_next_due(void) {
  long long target = RC.cam_old + RC.delta_units;
  double due_ms = (double)((target + 255) / 256) * 5.0;
  return (due_ms - RC.clock_ms) / 1000.0;
}

// ------------------------------------------------------------------ physics of pushed / thrown road objects
// All values are the port's own tuning (units: world units, seconds, radians). They are NOT measured in the
// original yet (open point: calibrate against recordings of the original).
struct ObjPhys {
  double gravity;            // fall acceleration of a thrown cone (units/s^2)
  double fly_nohit;          // above this height a flying object hits nothing
  double close_min;          // closing speed below which a contact only pushes the object aside (units/s)
  double cone_up_min, cone_up_max, cone_lon_min, cone_lon_max, cone_side_min, cone_side_max, cone_spin, cone_tumble;
  double cone_friction, cone_brake, cone_cd, cone_land_keep, cone_land_spin;
  double barrel_lon, barrel_lon_rnd, barrel_side_min, barrel_side_max, barrel_spin, barrel_friction, barrel_brake, barrel_damage, barrel_cd;
  double spin_damp, spin_stop;
};
static const ObjPhys OBJ_PHYS = {
  60000.0, 6000.0, 3000.0,
  16000.0, 26000.0, 0.7, 1.1, 3000.0, 9000.0, 9.0, 12.0,
  40000.0, 0.97, 0.3, 0.35, 0.3,
  0.6, 1500.0, 9000.0, 16000.0, 3.0, 260000.0, 0.6, 0.6, 0.6,
  4.0, 0.05,
};
static void obj_sync_pp(TrackObj &o) {   // road position used by the game logic, from the object's own frame
  double L = seg_len(o.node, RP(o.node).nx[0]);
  o.pp = pp_make(o.node, d_clamp((o.lon + o.dlon) / L, 0, 1)); o.x = o.lat + o.dlat;
}
// object moved past the end / start of its segment: continue in the next node's frame (branch by position).
// The whole position (including the height) and the whole velocity are transformed, so a flying object keeps
// its world position; only an object lying on the road is put onto the new road surface (contact with ground).
static void obj_change_frame(TrackObj &o) {
  for (int g = 0; g < 8; g++) {
    const RoadPt &cur = RP(o.node);
    double along = o.lon + o.dlon;
    int nn = -1;
    if (along > seg_len(o.node, cur.nx[0])) nn = pick_next(o.node, o.W);
    else if (along < 0) nn = pick_prev(o.node, o.W);
    if (nn < 0) return;
    if (nn >= TK.nroad) return;
    const RoadPt &nq = RP(nn);
    int flying = o.hz > 0 || o.vz > 0;
    double vw[3]; for (int k = 0; k < 3; k++) vw[k] = cur.fw[k] * o.vlon + cur.rt[k] * o.vlat + (flying ? cur.up[k] * o.vz : 0);
    double d[3] = {o.W[0] - nq.p[0], o.W[1] - nq.p[1], o.W[2] - nq.p[2]};
    double lon = d[0] * nq.fw[0] + d[1] * nq.fw[1] + d[2] * nq.fw[2];
    double lat = d[0] * nq.rt[0] + d[1] * nq.rt[1] + d[2] * nq.rt[2];
    double hgt = d[0] * nq.up[0] + d[1] * nq.up[1] + d[2] * nq.up[2];
    o.node = nn; o.lon = 0; o.lat = 0; o.dlon = lon; o.dlat = lat;   // fractions kept in the double offsets
    if (flying) { o.hz = hgt - o.h; if (o.hz < 0) o.hz = 0; o.vz = vw[0] * nq.up[0] + vw[1] * nq.up[1] + vw[2] * nq.up[2]; }
    else o.hz = 0;   // lying on the road: put onto the new road surface
    o.vlon = vw[0] * nq.fw[0] + vw[1] * nq.fw[1] + vw[2] * nq.fw[2]; o.vlat = vw[0] * nq.rt[0] + vw[1] * nq.rt[1] + vw[2] * nq.rt[2];
    obj_update_world(o);
  }
  sys_log("road object: more than 8 segment changes in one step (stopped)");
  o.vlon = o.vlat = 0;
}
static void obj_move(TrackObj &o, double dt) {
  const ObjPhys &P = OBJ_PHYS;
  o.dlon += o.vlon * dt; o.dlat += o.vlat * dt; o.yaw += o.yawr * dt;
  int landed = 0;
  if (o.hz > 0 || o.vz > 0) {   // in the air
    o.vz -= P.gravity * dt; o.hz += o.vz * dt; o.pitch += o.pitchr * dt;
    if (o.hz <= 0) { o.hz = 0; o.vz = 0; o.pitch = 0; o.pitchr = 0; o.vlon *= P.cone_land_keep; o.vlat *= P.cone_land_keep; o.yawr *= P.cone_land_spin; landed = 1; }
  } else {   // sliding on the road
    double fr = (o.type == 8 ? P.cone_friction : P.barrel_friction) * dt, sp = d_sqrt(o.vlon * o.vlon + o.vlat * o.vlat);
    if (sp <= fr) { o.vlon = o.vlat = 0; } else { o.vlon -= o.vlon / sp * fr; o.vlat -= o.vlat / sp * fr; }
    o.yawr *= 1 - d_clamp(P.spin_damp * dt, 0, 1);
    if (d_abs(o.yawr) < P.spin_stop) o.yawr = 0;
  }
  // the road boundary stops it
  double lim = RP(o.node).w * 1.1;
  if (o.lat + o.dlat > lim) { o.dlat = lim - o.lat; o.vlat = -o.vlat * 0.3; }
  if (o.lat + o.dlat < -lim) { o.dlat = -lim - o.lat; o.vlat = -o.vlat * 0.3; }
  obj_update_world(o);
  obj_change_frame(o);
  obj_sync_pp(o);
  if (o.vlon == 0 && o.vlat == 0 && o.hz <= 0 && o.vz <= 0 && o.yawr == 0) o.moving = 0;   // also the turning has stopped
  if (landed && o.type == 8) sfx_at(S_PLOT, o.W, 0.35f);   // at the landing point (after the position update)
}

#include "g_projectiles.inc"
static void race_update(const Input *in, double dt) {
  road_plane_frames_build();g_original_defer=1;
  RC.t += dt; RC.phase_t += dt;
  if (RC.msg_t > 0) RC.msg_t -= dt;
  RCar &P = player();
  // global zigzag counter (one for all drivers, as in the original): 20 steps, then the sign changes
  RC.zig_t += cnt_dt(dt); if (RC.zig_t >= 20 * SIM_STEP - 1e-9) { RC.zig_t = 0; RC.zig_sign = -RC.zig_sign; }
  double thr = 0, brk = 0, steer = 0;
  if (!P.autopilot && RC.phase != PH_DONE && !P.finished && !P.out) {
    thr = in->down[K_UP] ? 1 : 0; brk = in->down[K_DOWN] ? 1 : 0;
    steer = (in->down[K_RIGHT] ? 1 : 0) - (in->down[K_LEFT] ? 1 : 0);
    if (g_pad_analog) {   // controller: analog steering, gas and brake
      if (g_pad_steer > 0.05f || g_pad_steer < -0.05f) steer = d_clamp(steer + g_pad_steer, -1, 1);
      if (g_pad_gas > thr) thr = g_pad_gas;
      if (g_pad_brake > brk) brk = g_pad_brake;
    }
    if (in->pressed[K_SPACE]) fire_front(P, 0);
    if (in->pressed[K_ENTER]) drop_rear(P, 0);
    if (in->pressed[K_TAB]) P.lo.fsel ^= 1;
    if (in->pressed[K_SLASH]) P.lo.rsel ^= 1;
  }
  if (RC.phase == PH_FLYBY) { RC.phase = PH_GRID; RC.phase_t = 0; RC.t = 0; }
  for (int i = 0; i < RC.ncar; i++) {
    RCar &c = RC.car[i];
    // rolling start: each car drives on autopilot until it has crossed the start line itself (M.EXE)
    if (c.autopilot && c.prog >= 0) {
      c.autopilot = 0;
      if (c.player) { RC.phase = PH_RACE; msg_show("START !!!", 1.4, rgba(255, 255, 255, 255)); }
    }
    if (A.start_frame > 0 && RC.phase == PH_GRID && RC.phase_t > 3.0) { RC.phase = PH_RACE; RC.msg_t = 0; }
    double st = 0; int drive = 0; double lim = c.player ? V_PLAYER : (c.tactic_active == T_BOOSTER ? V_BOOST : V_AI);
    if (c.finished) { c.desired_v = c.vmax * 0.5; c.lane = c.base_lane; st = pilot_steer(c); drive = pilot_drive(c); }
    else if (c.autopilot) { c.desired_v = V_START; st = pilot_steer(c); drive = pilot_drive(c); }   // keeps lane +-128 and speed 518
    else if (c.player && g_test_wrongway == 1) { { static int once; if (!once) { once = 1; c.psi = 3.0; } } drive = 1; st = 0; }
    else if (c.player && g_test_wrongway == 2) { drive = 1; st = 0; c.psi = 0; c.x = car_hw(c); }   // test: at the right road edge
    else if (!c.player || A.demo_time > 0) {
      ai_decide(c, i, dt); st = pilot_steer(c); drive = pilot_drive(c); if (c.player) lim = V_PLAYER;
    }
    else { st = steer; drive = thr > 0.05 ? 1 : brk > 0.05 ? -1 : 0; if (thr > 0.05 && thr < 0.95) lim = V_PLAYER * thr; }
    if (A.objtest == 2 && c.player) { drive = 1; st = 0; c.autopilot = 0; }   // --objtest-drive: straight ahead into the objects
    if (c.out) { drive = 0; st = 0; }
    car_physics(c, drive, st, dt, lim);

  }
  g_original_defer=0;original_contacts(dt);
  for(int i=0;i<RC.ncar;i++){RCar &c=RC.car[i];
    if (!c.finished && !c.out && RC.phase != PH_GRID && c.prog >= RC.laps * (double)TK.L) {
      c.finished = 1; c.finish_t = RC.t;
      if (c.player) { char m[64] = ""; str_cpy(m, ui_translate("Ende des Rennens\nPos : "), 64); int pl = 1; for (int j = 0; j < RC.ncar; j++) if (j != i && RC.car[j].finished) pl++; str_int(m, 64, pl); msg_show(m, 3, rgba(255, 255, 255, 255)); RC.phase = PH_DONE; RC.done_t = 0; }
    }
  }
  {  // lap messages, wrong way
    int lap = (int)(P.prog / TK.L) + 1;
    if (RC.t < 0.05) RC.last_lap = lap;
    if (lap != RC.last_lap && RC.phase == PH_RACE && lap >= 2 && lap <= RC.laps && lap > RC.last_lap) {
      if (lap == RC.laps) msg_show("LETZTE RUNDE!!!", 2.5, rgba(255, 255, 255, 255));   // the original has no message for the other laps
    }
    if (lap > RC.last_lap) RC.last_lap = lap;
    if (RC.phase == PH_RACE && P.vlong < -0.1 * V_PLAYER) RC.wrong_t += dt; else RC.wrong_t = 0;
    if (RC.wrong_t > 1.0 && RC.msg_t <= 0) msg_show("FALSCHE FAHRTRICHTUNG!!!", 1.0, rgba(255, 255, 255, 255));
  }
  for(int i=0;i<32;i++)projectile_step(RC.proj[i],dt);
  // mines (start with 3/4 of the car's motion, damped by 0xDC00/65536 per step) and oil (lies still)
  for (int i = 0; i < 48; i++) {
    Hazard &h = RC.haz[i]; if (!h.alive) continue;
    h.life -= dt; h.arm -= dt; if (h.life <= 0) { h.alive = 0; continue; }
    if (h.kind == W_MINE && (d_abs(h.vlong) > 1 || d_abs(h.vlat) > 1)) {
      double dmp = d_pow(0.859375, dt / SIM_STEP); h.vlong *= dmp; h.vlat *= dmp;
      pp_move(h.pp, h.x, h.vlong * dt); h.x += h.vlat * dt; pp_world(h.pp, h.x, 0, h.W);
    }
    if (h.arm > 0) continue;
    for (int k = 0; k < RC.ncar; k++) {
      RCar &c = RC.car[k]; if (c.out) continue;
      double a, l; rel_to(c, h.W, &a, &l);
      if ((h.kind == W_MINE ? (d_abs(a) < 11000 && d_abs(l) < 8500) : (d_abs(a) < 13000 && d_abs(l) < 13000)) && same_road_at(c, h.W, h.pp.i, NEAR_CONTACT)) {
        if (h.kind == W_MINE) {
          h.alive = 0; damage_car(c, WEAPONS[W_MINE].damage, h.owner); c.vlong *= 0.4; c.v *= 0.4; c.stun = 0.8; c.psi += frnd(-0.6, 0.6);
          fx_add(OM_EXP_BIG, h.pp, h.x, 2500, 1.3); sfx_at(S_EXPLO3, h.W, 1.0f);
        } else if (!(h.hit_mask & (1 << k))) {   // oil: 30 steps of sliding (M.EXE), once per car
          h.hit_mask |= 1 << k; c.oil_t = 30 * SIM_STEP; if (c.steer_st == 0) c.steer_st = irnd(2) ? 4000 : -4000;
          if (c.player) sfx(S_DERAPOIL, 0.9f);
        }
        if (!h.alive) break;
      }
    }
  }
  // road objects of the race
  //  OBJET2 (skull barrel / bomb): explodes.  OBJET1 (plain barrel / crate): heavy, is pushed away, slows the car
  //  down, hardly any damage, dull thud.  PLOT (cone): flies up, turns in the air and lands somewhere else.
  for (int i = 0; i < TK.nobj; i++) {
    TrackObj &o = TK.obj[i]; if (!o.alive) continue;
    if (o.hit_cd > 0) o.hit_cd -= dt;
    if (o.moving) obj_move(o, dt);
    const RoadPt &q = RP(o.node);   // the object's current frame (after a possible node change)
    const double *w = o.W;
    if (o.hz > OBJ_PHYS.fly_nohit) continue;   // high in the air: hits nothing
    // the object's ground point (its road surface), used for the level test against the car's road
    double g[3]; for (int k = 0; k < 3; k++) g[k] = w[k] - q.up[k] * (o.h + o.hz);
    for (int k = 0; k < RC.ncar; k++) {
      RCar &c = RC.car[k]; if (c.out) continue;
      double a, l; rel_to(c, w, &a, &l);
      if (d_abs(a) >= 12000 || d_abs(l) >= 9000) continue;
      if (d_abs(height_at(c, g)) > 3000 || !nodes_connected(c.pp.i, o.node, 6)) continue;   // other level or other road
      if (o.hit_cd > 0) break;
      // relative motion: the car's velocity minus the object's, both in world space
      double cv[3], ov[3];
      for (int m = 0; m < 3; m++) { cv[m] = c.F[m] * c.vlong + c.Rt[m] * c.vlat; ov[m] = q.fw[m] * o.vlon + q.rt[m] * o.vlat; }
      double dir[3] = {w[0] - c.W[0], w[1] - c.W[1], w[2] - c.W[2]};
      { double hu = dir[0] * c.U[0] + dir[1] * c.U[1] + dir[2] * c.U[2]; for (int m = 0; m < 3; m++) dir[m] -= c.U[m] * hu; }
      double dl = d_sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]); if (dl < 1) { for (int m = 0; m < 3; m++) dir[m] = c.F[m]; dl = 1; } else for (int m = 0; m < 3; m++) dir[m] /= dl;
      double closing = (cv[0] - ov[0]) * dir[0] + (cv[1] - ov[1]) * dir[1] + (cv[2] - ov[2]) * dir[2];
      double side = l >= 0 ? 1 : -1;   // object to the right of the car: pushed to the right
      if (o.type != 2 && closing < OBJ_PHYS.close_min) {
        // resting / very slow contact: only push the object out of the car, no impulse, no damage, no sound
        double push = 600; o.dlon += (dir[0] * q.fw[0] + dir[1] * q.fw[1] + dir[2] * q.fw[2]) * push; o.dlat += (dir[0] * q.rt[0] + dir[1] * q.rt[1] + dir[2] * q.rt[2]) * push;
        obj_update_world(o); obj_sync_pp(o);
        break;
      }
      double clon = cv[0] * q.fw[0] + cv[1] * q.fw[1] + cv[2] * q.fw[2], clat = cv[0] * q.rt[0] + cv[1] * q.rt[1] + cv[2] * q.rt[2];
      double f = d_clamp(closing / (0.5 * V_PLAYER), 0.2, 1.0);   // a slow push moves the object less
      if (sys_debug_enabled() && A.demo_time > 0) { char m[96] = "object hit: type "; str_int(m, 96, o.type); str_cat(m, " node ", 96); str_int(m, 96, o.node); str_cat(m, " car ", 96); str_int(m, 96, k); str_cat(m, " t ", 96); str_int(m, 96, (long)(RC.t * 100)); sys_log(m); }
      if (o.type == 2) {        // explosive barrel
        o.alive = 0; damage_car(c, 6666 / 655.36, -1); c.vlong *= 0.6; c.v *= 0.6; c.stun = 0.8;
        fx_add(OM_EXP_BIG, o.pp, o.x, 2500, 1.4); sfx_at(S_EXPLO1, w, 1.0f);
      } else if (o.type == 1) { // heavy barrel / crate: rammed away
        const ObjPhys &P = OBJ_PHYS;
        // M.EXE's normal exchange/velocity-dependent retention: transfer the
        // impact along its actual direction, preserve tangential motion and
        // remove the old random sideways launch.
        int ax=contact_raw(clat/V_RAW),az=contact_raw(clon/V_RAW),bx=contact_raw(o.vlat/V_RAW),bz=contact_raw(o.vlon/V_RAW);
        int old_ax=ax,old_az=az,old_bx=bx,old_bz=bz;
        int retain=original_contact_retention(ax,az,bx,bz,0);
        int dx=contact_raw((dir[0]*q.rt[0]+dir[1]*q.rt[1]+dir[2]*q.rt[2])*10000),dz=contact_raw((dir[0]*q.fw[0]+dir[1]*q.fw[1]+dir[2]*q.fw[2])*10000);
        original_contact_exchange(dx,dz,ax,az,bx,bz);
        double new_lat=(ax*retain+old_ax*(256-retain))/256.0*V_RAW,new_lon=(az*retain+old_az*(256-retain))/256.0*V_RAW;
        o.vlat=(bx*retain+old_bx*(256-retain))/256.0*V_RAW;o.vlon=(bz*retain+old_bz*(256-retain))/256.0*V_RAW;
        c.vlat=c.vlong=0;for(int m=0;m<3;m++){double velocity=q.rt[m]*new_lat+q.fw[m]*new_lon;c.vlat+=velocity*c.Rt[m];c.vlong+=velocity*c.F[m];}
        o.yawr=d_clamp((l*c.vlong-a*c.vlat)/180000000.0,-P.barrel_spin,P.barrel_spin);o.moving=1;o.touched=1;o.hit_cd=P.barrel_cd;
        damage_car(c,P.barrel_damage*f,-1);
        if(hard_wall_impact(closing))sfx_at(S_BIDON,w,(float)(.5+.5*f));
      } else {                  // cone: up into the air, turning, lands elsewhere
        const ObjPhys &P = OBJ_PHYS;
        o.vlon = clon * frnd(P.cone_lon_min, P.cone_lon_max); o.vlat = clat * 0.5 + side * f * frnd(P.cone_side_min, P.cone_side_max); o.vz = f * frnd(P.cone_up_min, P.cone_up_max);
        o.yawr = frnd(-P.cone_spin, P.cone_spin); o.pitchr = frnd(-P.cone_tumble, P.cone_tumble); o.moving = 1; o.touched = 1; o.hit_cd = P.cone_cd;
        c.vlong *= P.cone_brake; c.v *= P.cone_brake; sfx_at(S_PLOT, w, 0.8f);
      }
      break;
    }
  }
  for (int i = 0; i < 24; i++) if (RC.fx[i].alive) { RC.fx[i].t += dt; if (RC.fx[i].t > 1.2) RC.fx[i].alive = 0; }
  // classification: finished first (by time), then by distance
  for (int i = 0; i < RC.ncar; i++) RC.order[i] = i;
  for (int i = 1; i < RC.ncar; i++) {
    int k = RC.order[i], j = i - 1;
    auto key = [&](int c) { const RCar &r = RC.car[c]; return r.finished ? 1e9 - r.finish_t : r.out ? -1e9 + r.prog : r.prog; };
    while (j >= 0 && key(RC.order[j]) < key(k)) { RC.order[j + 1] = RC.order[j]; j--; }
    RC.order[j + 1] = k;
  }
  for (int i = 0; i < RC.ncar; i++) RC.car[RC.order[i]].place = i + 1;
  // camera: the picture of the video frame at the car's road point (its camera is ~7-8 points behind),
  // followed smoothly, never backwards
  {
    double back_t = 7.5 * PT;
    double pv = P.vlong;
    double w = 2.6, back = RC.ply_D - RC.cam_D;
    RC.cam_v += ((back - back_t) * w * w + 2 * w * (pv - RC.cam_v)) * dt;
    if (RC.cam_v < 0) RC.cam_v = 0;
    RC.cam_D += RC.cam_v * dt;
    back = RC.ply_D - RC.cam_D;
    if (back < 5.0 * PT) { RC.cam_D = RC.ply_D - 5.0 * PT; if (RC.cam_v > pv) RC.cam_v = pv > 0 ? pv : 0; }
    if (back > 16 * PT) RC.cam_D = RC.ply_D - 16 * PT;
  }
  // Engine gain is independent of race effects; a disabled bus does no audio work.
  if(audio_enabled(AUDIO_ENGINES)) {
    car_engine_select(&RC.eng_voice,P.car,0,mexe_engine_frequency((int)(P.v/V_RAW))/22050.f);
    Voice *v = snd_voice(RC.eng_voice);
    if (v) { double r = d_abs(P.v) / V_PLAYER; v->rate = mexe_engine_frequency((int)(P.v/V_RAW))/22050.0; v->vol = (float)((0.17 + 0.15 * r) * MX.sfx_vol) * (P.out ? 0 : 1)*car_engine_gain(P.car); }
    double best = 1e9; int bi = -1;
    for (int k = 1; k < RC.ncar; k++) { double d = dist_pts(RC.car[k].W, P.W); if (d < best && !RC.car[k].out) { best = d; bi = k; } }
    if(bi>=0&&best<12)car_engine_select(&RC.eng2_voice,RC.car[bi].car,0,1);else{snd_stop(RC.eng2_voice);RC.eng2_voice=-1;}
    Voice *v2 = snd_voice(RC.eng2_voice);
    if (v2 && bi>=0) { v2->vol = (float)((1 - best / 12) * 0.22 * MX.sfx_vol)*car_engine_gain(RC.car[bi].car); double a,l;rel_to(P,RC.car[bi].W,&a,&l);v2->rate=mexe_engine_frequency((int)(RC.car[bi].v/V_RAW))/22050.0;v2->pan=(float)d_clamp(l/20000.0,-0.8,0.8); }
  }
  scrape_update();
  if (RC.phase == PH_DONE) RC.done_t += dt;
  if (RC.crashed) RC.done_t += dt;
}

// ------------------------------------------------------------------ drawing
static Font g_font_big, g_font_small, g_font_mid;
static Pic g_racemenu;                  // pause menu bar (DATA640\MENU0.HLZ)
static SprFile g_pausedrum[3];          // its drums (MENUA/B/C.SPR)
static void t_hud_replay(void);

// Original 320x400 HUD: x doubles and y scales by 480/400 on the 640x480 canvas.
// M.EXE (German GOG) 0x1440f draws TRACK*/<track>.SPR with index zero transparent;
// the associated <track>.HLZ supplies the palette. ARMES.HLZ is an unused, different sheet.
static void cv_armes(int icon, int dx, int dy) {
  if (icon < 0 || icon >= TK.hud.n) return;
  Sprite s = TK.hud.f[icon]; s.x = s.y = 0;
  cv_spr_rect(s,TK.hud_pal,2.0,1.2,dy,dx);

}
static int weapon_icon(int w) { return w == W_KARD ? 0 : w == W_BOOMER ? 1 : w == W_HITRAX ? 2 : w == W_MINE ? 3 : w == W_OIL ? 4 : -1; }
static void cv_bar(int x, int y, int shield, double frac) {
  // M.EXE 0x14539/0x145d4: floor(raw/2048)/2 segments, left to right;
  // shield max 65535 gives 15 segments; a new hull is 65536 and gives 16.
  // First five are palette 254,
  // remaining segments 252; alternate dark lines use 251. Frame: 34 by 7/5.
  int lit = (int)(d_clamp(frac, 0, 1) * (shield?65535.0:65536.0)) / 2048 / 2;
  int hh = shield ? 5 : 7;
  uint32_t light = pal_rgba(TK.hud_pal, 252), dark = pal_rgba(TK.hud_pal, 251);
  cv_rect(x, y, 70, (int)((hh + 1) * 1.2), light);
  cv_rect(x + 2, y + 1, 66, (int)((hh - 1) * 1.2), dark);
  for (int i = 0; i < lit; i++) {
    int xx = x + 4 + i * 4;
    cv_rect(xx, y + 2, 2, shield ? 3 : 5, pal_rgba(TK.hud_pal, i < 5 ? 254 : 252));
  }
}

// the player's route around the camera: points behind (history) and ahead (both lanes at forks)
static int g_route[256], g_route_n;
static int route_has(int i) { for (int k = 0; k < g_route_n; k++) if (g_route[k] == i) return 1; return 0; }
static void route_build(void) {
  g_route_n = 0;
  for (int k = RC.hn - 1; k >= 0 && k >= RC.hn - 30 && g_route_n < 40; k--) g_route[g_route_n++] = RC.hist[k % HIST];
  int q0 = g_route_n; g_route[g_route_n++] = player().pp.nx;
  for (int h = q0; h < g_route_n && g_route_n < 250; h++) {
    if (h - q0 > 120) break;
    const RoadPt &r = RP(g_route[h]);
    for (int k = 0; k < 2; k++) { int n = r.nx[k]; if (!route_has(n) && g_route_n < 250) g_route[g_route_n++] = n; }
  }
}
// Invisible road surface in the depth buffer: cars behind a crest or a dip of the road are hidden
// by the road itself (the video has no depth, so without this they were drawn floating above the hill).
static GLuint g_occ_prog, g_occ_vao, g_occ_vbo; static GLint g_occ_uP;
static float g_occ_buf[256 * 2 * 6 * 6 * 3];
static void occluder_draw(const double *cam) {
  if (!g_occ_prog) {
    g_occ_prog = shader_build("#version 330 core\nlayout(location=0) in vec3 aPos; uniform mat4 uP; void main(){ gl_Position = uP * vec4(aPos, 1.0); }\n",
                              "#version 330 core\nout vec4 o; void main(){ o = vec4(0.0); }\n");
    g_occ_uP = glGetUniformLocation(g_occ_prog, "uP");
    glGenVertexArrays(1, &g_occ_vao); glGenBuffers(1, &g_occ_vbo);
    glBindVertexArray(g_occ_vao); glBindBuffer(GL_ARRAY_BUFFER, g_occ_vbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);
  }
  const double *R0 = cam + 3, *R1 = cam + 6, *R2 = cam + 9;
  int n = 0; const int SUB = 3;
  auto eye = [&](const double w[3]) {
    double d[3] = {w[0] + g_shift[0] - cam[0], w[1] + g_shift[1] - cam[1], w[2] + g_shift[2] - cam[2]};
    g_occ_buf[n++] = (float)((R0[0] * d[0] + R0[1] * d[1] + R0[2] * d[2]) * MODEL_SCALE);
    g_occ_buf[n++] = (float)((R1[0] * d[0] + R1[1] * d[1] + R1[2] * d[2]) * MODEL_SCALE);
    g_occ_buf[n++] = (float)(-(R2[0] * d[0] + R2[1] * d[1] + R2[2] * d[2]) * MODEL_SCALE);
  };
  for (int k = 0; k < g_route_n; k++) {
    int i = g_route[k]; const RoadPt &r = RP(i);
    for (int l = 0; l < 2; l++) {
      int nx = r.nx[l]; if (nx < 0 || nx >= TK.nroad || (l == 1 && nx == r.nx[0])) continue;
      for (int s = 0; s < SUB; s++) {
        if (n + 18 > (int)(sizeof g_occ_buf / sizeof g_occ_buf[0])) break;
        double c[2][2][3];
        for (int e = 0; e < 2; e++) {
          PPos q; q.i = i; q.nx = nx; q.t = (s + e) / (double)SUB;
          double p[3], rt[3], up[3], fw[3], w; pp_frame(q, p, rt, up, fw, &w);
          double hw = w * 1.15;
          for (int k2 = 0; k2 < 3; k2++) { c[e][0][k2] = p[k2] - rt[k2] * hw - up[k2] * 350; c[e][1][k2] = p[k2] + rt[k2] * hw - up[k2] * 350; }
        }
        eye(c[0][0]); eye(c[0][1]); eye(c[1][1]); eye(c[0][0]); eye(c[1][1]); eye(c[1][0]);
      }
    }
  }
  if (!n) return;
  glUseProgram(g_occ_prog); glUniformMatrix4fv(g_occ_uP, 1, GL_FALSE, g_proj.m);
  glBindVertexArray(g_occ_vao); glBindBuffer(GL_ARRAY_BUFFER, g_occ_vbo);
  glBufferData(GL_ARRAY_BUFFER, n * 4, g_occ_buf, GL_DYNAMIC_DRAW);
  glColorMask(0, 0, 0, 0); glDepthMask(1);
  glDrawArrays(GL_TRIANGLES, 0, n / 3);
  glColorMask(1, 1, 1, 1);
  glBindVertexArray(0);
}
static float VIDEO_Y0 = 45, VIDEO_H = 350;    // track video on the 640x480 screen: unstretched as in the original 640x400 picture (debug option: 30/420 = 4:3 stretched)

#include "g_shadows.inc"
#include "g_effects.inc"
static void race_draw(int W, int H) {
  RCar &P = player();
  if(A.graphics_test==19||A.graphics_test==20)RC.pause_slide=A.graphics_test==19?1:.5;
  double pause_slide=RC.pause_slide;pause_slide=pause_slide*pause_slide*(3-2*pause_slide);
  float pause_offset=g_tex_nearest?(float)(96*pause_slide):0;
  if (RC.phase == PH_FLYBY) {
    int f = TK.nroad + (int)(RC.phase_t * 12.5); if (f >= TK.vid.nframes) f = TK.vid.nframes - 1;
    video_decode(&TK.vid, f); video_tex_upload(&TK.vtex, TK.vid);
    int vx, vy, vw, vh; vrect_viewport(W, H, 0, VIDEO_Y0+pause_offset, 640, VIDEO_H, &vx, &vy, &vw, &vh);
    draw_video_rect(TK.vtex, W, H, vx, vy, vw, vh);
    cv_clear(0); canvas_draw(W, H);
    return;
  }
  // the package of the last original update is shown as it is (picture, its CAM record, visibility, poses)
  RC.pkg_repeat++;
  if (A.warptest) {   // test: picture of frame K turned to the camera of frame K+3 (compare with frame K+3)
    int k = A.warptest > 0 ? A.warptest : -A.warptest, kc = A.warptest > 0 ? k + 3 : k;
    RC.frame = k; for (int q = 0; q < 12; q++) RC.vcam[q] = TK.cam[kc * 12 + q];
    for (int r = 0; r < 3; r++) { double *v = RC.vcam + 3 + r * 3; double l = d_sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) for (int q = 0; q < 3; q++) v[q] /= l; }
  }
  // debug output modes 60 FPS: draw between the previous and the latest update (one update behind),
  // camera and cars with the same fraction; the video shows the nearer of the two pictures
  double ia = -1; int save_frame = RC.frame; double save_cam[12]; for (int k = 0; k < 12; k++) save_cam[k] = RC.vcam[k];
  if (g_out_mode == 1 && !g_replay_mode && !A.warptest && !g_cam_rot_interp && g_snap[0].valid && g_snap[1].valid) {
    double t0 = RC.cam_old * 5.0 / 256.0, len = RC.delta_units * 5.0 / 256.0;
    ia = len > 0 ? (RC.clock_ms - t0) / len : 1; if (ia < 0) ia = 0; if (ia > 1) ia = 1;
    const PoseSnap &a = g_snap[0], &b = g_snap[1];
    double dx = a.cam[0] - b.cam[0], dy = a.cam[1] - b.cam[1], dz = a.cam[2] - b.cam[2];
    int cut = dx * dx + dy * dy + dz * dz > 20000.0 * 20000.0 || a.frame == b.frame ? 1 : 0;   // camera cut or fixed camera
    if (cut) { const PoseSnap &u = ia < 0.5 && a.frame != b.frame ? a : b; RC.frame = u.frame; for (int k = 0; k < 12; k++) RC.vcam[k] = u.cam[k]; }
    else {
      RC.frame = ia < 0.5 ? a.frame : b.frame;
      lerp3(a.cam, b.cam, ia, RC.vcam, 0);
      for (int r = 0; r < 3; r++) lerp3(a.cam + 3 + r * 3, b.cam + 3 + r * 3, ia, RC.vcam + 3 + r * 3, 1);
    }
  }
  if(A.graphics_test){ // Deterministic developer captures; no normal gameplay path uses this.
    TK.nobj=0;mem_zero(RC.haz,sizeof RC.haz);mem_zero(RC.proj,sizeof RC.proj);mem_zero(RC.fx,sizeof RC.fx);
    PPos q=P.pp;double x=P.x;pp_move(q,x,1.8*PT);
    if(A.graphics_test==1)for(int i=0;i<3;i++){Proj &p=RC.proj[i];p.alive=1;p.kind=W_KARD+i;p.pp=q;p.x=x+(i-1)*9000;p.life=4;p.gen=++g_proj_gen;}
    if(A.graphics_test==10||A.graphics_test==11){Proj &p=RC.proj[0];p.alive=1;p.kind=W_KARD+(int)d_clamp(A.test_damage,0,2);p.pp=q;p.x=x-9000;p.life=3.8;p.gen=100;}
    if(A.graphics_test==2){Hazard &h=RC.haz[0];h.alive=1;h.kind=W_OIL;h.pp=q;h.x=x-12000;h.life=100;pp_world(q,h.x,0,h.W);}
    if(A.graphics_test==7){RC.ncar=2;RCar &c=RC.car[1];c=P;c.car=1;c.player=0;c.pp=q;c.x=x-17000;c.damage=A.test_damage;c.braking=1;c.out=A.test_damage>=100;car_update_world(c);}
    if(A.graphics_test==8&&(g_contact_next==0||RC.t-g_contacts[(g_contact_next-1)%128].birth>.035)){double w[3],vel[3];for(int k=0;k<3;k++){w[k]=P.W[k]+P.Rt[k]*(P.hwm+250)-P.F[k]*7000+P.U[k]*1000;vel[k]=-P.F[k]*65000;}contact_sparks(w,P.U,vel,.9f,1);}
    if(A.graphics_test==9){static PPos base;static double base_x;static int have;if(!have){base=P.pp;base_x=P.x;have=1;}P.pp=base;P.x=base_x;pp_move(P.pp,P.x,RC.t*8000);car_update_world(P);}
    if(A.graphics_test==5)P.braking=1;
    if(A.graphics_test==6){P.damage=A.test_damage;P.out=A.test_damage>=100;if(P.out&&g_damage_effects){effects_reset();Fx &f=RC.fx[0];f.alive=1;f.model=OM_EXP_BIG;f.scale=2.4;f.t=A.effect_age_ms*.001;pp_world(P.pp,P.x,2500,f.W);}}
    if(A.graphics_test==12||A.graphics_test==13){effects_reset();Fx &f=RC.fx[0];f.alive=1;f.model=OM_EXP_BIG;f.scale=A.graphics_test==12?1.4:1.3;f.t=A.effect_age_ms*.001;pp_world(q,x-12000,2500,f.W);}
    if(A.graphics_test==3){effects_reset();Fx &f=RC.fx[0];f.alive=1;f.model=OM_EXP_BIG;f.scale=1.35;f.t=A.effect_age_ms*0.001;pp_world(q,x-12000,2500,f.W);}
  }
  route_build();
  // two textures hold the last two track pictures, so switching between the previous and the latest picture
  // (60 FPS) needs no new decoding
  static GLuint vslot[2]; static int vslot_frame[2] = {-1, -1}, vslot_next;
  if (g_race_tex_frame < 0) vslot_frame[0] = vslot_frame[1] = -1;   // the textures were used for something else
  GLuint vtex = 0;
  for (int k = 0; k < 2; k++) if (vslot[k] && vslot_frame[k] == RC.frame && !A.warptest) vtex = vslot[k];
  if (!vtex) {
    int k = vslot_next; vslot_next ^= 1;
    video_decode(&TK.vid, RC.frame);
    video_tex_upload(&vslot[k], TK.vid);
    vslot_frame[k] = RC.frame; vtex = vslot[k];
  }
  g_race_tex_frame = RC.frame;
  int vx, vy, vw, vh; vrect_viewport(W, H, 0, VIDEO_Y0+pause_offset, 640, VIDEO_H, &vx, &vy, &vw, &vh);
  if (A.yoff_set) g_proj_yoff = A.yoff;
  track_proj();
  {  // the video frame is turned to the interpolated camera, so cars and background stay together in curves
    const double *f = TK.cam + RC.frame * 12, *v = RC.vcam;
    double Rf[3][3], Rv[3][3];
    for (int r = 0; r < 3; r++) {
      double fl = d_sqrt(f[3 + r * 3] * f[3 + r * 3] + f[4 + r * 3] * f[4 + r * 3] + f[5 + r * 3] * f[5 + r * 3]); if (fl < 1e-9) fl = 1;
      for (int k = 0; k < 3; k++) { double sg = r == 2 ? -1 : 1; Rf[r][k] = sg * f[3 + r * 3 + k] / fl; Rv[r][k] = sg * v[3 + r * 3 + k]; }
    }
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) g_vwarp[A.camoff == 2 ? i * 3 + j : j * 3 + i] = (float)(Rf[i][0] * Rv[j][0] + Rf[i][1] * Rv[j][1] + Rf[i][2] * Rv[j][2]);   // column-major
    g_vproj[0] = g_proj.m[0]; g_vproj[1] = g_proj.m[5]; g_vproj[2] = g_proj.m[9];
    g_vwarp_on = g_cam_rot_interp && RC.frame >= 0 && RC.frame < TK.ncam;
  }
  draw_video_rect(vtex, W, H, vx, vy, vw, vh);
  g_vwarp_on = 0;
  if (A.warptest) { cv_clear(0); canvas_draw(W, H); return; }
  glViewport(vx, vy, vw, vh);
  glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDisable(GL_CULL_FACE);
  glClear(GL_DEPTH_BUFFER_BIT);
  const double *cam = RC.vcam;
  if (!A.roaddbg) occluder_draw(cam);
  model_shader_begin(g_proj, RC.gain);
  auto shifted_depth = [&](const double w[3]) { double p[3] = {w[0] + g_shift[0], w[1] + g_shift[1], w[2] + g_shift[2]}; return cam_depth(cam, p); };
  auto visible = [&](const PPos &q, double x) {
    double w[3]; pp_world(q, x, 0, w);
    double d = shifted_depth(w);
    return d > 4000 && d < 90 * PT && v3d_visible(RC.frame, q.i);
  };
  if (sys_debug_enabled() && A.demo_time > 0) {
    static double last = -1; if (RC.t - last > 1.0) { last = RC.t;
      char m[256] = "t="; str_int(m, 256, (long)(RC.t * 10)); str_cat(m, " pt=", 256); str_int(m, 256, P.pp.i); str_cat(m, " prog=", 256); str_int(m, 256, (long)P.prog);
      str_cat(m, " campt=", 256); str_int(m, 256, RC.campp.i); str_cat(m, " frame=", 256); str_int(m, 256, RC.frame); str_cat(m, " fpt=", 256); str_int(m, 256, TK.fpt[RC.frame]);
      str_cat(m, " run=", 256); str_int(m, 256, TK.frun[RC.frame]); str_cat(m, " shift=", 256); str_int(m, 256, (long)d_sqrt(g_shift[0] * g_shift[0] + g_shift[1] * g_shift[1] + g_shift[2] * g_shift[2]));
      str_cat(m, " x=", 256); str_int(m, 256, (long)P.x); str_cat(m, " v=", 256); str_int(m, 256, (long)P.v); str_cat(m, " dmg=", 256); str_int(m, 256, (long)P.damage);
      str_cat(m, " depth=", 256); str_int(m, 256, (long)shifted_depth(P.W));
      str_cat(m, RC.cam_mob ? " MOB" : " FIX", 256); str_cat(m, " cs=", 256); str_int(m, 256, RC.cam_speed); str_cat(m, " du=", 256); str_int(m, 256, RC.delta_units); str_cat(m, " upd=", 256); str_int(m, 256, RC.upd_n); str_cat(m, " late=", 256); str_int(m, 256, RC.late_n); sys_log(m); }
  }
  if (A.roaddbg) {   // debug: cones on the road centre and both edges along the camera's path
    Model *m = TK.obj_models[OM_CONE];
    if (m) { int st = P.pp.i; for (int k = 0; k < 10; k++) st = RP(st).pv[0]; PPos q = pp_make(st, 0); double x0 = 0;
      for (int k = 0; k < 40; k++) { double hw = pp_halfw(q); for (int e = -1; e <= 1; e++) if (visible(q, e * hw)) draw_model_mv(*m, road_mv(cam, q, e * hw, 0, 0, 1.0, 0), 0, 0); pp_move(q, x0, 2 * PT); } }
  }
  g_scene_count=0;
  // road objects
  for (int i = 0; i < TK.nobj; i++) {
    TrackObj &o = TK.obj[i]; if (!o.alive) continue;
    int kind = o.type == 1 ? OM_OBJ1 : o.type == 2 ? OM_BARREL : OM_CONE;
    Model *m = TK.obj_models[kind]; if (!m) continue;
    double ow[3] = {o.W[0] + g_shift[0], o.W[1] + g_shift[1], o.W[2] + g_shift[2]};
    { double d = cam_depth(cam, ow); if (d < 4000 || d > 90 * PT || !v3d_visible(RC.frame, o.node)) continue; }
    // the object's own PST frame (M.EXE: node << 7, local values +0x38/+0x3C/+0x40), turned / tumbling after a hit
    double pos[3], R[3], U[3], F[3]; obj_pose(o, pos, R, U, F);
    if (ia >= 0 && i < MAXOBJ && g_snap[0].oalive[i] && g_snap[1].oalive[i]) {   // in-between picture
      lerp3(g_snap[0].opos[i], g_snap[1].opos[i], ia, pos, 0); lerp3(g_snap[0].oR[i], g_snap[1].oR[i], ia, R, 1);
      lerp3(g_snap[0].oU[i], g_snap[1].oU[i], ia, U, 1); lerp3(g_snap[0].oF[i], g_snap[1].oF[i], ia, F, 1);
    }
    for (int k = 0; k < 3; k++) pos[k] += g_shift[k];
    scene_add(*m, world_mv(cam, pos, R, U, F, 1.0), 0, 0);
  }
  for (int i = 0; i < 48; i++) {
    Hazard &h = RC.haz[i]; if (!h.alive) continue;
    Model *m = TK.obj_models[h.kind == W_MINE ? OM_MINE : OM_OIL]; if (!m || !visible(h.pp, h.x)) continue;
    scene_add(*m, road_mv(cam, h.pp, h.x, 0, 0, 1.0, 0), 0, 0);
  }
  for (int i = 0; i < 32; i++) {
    Proj &p = RC.proj[i]; if (!p.alive) continue;
    Model *m = TK.obj_models[projectile_model(p.kind)]; if (!m || !visible(p.pp, p.x)) continue;
    M4 missile_mv;
    if (ia >= 0 && g_snap[0].palive[i] && g_snap[1].palive[i] && g_snap[0].pgen[i] == g_snap[1].pgen[i]) {
      double pos[3], R[3], U[3], F[3];
      lerp3(g_snap[0].ppos[i], g_snap[1].ppos[i], ia, pos, 0); lerp3(g_snap[0].pR[i], g_snap[1].pR[i], ia, R, 1);
      lerp3(g_snap[0].pU[i], g_snap[1].pU[i], ia, U, 1); lerp3(g_snap[0].pF[i], g_snap[1].pF[i], ia, F, 1);
      missile_mv=world_mv(cam, pos, R, U, F, 1.0);
    } else
    missile_mv=road_mv(cam, p.pp, p.x, 0, p.psi, 1.0, 0);
    scene_add(*m,missile_mv,0,0);
    if((A.graphics_test==10||A.graphics_test==11)&&!g_missile_emitters[i].valid){double now=RC.t;for(int sample=0;sample<=24;sample++){RC.t=now-.2+sample/120.0;PPos previous_pp=p.pp;double previous_x=p.x;pp_move(previous_pp,previous_x,-(now-RC.t)*180000);M4 previous=road_mv(cam,previous_pp,previous_x,0,p.psi,1.0,0);missile_effect_pose(cam,i,p,*m,previous);}RC.t=now;}
    missile_effect_pose(cam,i,p,*m,missile_mv);
  }
  // cars, far to near
  int idx[8]; double dep[8]; int n = 0;
  for (int i = 0; i < RC.ncar; i++) { idx[n] = i; dep[i] = shifted_depth(RC.car[i].W); n++; }
  for (int i = 1; i < n; i++) { int k = idx[i], j = i - 1; while (j >= 0 && dep[idx[j]] < dep[k]) { idx[j + 1] = idx[j]; j--; } idx[j + 1] = k; }
  for (int ii = 0; ii < n; ii++) {
    RCar &c = RC.car[idx[ii]];
    if (c.out && !c.player && !(g_damage_effects&&(!g_tex_nearest))) continue;
    if (!c.player && !visible(c.pp, c.x)) continue;
    if (!c.player && !route_has(c.pp.i) && !route_has(c.pp.nx)) {   // a car on another lane: only when that lane runs right beside ours
      double best = 1e300; int bi = -1;
      for (int k = 0; k < g_route_n; k++) { const RoadPt &r = RP(g_route[k]); double dx = r.p[0] - c.W[0], dy = r.p[1] - c.W[1], dz = r.p[2] - c.W[2], d2 = dx * dx + dy * dy + dz * dz; if (d2 < best) { best = d2; bi = g_route[k]; } }
      if (bi < 0 || d_sqrt(best) > RP(bi).w + 20000) continue;
    }
    if (c.player && dep[idx[ii]] < 3000) continue;
    Model *m = track_car_model(c.car); if (!m) continue;
    g_roll = 0;   // no visual roll of the body
    int ci = (int)(&c - RC.car);
    float wheel=(float)car_render_wheel(ci,ia);
    if (ia >= 0 && ci < 8 && g_snap[0].cgen[ci] == g_snap[1].cgen[ci]) {   // interpolated pose (in-between pictures)
      double pos[3], R[3], U[3], F[3];
      lerp3(g_snap[0].pos[ci], g_snap[1].pos[ci], ia, pos, 0); lerp3(g_snap[0].R[ci], g_snap[1].R[ci], ia, R, 1);
      lerp3(g_snap[0].U[ci], g_snap[1].U[ci], ia, U, 1); lerp3(g_snap[0].F[ci], g_snap[1].F[ci], ia, F, 1);
      scene_add(*m, world_mv(cam, pos, R, U, F, CAR_SCALE), wheel, ci+1);
    } else
    scene_add(*m, road_mv(cam, c.pp, c.x, 0, c.psi, CAR_SCALE, 0), wheel, ci+1);
    g_roll = 0;
  }
  scene_render(cam,W,H);
  // explosions (camera facing)
  for (int i = 0; i < 24; i++) {
    Fx &f = RC.fx[i]; if (!f.alive || modern_fx(f)) continue;
    Model *m = TK.obj_models[f.model]; if (!m) continue;
    double p[3] = {f.W[0] + g_shift[0], f.W[1] + g_shift[1], f.W[2] + g_shift[2]};
    if (cam_depth(cam, p) < 3000) continue;
    const double *R0 = cam + 3, *R1 = cam + 6, *R2 = cam + 9;
    double nF[3] = {-R2[0], -R2[1], -R2[2]};
    draw_model_mv(*m, world_mv(cam, p, R0, R1, nF, f.scale * 0.5), 0, (int)(f.t * 14));
  }
  effects_draw(cam);
  glBindVertexArray(0);
  if (ia >= 0) { RC.frame = save_frame; for (int k = 0; k < 12; k++) RC.vcam[k] = save_cam[k]; }
  // ---- HUD on the canvas (transparent)
  cv_clear(0);
  if (g_replay_mode) {
    cv_text(g_font_big, 320, 34, "WIEDERGABE", rgba(255, 255, 255, 255), 1, 1);
    t_hud_replay();
  } else {
    // the pause menu bar scrolls down from above (as in the original) and back up when the race goes on
    double slide = RC.pause_slide; slide = slide * slide * (3 - 2 * slide);
    int bar_y = (int)(-96 * (1 - slide));
    int hy = g_tex_nearest?0:(int)(80*slide);   // Retro shifts the complete composited canvas below.
    int fi = P.lo.fsel, ri = P.lo.rsel;
    char t[64];
    if (P.lo.front[fi] != W_NONE) { cv_armes(weapon_icon(P.lo.front[fi]), 14, 35 + hy); t[0] = 0; str_int(t, 64, P.lo.fammo[fi]); cv_text(g_font_big, 12, 61 + hy, t, rgba(255, 255, 255, 255), 0, 1); }
    if (P.lo.rear[ri] != W_NONE) { cv_armes(weapon_icon(P.lo.rear[ri]), 96, 30 + hy); t[0] = 0; str_int(t, 64, P.lo.rammo[ri]); cv_text(g_font_big, 94, 61 + hy, t, rgba(255, 255, 255, 255), 0, 1); }
    if (P.shield > 0) { cv_armes(5, 178, 30 + hy); cv_bar(160, 61 + hy, 1, P.shield / 100.0); }
    int show_rank = RC.phase != PH_GRID;   // before the start (rolling start) the original shows no Pos / Runden (START01-08)
    if (show_rank) {
    cv_text(g_font_big, 330, 30 + hy, "Pos", rgba(255, 255, 255, 255), 1, 1);
    t[0] = 0; str_int(t, 64, P.place); str_cat(t, "/", 64); str_int(t, 64, RC.ncar); cv_text(g_font_big, 334, 56 + hy, t, rgba(255, 255, 255, 255), 1, 1);
    }
    cv_bar(400, 42 + hy, 0, 1.0 - P.damage / 100.0);
    if (show_rank) {
    cv_text(g_font_big, 570, 30 + hy, "Runden", rgba(255, 255, 255, 255), 1, 1);
    int lap = (int)(P.prog / TK.L) + 1; if (lap < 1) lap = 1; if (lap > RC.laps) lap = RC.laps;
    t[0] = 0; str_int(t, 64, lap); str_cat(t, "/", 64); str_int(t, 64, RC.laps); cv_text(g_font_big, 580, 56 + hy, t, rgba(255, 255, 255, 255), 1, 1);
    }
    if (RC.msg_t > 0 && (!RC.paused||g_tex_nearest)) cv_text(g_font_big, 320, (int)(VIDEO_Y0 + VIDEO_H * 0.24f), RC.msg, RC.msg_col, 1, 1);
    if (g_dbg_fps && !g_dbg_info) { char d[32] = "FPS "; str_int(d, 32, (long)(A.fps + 0.5f)); cv_text(g_font_small, 634, 456, d, rgba(120, 255, 120, 255), 2, 1); }
    if (g_dbg_info) {   // debug: race info
      char d[200] = ""; str_cpy(d, ui_debug_text("Bild "), 200); str_int(d, 200, RC.frame); str_cat(d, ui_debug_text("  Punkt "), 200); str_int(d, 200, P.pp.i);
      str_cat(d, ui_debug_text("  Tempo "), 200); str_int(d, 200, (long)(P.v / PT * 10)); str_cat(d, ui_debug_text("/10 Pkt/s  x "), 200); str_int(d, 200, (long)P.x); str_cat(d, ui_debug_text("  Schaden "), 200); str_int(d, 200, (long)P.damage);
      str_cat(d, ui_debug_text("  Lenk "), 200); str_int(d, 200, (long)P.steer_st); str_cat(d, ui_debug_text("  roh "), 200); str_int(d, 200, (long)(P.v / V_RAW));
      str_cat(d, P.autopilot ? "  AUTO" : "", 200);
            cv_rect(0, 452, 640, 16, rgba(0, 0, 0, 160)); cv_text(g_font_small, 6, 453, d, rgba(120, 255, 120, 255), 0, 1);
      // cadence and presentation package (Original / 60 FPS)
      static const char *OM[2] = {"Original", "60fps"};
      char e[220] = ""; str_cpy(e, ui_debug_text("Ausgabe "), 220); str_cat(e, OM[g_out_mode % 2], 220); str_cat(e, " ", 220); str_int(e, 220, (long)(A.fps + 0.5f)); str_cat(e, ui_debug_text(" B/s  Upd "), 220);
      str_int(e, 220, (long)(g_upd_rate + 0.5)); str_cat(e, "/s  ", 220); str_cat(e, g_cam_rot_interp ? "ALT" : RC.cam_mob ? "MOB" : "FIX", 220);
      str_cat(e, ui_debug_text("  KamV "), 220); str_int(e, 220, RC.cam_speed); str_cat(e, "  Delta ", 220); str_int(e, 220, RC.delta_units);
      str_cat(e, "  CAM ", 220); str_int(e, 220, RC.frame); str_cat(e, " UBB ", 220); str_int(e, 220, RC.pkg_ubb);
      str_cat(e, ui_debug_text("  Paket x"), 220); str_int(e, 220, RC.pkg_repeat);
      str_cat(e, ui_debug_text("  versp. "), 220); str_int(e, 220, RC.late_n);
      cv_rect(0, 436, 640, 16, rgba(0, 0, 0, 160)); cv_text(g_font_small, 6, 437, e, rgba(255, 220, 120, 255), 0, 1);
    }
    if(g_tex_nearest&&pause_offset>0){
      // Translate the finished original HUD as one image, preserving the same
      // nearest-sampled pixel rows throughout the pause animation.
      texture_canvas_snapshot();for(int i=0;i<g_texture_layers.n;i++)g_texture_layers.p[i].y+=pause_offset;
    }
    if (RC.pause_slide > 0 && g_racemenu.px) {   // MENU0.HLZ (320x80 of the 320x400 mode) with the turning drums MENUA/B/C
      cv_pic_rect(g_racemenu, 0, bar_y, 640, 96);
      const SprFile &d = g_pausedrum[RC.pause_sel];
      if (d.n) cv_spr_rect(d.f[RC.paused ? (int)(RC.pause_anim * 25) % d.n : 0], g_racemenu.pal, 2.0, 1.2, bar_y);
    }
  }
  canvas_draw(W, H);
}
