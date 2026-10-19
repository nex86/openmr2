// ===================================================================================================
// Tracks: original track data from the CD and 3D rendering on top of the pre-rendered video.
//   .UBB  pre-rendered video (320x350, displayed in 320x400 mode)
//   .CAM  per video frame: camera pos[3] + rotation rows (right, up, forward), 1.0 = 32768
//   .PST  road points (128 bytes): pos, right, up, forward, width (+72), segment length (+80)
//   .V3D  per video frame: visible road point ranges (occlusion of 3D objects by the scenery)
//   .1/.2/.3 objects on the road for the three heats: count, 3 category counts, then (point, angle, lateral)
// Projection of the original engine: x' = 160 + X*407/Z, y' = 175 - Y*(407*5/3)/Z
// ===================================================================================================

struct TrackDef { const char *name, *cd, *set, *music, *intro_clip, *win_clip; };
// championship order of the original game
static const TrackDef TRACKS[6] = {
  {"SCHMIEDE", "TRACK1/FACTO", "FAC", "USINE", "BOYLE/BOYLE06.UBB", "BOYLE/BOYLE07C.UBB"},
  {"TIBET", "TRACK1/TIBET", "TIB", "TIBET", "BOYLE/BOYLE02.UBB", "BOYLE/BOYLE03C.UBB"},
  {"E-TERRESTRIAL", "TRACK1/EXTRA", "EXT", "ET", "BOYLE/BOYLE08.UBB", "BOYLE/BOYLE09C.UBB"},
  {"MONDBASIS 9", "TRACK2/LUNAR", "LUN", "LUNE", "BOYLE2/BOYLE10.UBB", "BOYLE2/BOYLE11C.UBB"},
  {"BAYOU", "TRACK2/BAYOU", "BAY", "BAYOU", "BOYLE2/BOYLE04.UBB", "BOYLE2/BOYLE05C.UBB"},
  {"BRONX", "TRACK2/BRONX", "BRO", "BRONX", "BOYLE2/BOYLE12.UBB", "BOYLE2/BOYLE13.UBB"},
};

// the 14 cars of the car shop (names, speeds and model files from M.EXE)
// prices, shown speeds and driving profiles from M.EXE (the shown Mph are only text: all cars have the same
// speed limits; the three profiles differ in steering and grip)
struct CarDef { const char *name, *model; int mph; int price; const char *fuel, *engine; int profile; };
static const CarDef CARS[14] = {
  {"Thunder V.", "CAISSE30", 120, 20000, "Nitro.", "Zb12000", 1},
  {"Vipper", "CAISSE90", 125, 20000, "Flux Metal", "Ak147", 2},
  {"Astra B.", "CRUSADER", 122, 20000, "Fluessiges Uran", "Power Ruckus II", 3},
  {"Strata VII", "CAISSE60", 120, 27000, "Eistee mit Milch", "UK-A E12", 1},
  {"CoCoon", "CAISSE50", 124, 28000, "Fluessiges Nitro.", "Suppo Dia-R", 2},
  {"BenHur 2000", "CAISSE20", 116, 26000, "Fluessiger Kuhdung", "Horss R-Vis", 3},
  {"MyWay", "CAISSE70", 118, 30000, "Fluessiges Nitro.", "Power Rustle 69", 1},
  {"Traction XII", "CAISSE10", 119, 29000, "Diesel", "Bear v12 16s", 2},
  {"Futura IV", "INTRUDER", 121, 30100, "Swift Acid", "A-Re66", 3},
  {"Condor", "HUNTER", 122, 25500, "Guacamole", "MX-Anchor Sys", 1},
  {"Flyzz II", "LEADER", 117, 27300, "Fluessiges Nitro.", "Hurricane E-12", 2},
  {"TonnCar", "CAISSE80", 120, 26750, "Fluessiges Uran", "Beastly Eng.B77", 3},
  {"Flyzz", "SPIDER", 117, 27500, "Fluessiges Nitro.", "Hurricane E-8", 1},
  {"Blatt 96", "STING005", 120, 26000, "Fluessiges Uran", "Vulcan v12 e5b", 2},
};
// driving profiles (17 values each in M.EXE): VitTurn, AnglMax, VitRet, DerpLat, DerpFrn, ...
struct DriveProfile { int vitturn, anglmax, vitret, derplat, derpfrn, slip1, slip2, corr1, corr2, rotlim, rotderp; };
static const DriveProfile PROFILES[4] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {6144, 11520, -137, 1500, 1910, 19, 48, 90, 36, 190, 370},
  {7168, 10368, -135, 1540, 1950, 18, 48, 93, 37, 190, 370},
  {5120, 12800, -140, 1420, 1820, 18, 48, 95, 39, 190, 370},
};
enum { NCARS = 14 };

// ---------------------------------------------------------------------------------------------------
// The road is a graph (.PST, 128 bytes per point):
//   +12 pos[3], +36 right[3], +48 up[3], +60 forward[3], +72 width,
//   +76.. link A: (angle, length, ?, prev, next), +96.. link B: same. At a fork the two links lead into
//   two half-width lanes separated by a barrier, which join again later. +116 high byte = zone id.
// The main loop is 0..P (P = prev of point 0); lane points beyond P belong to the alternative lanes.
// The track video contains footage for every lane: each video frame's camera is near one road point.
// ---------------------------------------------------------------------------------------------------
struct RoadPt {
  int physics_turn[2],physics_length[2],physics_rotation[2]; double p[3], rt[3], up[3], fw[3], w; int nx[2], pv[2]; double len[2]; int zone; double M; int fr0, nfr;
                int kind, mobflag, anc_f, anc_r; };   // PST +0x00 (type & 0x7FFF, bit 15 of the low word), +0x08 / +0x0A camera anchors
struct PPos { int i, nx; double t; };                  // position on the road graph: segment i -> nx, fraction t
// road objects of the race (.1/.2/.3): type 1 OBJET1, 2 OBJET2, 8 PLOT; canonical original values node / longitudinal / lateral / height
struct TrackObj { PPos pp; double x, angle; int kind, alive, variant; int type, node, lon, lat, h; double W[3];   // kind 1 obstacle (OBJET1/2), 2 cone (PLOT)
                 // movement after a hit, in the object's own PST frame: offset along / across, height above the road,
                 // speeds, turn (yaw) and tumble (pitch) for the cones
                 double dlon, dlat, hz, vlon, vlat, vz, yaw, yawr, pitch, pitchr, hit_cd; int moving;
                 int touched, node0, lon0, lat0; };   // touched: moved by a hit (replays); node0/lon0/lat0: original placement
enum { MAXOBJ = 64 };

// road objects: the .1/.2/.3 files have three groups: bonus items, explosive barrels (skull), cones
enum { OM_BARREL, OM_MONEY, OM_CONE, OM_MINE, OM_OIL, OM_MISSILE, OM_EXP_BIG, OM_EXP_MID, OM_EXP_SMALL, OM_REPAIR, OM_OBJ1, OM_MISSILE_MID, OM_MISSILE_BIG, OM_COUNT };
static const char *OBJ_MODEL_FILES[OM_COUNT] = {"OBJET2.M3D", "*MONEY.M3D", "PLOT.M3D", "TR_MINE2.M3D", "OIL.M3D", "TR_MIS2.M3D", "EXPLOBIG.M3D", "EXP_MOY.M3D", "EXPLSMAL.M3D", "*REPAIR.M3D", "OBJET1.M3D", "TR_MIS1.M3D", "TR_MIS.M3D"};
// initial height of the road objects per track (M.EXE CORRECTION_Y_CIRCUIT, campaign order)
static const int CORRECTION_Y_CIRCUIT[6] = {-550, -600, -1000, 0, -2000, 0};

struct Track {
  SprFile hud;
  uint8_t hud_pal[768];
  int loaded, idx, heat;
  Video vid;
  int ncam; double *cam;              // 12 doubles per frame
  uint32_t *cam_ubb;                  // CAM +0x10: seek offset of the frame in the UBB stream (diagnosis)
  RoadPt *road; int nroad; int L;     // L = length of the main loop in points (P + 1)
  uint8_t *v3d; size_t v3d_size; int nv3d;
  int nframes; int *fpt, *fnx, *frun; double *ft;   // per frame: road point / segment / fraction of its camera, cut-free run id
  int *frlist; uint8_t *fok;           // frames sorted by point (RoadPt.fr0 / nfr index into it)
  TrackObj obj[MAXOBJ]; int nobj;
  Model *car_models[NCARS];
  Model *obj_models[OM_COUNT];
  Sound music;
  GLuint vtex;
};
static Track TK;

static const RoadPt &RP(int i) { return TK.road[i < 0 ? 0 : i >= TK.nroad ? TK.nroad - 1 : i]; }
static double seg_len(int i, int nx) {
  const RoadPt &a = RP(i);
  if (nx == a.nx[0]) return a.len[0];
  if (nx == a.nx[1]) return a.len[1];
  const RoadPt &b = RP(nx); double dx = b.p[0] - a.p[0], dy = b.p[1] - a.p[1], dz = b.p[2] - a.p[2];
  double l = d_sqrt(dx * dx + dy * dy + dz * dz); return l > 100 ? l : 6900;
}
static void pp_frame(const PPos &q, double p[3], double rt[3], double up[3], double fw[3], double *w) {
  // smooth (Hermite) curve through the road points with the road direction as tangent: a straight polyline
  // has a kink at every point, which made the cars twitch in curves
  const RoadPt &a = RP(q.i), &b = RP(q.nx); double t = q.t;
  double d[3] = {b.p[0] - a.p[0], b.p[1] - a.p[1], b.p[2] - a.p[2]};
  double L = d_sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  double ta[3], tb[3];
  double ca = L > 1 ? (a.fw[0] * d[0] + a.fw[1] * d[1] + a.fw[2] * d[2]) / L : 0, cb = L > 1 ? (b.fw[0] * d[0] + b.fw[1] * d[1] + b.fw[2] * d[2]) / L : 0;
  // (the camera test switch --camoff no longer changes this: road geometry for physics and AI is independent of debug views)
  for (int k = 0; k < 3; k++) { ta[k] = ca > 0.8 ? a.fw[k] * L : d[k]; tb[k] = cb > 0.8 ? b.fw[k] * L : d[k]; }
  double t2 = t * t, t3 = t2 * t;
  double h00 = 2 * t3 - 3 * t2 + 1, h10 = t3 - 2 * t2 + t, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
  for (int k = 0; k < 3; k++) {
    p[k] = h00 * a.p[k] + h10 * ta[k] + h01 * b.p[k] + h11 * tb[k];
    rt[k] = a.rt[k] + (b.rt[k] - a.rt[k]) * t; up[k] = a.up[k] + (b.up[k] - a.up[k]) * t; fw[k] = a.fw[k] + (b.fw[k] - a.fw[k]) * t;
  }
  double *v[3] = {rt, up, fw};
  for (int j = 0; j < 3; j++) { double l = d_sqrt(v[j][0] * v[j][0] + v[j][1] * v[j][1] + v[j][2] * v[j][2]); if (l > 1e-9) for (int k = 0; k < 3; k++) v[j][k] /= l; }
  if (w) *w = a.w + (b.w - a.w) * t;
}
static void pp_world(const PPos &q, double x, double h, double out[3]) {
  double p[3], rt[3], up[3], fw[3]; pp_frame(q, p, rt, up, fw, 0);
  for (int k = 0; k < 3; k++) out[k] = p[k] + rt[k] * x + up[k] * h;
}
// PST +0x48 is the distance from the road centre to the boundary (half the road width; M.EXE)
static double pp_halfw(const PPos &q) { double f = A.halfw_pct ? A.halfw_pct / 100.0 : 1.0; return f * (RP(q.i).w + (RP(q.nx).w - RP(q.i).w) * q.t); }
static double pp_kappa(const PPos &q) {   // heading change of the road per world unit on this segment
  const RoadPt &a = RP(q.i), &b = RP(q.nx);
  double c[3] = {a.fw[1] * b.fw[2] - a.fw[2] * b.fw[1], a.fw[2] * b.fw[0] - a.fw[0] * b.fw[2], a.fw[0] * b.fw[1] - a.fw[1] * b.fw[0]};
  double sn = c[0] * a.up[0] + c[1] * a.up[1] + c[2] * a.up[2], cs = a.fw[0] * b.fw[0] + a.fw[1] * b.fw[1] + a.fw[2] * b.fw[2];
  return d_atan2(sn, cs) / seg_len(q.i, q.nx);
}
static double pp_M(const PPos &q) {         // position along the main loop (for laps and ranking)
  double a = RP(q.i).M, b = RP(q.nx).M, d = b - a;
  if (d > TK.L * 0.5) d -= TK.L; if (d < -TK.L * 0.5) d += TK.L;
  return a + d * q.t;
}
static double wrapM(double d) { while (d > TK.L * 0.5) d -= TK.L; while (d < -TK.L * 0.5) d += TK.L; return d; }
// lateral offset of a world point relative to road point i
static double lat_at(int i, const double w[3]) { const RoadPt &r = RP(i); return (w[0] - r.p[0]) * r.rt[0] + (w[1] - r.p[1]) * r.rt[1] + (w[2] - r.p[2]) * r.rt[2]; }
// at a fork, take the lane whose centre is closest to the given world position
// does the lane starting at point n have video footage? (some fork lanes have none: they cannot be driven)
static int lane_has_video(int n) {
  if (!TK.fok) return 1;
  int ok = 0;
  for (int k = 0; k < 6 && n >= 0 && n < TK.nroad; k++) { if (n < TK.nframes && TK.fok[n]) ok++; n = TK.road[n].nx[0]; }
  return ok >= 3;
}
static int pick_next(int i, const double w[3]) {
  const RoadPt &r = RP(i);
  if (r.nx[0] == r.nx[1] || r.nx[1] < 0) return r.nx[0];
  int v0 = lane_has_video(r.nx[0]), v1 = lane_has_video(r.nx[1]);
  if (v0 != v1) return v0 ? r.nx[0] : r.nx[1];
  double a = d_abs(lat_at(r.nx[0], w)), b = d_abs(lat_at(r.nx[1], w));
  return a <= b ? r.nx[0] : r.nx[1];
}
static int pick_prev(int i, const double w[3]) {
  const RoadPt &r = RP(i);
  if (r.pv[0] == r.pv[1] || r.pv[1] < 0) return r.pv[0];
  double a = d_abs(lat_at(r.pv[0], w)), b = d_abs(lat_at(r.pv[1], w));
  return a <= b ? r.pv[0] : r.pv[1];
}
// move along the road by dist world units (negative = backwards); x (lateral) is kept in world space.
// on_enter is called with each new point entered going forward (for the camera history).
static void pp_move(PPos &q, double &x, double dist, void (*on_enter)(int, int) = 0, int tag = 0) {
  double w[3];
  int guard = 0;
  q.t += dist / seg_len(q.i, q.nx);
  while ((q.t >= 1 || q.t < 0) && guard++ < 16) {
    double t = q.t; q.t = t >= 1 ? 1 : 0;
    pp_world(q, x, 0, w);
    if (t >= 1) {
      double rest = (t - 1) * seg_len(q.i, q.nx);
      q.i = q.nx; q.nx = pick_next(q.i, w);
      x = lat_at(q.i, w);
      q.t = rest / seg_len(q.i, q.nx);
      if (on_enter) on_enter(q.i, tag);
    } else {
      double rest = t * seg_len(q.i, q.nx);   // negative
      int pv = pick_prev(q.i, w);
      q.nx = q.i; q.i = pv;
      x = lat_at(q.i, w) ; // close enough (re-projected on the new segment start)
      q.t = 1 + rest / seg_len(q.i, q.nx);
    }
  }
  if (q.t < 0) q.t = 0; if (q.t > 1) q.t = 1;
}
static PPos pp_make(int i, double t) { PPos q; q.i = i; q.nx = RP(i).nx[0]; q.t = t; return q; }

static int v3d_visible(int frame, int pt) {
  if (!TK.v3d || frame < 0 || frame >= TK.nv3d) return 1;
  uint32_t a = rd32(TK.v3d + frame * 4), b = rd32(TK.v3d + (frame + 1) * 4);
  if (b > TK.v3d_size || a > b) return 1;
  for (uint32_t o = a; o + 4 <= b; o += 4) {
    int lo = (int16_t)rd16(TK.v3d + o), hi = (int16_t)rd16(TK.v3d + o + 2);
    if (pt >= lo && pt <= hi) return 1;
  }
  return 0;
}

static Model *load_model_from(const char *set, const char *file) {
  char dir[512]; str_cpy(dir, g_gamedir, 512); str_cat(dir, "/CARS/", 512); str_cat(dir, set, 512);
  if (file[0] == '*') { file++; str_cpy(dir, g_gamedir, 512); str_cat(dir, "/OBJS/TIB", 512); }   // shop items and bonus objects
  Model *m = (Model *)sys_alloc(sizeof(Model));
  char f[40]; str_cpy(f, file, 40);
  if (!str_icontains(f, ".M3D")) str_cat(f, ".M3D", 40);
  if (!model_load(m, dir, f)) { sys_free(m); return 0; }
  return m;
}
static Model *track_car_model(int car) {
  if (car < 0 || car >= NCARS) return 0;
  if (!TK.car_models[car]) { TK.car_models[car] = load_model_from(TRACKS[TK.idx].set, CARS[car].model);
    Model *m = TK.car_models[car]; if (sys_debug_enabled() && m && A.demo_time > 0) { char t[160] = "car model "; str_cat(t, CARS[car].model, 160); str_cat(t, " size x/y/z = ", 160);
      str_int(t, 160, (long)((m->bmax.x - m->bmin.x) * 1000)); str_cat(t, " / ", 160); str_int(t, 160, (long)((m->bmax.y - m->bmin.y) * 1000)); str_cat(t, " / ", 160); str_int(t, 160, (long)((m->bmax.z - m->bmin.z) * 1000)); sys_log(t); } }
  return TK.car_models[car];
}

static void track_free(void) {
  spr_free(&TK.hud);
  if (TK.vid.data) video_close(&TK.vid);
  sys_free(TK.cam); sys_free(TK.cam_ubb); sys_free(TK.road); sys_free(TK.v3d); sys_free(TK.fpt); sys_free(TK.fnx); sys_free(TK.frun); sys_free(TK.ft); sys_free(TK.frlist); sys_free(TK.fok);
  for (int i = 0; i < NCARS; i++) if (TK.car_models[i]) { model_free(TK.car_models[i]); sys_free(TK.car_models[i]); }
  for (int i = 0; i < OM_COUNT; i++) if (TK.obj_models[i]) { model_free(TK.obj_models[i]); sys_free(TK.obj_models[i]); }
  music_stop(); sound_free(&TK.music);
  GLuint t = TK.vtex; mem_zero(&TK, sizeof TK); TK.vtex = t;
}

struct NameList { char n[64][40]; int c; const char *suffix; };
static void cb_names(void *u, const char *name, uint32_t size) {
  (void)size; NameList *l = (NameList *)u;
  size_t ln = str_len(name), ls = str_len(l->suffix);
  if (l->c < 64 && ln > ls && !str_icmp(name + ln - ls, l->suffix)) str_cpy(l->n[l->c++], name, 40);
}
static void sort40(char (*a)[40], int n) {
  for (int i = 1; i < n; i++) { char t[40]; str_cpy(t, a[i], 40); int j = i - 1; while (j >= 0 && str_icmp(a[j], t) > 0) { str_cpy(a[j + 1], a[j], 40); j--; } str_cpy(a[j + 1], t, 40); }
}
static void load_music_dir(Sound *out, const char *dir, const char *suffix) {
  NameList nl; nl.c = 0; nl.suffix = suffix;
  iso_list(dir, cb_names, &nl);
  sort40(nl.n, nl.c);
  int cap = 0; mem_zero(out, sizeof *out); out->ch = 1;
  for (int i = 0; i < nl.c; i++) {
    char mp[128]; str_cpy(mp, dir, 128); str_cat(mp, "/", 128); str_cat(mp, nl.n[i], 128);
    size_t mn; uint8_t *md = iso_load(mp, &mn); if (!md) continue;
    int fr, ch, rate; int16_t *pcm = wav_decode(md, mn, &fr, &ch, &rate); sys_free(md);
    if (!pcm) continue;
    if (out->frames + fr > cap) { cap = (out->frames + fr) * 2; out->pcm = (int16_t *)sys_realloc(out->pcm, (size_t)cap * 2); }
    for (int k = 0; k < fr; k++) out->pcm[out->frames + k] = ch == 2 ? (int16_t)((pcm[k * 2] + pcm[k * 2 + 1]) / 2) : pcm[k];
    out->frames += fr; sys_free(pcm);
  }
}

static double seg_dist2(int i, int nx, const double c[3], double *tt) {   // squared distance of c to segment i->nx
  const RoadPt &a = RP(i), &b = RP(nx);
  double sx = b.p[0] - a.p[0], sy = b.p[1] - a.p[1], sz = b.p[2] - a.p[2], L2 = sx * sx + sy * sy + sz * sz, t = 0;
  if (L2 > 0) t = ((c[0] - a.p[0]) * sx + (c[1] - a.p[1]) * sy + (c[2] - a.p[2]) * sz) / L2;
  t = d_clamp(t, 0, 1);
  double dx = a.p[0] + sx * t - c[0], dy = a.p[1] + sy * t - c[1], dz = a.p[2] + sz * t - c[2];
  *tt = t; return dx * dx + dy * dy + dz * dz;
}

// world position of a road object: its node's PST frame, original local values plus the movement after hits
static void obj_update_world(TrackObj &o) {
  const RoadPt &q = TK.road[o.node];
  double lon = o.lon + o.dlon, lat = o.lat + o.dlat, h = o.h + o.hz;
  for (int k = 0; k < 3; k++) o.W[k] = q.p[k] + q.fw[k] * lon + q.rt[k] * lat + q.up[k] * h;
}
static uint8_t *g_preload_ubb; static size_t g_preload_n; static int g_preload_track = -1;   // track video read beforehand (loading bar)
static int track_load(int ti, int heat) {
  if(sys_debug_enabled()){char event[160]="[event] Loading track: ";str_cat(event,ui_debug_text(TRACKS[ti].name),sizeof event);str_cat(event," heat ",sizeof event);str_int(event,sizeof event,heat+1);sys_log(event);}
  if (TK.loaded && TK.idx == ti) { TK.heat = heat; }
  else {
    track_free();
    TK.idx = ti; TK.heat = heat;
    const TrackDef &td = TRACKS[ti];
    char p[128]; size_t n;
    str_cpy(p, td.cd, 128); str_cat(p, ".SPR", 128);
    if (!spr_load_cd(&TK.hud, p) || TK.hud.n != 6) return 0;
    str_cpy(p, td.cd, 128); str_cat(p, ".HLZ", 128);
    uint8_t *hud_data = iso_load(p, &n);
    if (!hud_data || n < 768) { sys_free(hud_data); return 0; }
    mem_cpy(TK.hud_pal, hud_data, 768); sys_free(hud_data);
    hud_data = 0;
    // Original loader 0x13e09: PAL_RACE supplies the five reserved HUD colors 251..255.
    char hud_path[512]; game_path(hud_path, "DATA640/PAL_RACE.HLZ");
    hud_data = sys_read_file(hud_path, &n);
    if (!hud_data || n < 768) { sys_free(hud_data); return 0; }
    mem_cpy(TK.hud_pal + 251 * 3, hud_data + 251 * 3, 5 * 3); sys_free(hud_data);
    str_cpy(p, td.cd, 128); str_cat(p, ".UBB", 128);
    uint8_t *d;
    if (g_preload_ubb && g_preload_track == ti) { d = g_preload_ubb; n = g_preload_n; g_preload_ubb = 0; g_preload_track = -1; }
    else d = iso_load(p, &n);
    if (!d || !video_open(&TK.vid, d, n, 350,0)) { str_cpy(A.status, ui_translate("Streckenvideo nicht lesbar"), 256); return 0; }
    str_cpy(p, td.cd, 128); str_cat(p, ".CAM", 128);
    d = iso_load(p, &n); if (!d) return 0;
    TK.ncam = (int)(n / 64); TK.cam = (double *)sys_alloc(sizeof(double) * 12 * TK.ncam);
    for (int i = 0; i < TK.ncam; i++) {
      const uint8_t *c = d + i * 64;
      for (int k = 0; k < 3; k++) TK.cam[i * 12 + k] = (s32)rd32(c + k * 4);
      for (int k = 0; k < 9; k++) TK.cam[i * 12 + 3 + k] = (s32)rd32(c + 24 + k * 4) / 32768.0;
    }
    TK.cam_ubb = (uint32_t *)sys_alloc(sizeof(uint32_t) * (TK.ncam + 1));
    for (int i = 0; i < TK.ncam; i++) TK.cam_ubb[i] = rd32(d + i * 64 + 16);
    sys_free(d);
    str_cpy(p, td.cd, 128); str_cat(p, ".PST", 128);
    d = iso_load(p, &n); if (!d) return 0;
    TK.nroad = (int)(n / 128); TK.road = (RoadPt *)sys_alloc(sizeof(RoadPt) * TK.nroad);
    for (int i = 0; i < TK.nroad; i++) {
      const uint8_t *r = d + i * 128; RoadPt &q = TK.road[i];
      for (int k = 0; k < 3; k++) {
        q.p[k] = (s32)rd32(r + 12 + k * 4); q.rt[k] = (s32)rd32(r + 36 + k * 4) / 32768.0;
        q.up[k] = (s32)rd32(r + 48 + k * 4) / 32768.0; q.fw[k] = (s32)rd32(r + 60 + k * 4) / 32768.0;
      }
      for(int branch=0;branch<2;branch++){q.physics_turn[branch]=(s32)rd32(r+76+branch*20);q.physics_length[branch]=(s32)rd32(r+80+branch*20);q.physics_rotation[branch]=(s32)rd32(r+84+branch*20);}
      q.w = (s32)rd32(r + 72); if (q.w <= 0 || q.w > 200000) q.w = 25000;
      q.pv[0] = (s32)rd32(r + 88); q.nx[0] = (s32)rd32(r + 92); q.pv[1] = (s32)rd32(r + 108); q.nx[1] = (s32)rd32(r + 112);
      q.zone = (int)(rd32(r + 116) >> 24);
      q.kind = (int)(rd32(r) & 0x7fff); q.mobflag = (rd16(r) & 0x8000) ? 1 : 0;
      q.anc_f = rd16(r + 8); q.anc_r = rd16(r + 10);
    }
    sys_free(d);
    for (int i = 0; i < TK.nroad; i++) {   // sanitize links, segment lengths
      RoadPt &q = TK.road[i];
      for (int k = 0; k < 2; k++) {
        if (q.nx[k] < 0 || q.nx[k] >= TK.nroad) q.nx[k] = k ? q.nx[0] : (i + 1) % TK.nroad;
        if (q.pv[k] < 0 || q.pv[k] >= TK.nroad) q.pv[k] = k ? q.pv[0] : (i + TK.nroad - 1) % TK.nroad;
      }
      for (int k = 0; k < 2; k++) {
        const RoadPt &b = TK.road[q.nx[k]];
        double dx = b.p[0] - q.p[0], dy = b.p[1] - q.p[1], dz = b.p[2] - q.p[2];
        q.len[k] = d_sqrt(dx * dx + dy * dy + dz * dz); if (q.len[k] < 100) q.len[k] = 6900;
      }
    }
    // main loop 0..P and the position of every lane point along it
    int P = TK.road[0].pv[0]; if (P <= 0 || P >= TK.nroad) P = TK.nroad - 1;
    TK.L = P + 1;
    for (int i = 0; i < TK.nroad; i++) {
      RoadPt &q = TK.road[i];
      if (i <= P) { q.M = i; continue; }
      double db = 0, df = 0; int a = i, b = i, g = 0;
      while (a > P && g++ < 2000) { int pv = TK.road[a].pv[0]; db += TK.road[pv].len[TK.road[pv].nx[0] == a ? 0 : 1]; a = pv; }
      g = 0;
      while (b > P && g++ < 2000) { df += TK.road[b].len[0]; b = TK.road[b].nx[0]; }
      double span = b - a; if (span < 0) span += TK.L;
      q.M = a + span * (db / (db + df > 1 ? db + df : 1));
      if (q.M >= TK.L) q.M -= TK.L;
    }
    str_cpy(p, td.cd, 128); str_cat(p, ".V3D", 128);
    TK.v3d = iso_load(p, &TK.v3d_size);
    if (TK.v3d && TK.v3d_size >= 4) TK.nv3d = (int)(rd32(TK.v3d) / 4) - 1;
    // the road point of each frame's camera (continuity along the graph, cuts between footage runs)
    TK.nframes = TK.vid.nframes < TK.ncam ? TK.vid.nframes : TK.ncam;
    TK.fpt = (int *)sys_alloc(sizeof(int) * (TK.nframes + 1)); TK.fnx = (int *)sys_alloc(sizeof(int) * (TK.nframes + 1));
    TK.frun = (int *)sys_alloc(sizeof(int) * (TK.nframes + 1)); TK.ft = (double *)sys_alloc(sizeof(double) * (TK.nframes + 1));
    // Video frame k shows the view for a car at road point k (frames beyond the last point are a fly-over).
    // Its camera sits some points behind on the road; find that point (continuity within a footage run,
    // a new run starts where the camera position jumps).
    if (TK.nframes > TK.nroad) TK.nframes = TK.nroad;
    int run = 0;
    for (int f = 0; f < TK.nframes; f++) {
      const double *c = TK.cam + f * 12;
      if (f > 0) { const double *c0 = TK.cam + (f - 1) * 12; double dx = c[0] - c0[0], dy = c[1] - c0[1], dz = c[2] - c0[2]; if (dx * dx + dy * dy + dz * dz > 20000.0 * 20000.0) run++; }
      // walk back from point f along the road that leads to it
      double best = 1e300, bt = 0; int bi = f, bnx = TK.road[f].nx[0];
      int j = f;
      for (int g = 0; g < 30; g++) {
        int pv = TK.road[j].pv[0];
        double t, d2 = seg_dist2(pv, j, c, &t);
        if (d2 < best) { best = d2; bi = pv; bnx = j; bt = t; }
        j = pv;
      }
      TK.fpt[f] = bi; TK.fnx[f] = bnx; TK.ft[f] = bt; TK.frun[f] = run;
    }
    // frames per road point (only frames whose camera looks along the road; crane shots and fly-overs are left out)
    TK.fok = (uint8_t *)sys_alloc(TK.nframes + 1);
    for (int f = 0; f < TK.nframes; f++) {
      const double *c = TK.cam + f * 12; const RoadPt &r = TK.road[TK.fpt[f]];
      double dt = c[9] * r.fw[0] + c[10] * r.fw[1] + c[11] * r.fw[2];
      double dx = c[0] - r.p[0], dy = c[1] - r.p[1], dz = c[2] - r.p[2];
      TK.fok[f] = dt > 0.6 && dx * dx + dy * dy + dz * dz < 45000.0 * 45000.0;
    }
    for (int f = 0; f < TK.nframes; f++) if (TK.fok[f]) TK.road[TK.fpt[f]].nfr++;
    int acc = 0; for (int i = 0; i < TK.nroad; i++) { TK.road[i].fr0 = acc; acc += TK.road[i].nfr; TK.road[i].nfr = 0; }
    TK.frlist = (int *)sys_alloc(sizeof(int) * (TK.nframes + 1));
    for (int f = 0; f < TK.nframes; f++) if (TK.fok[f]) { RoadPt &r = TK.road[TK.fpt[f]]; TK.frlist[r.fr0 + r.nfr++] = f; }
    if(sys_debug_enabled()){ int none = 0; for (int i = 0; i < TK.nroad; i++) if (!TK.road[i].nfr) none++;
      char m[160] = "track: points "; str_int(m, 160, TK.nroad); str_cat(m, " main ", 160); str_int(m, 160, TK.L); str_cat(m, " frames ", 160); str_int(m, 160, TK.nframes);
      str_cat(m, " runs ", 160); str_int(m, 160, TK.frun[TK.nframes - 1] + 1); str_cat(m, " points without footage ", 160); str_int(m, 160, none); sys_log(m); }
    if(sys_debug_enabled()){   // diagnosis: does video frame f start at the UBB offset of CAM[f]? (the original seeks with CAM +0x10)
      int bad = 0, cnt = 0; long base = TK.vid.frame_ofs ? (long)TK.vid.frame_ofs[0] - (long)TK.cam_ubb[0] : 0;
      for (int f = 0; f < TK.vid.nframes && f < TK.ncam; f++) { cnt++; if ((long)TK.vid.frame_ofs[f] - (long)TK.cam_ubb[f] != base) bad++; }
      char m[200] = "video/CAM: frames "; str_int(m, 200, TK.vid.nframes); str_cat(m, " CAM ", 200); str_int(m, 200, TK.ncam);
      str_cat(m, " offset base ", 200); str_int(m, 200, base); str_cat(m, " mismatches ", 200); str_int(m, 200, bad); str_cat(m, "/", 200); str_int(m, 200, cnt); sys_log(m);
    }
    for (int i = 0; i < OM_COUNT; i++) TK.obj_models[i] = load_model_from(td.set, OBJ_MODEL_FILES[i]);
    char md[64] = "MUSIC/"; str_cat(md, td.music, 64);
    if(audio_enabled(AUDIO_RACE_MUSIC))load_music_dir(&TK.music, md, ".16");
    TK.loaded = 1;
  }
  // objects of this race: 16-byte header (total, count type 1, count type 2, count type 8), then 12-byte records
  // (PST node, local longitudinal, local lateral) in this type order (M.EXE 0x232A9 ff.). The height comes from
  // CORRECTION_Y_CIRCUIT, the world position from the node's own PST frame.
  TK.nobj = 0;
  {
    char p[128]; size_t n; str_cpy(p, TRACKS[ti].cd, 128); str_cat(p, heat == 0 ? ".1" : heat == 1 ? ".2" : ".3", 128);
    uint8_t *d = iso_load(p, &n);
    if (d && n >= 16) {
      int total = (s32)rd32(d), c1 = (s32)rd32(d + 4), c2 = (s32)rd32(d + 8), c8 = (s32)rd32(d + 12);
      if(sys_debug_enabled()){   // check the header against the data (the import must not silently drop objects)
        char m[200] = "objects "; str_cat(m, p, 200); str_cat(m, ": ", 200); str_int(m, 200, total);
        int bad = 0;
        if (total < 0 || c1 < 0 || c2 < 0 || c8 < 0 || c1 + c2 + c8 != total) { str_cat(m, " HEADER MISMATCH (type counts)", 200); bad = 1; }
        if (16 + (long)total * 12 > (long)n) { str_cat(m, " FILE TOO SHORT", 200); bad = 1; }
        if (total > MAXOBJ) { str_cat(m, " MORE THAN MAXOBJ", 200); bad = 1; }
        if (bad || A.demo_time > 0) sys_log(m);
      }
      for (int i = 0; i < total && TK.nobj < MAXOBJ && 16 + i * 12 + 12 <= (int)n; i++) {
        const uint8_t *r = d + 16 + i * 12;
        int pt = (s32)rd32(r); if (pt < 0 || pt >= TK.nroad) { sys_log("objects: invalid road node, record skipped"); continue; }
        TrackObj &o = TK.obj[TK.nobj++]; mem_zero(&o, sizeof o);
        o.type = i < c1 ? 1 : i < c1 + c2 ? 2 : 8;
        o.node = pt; o.lon = (s32)rd32(r + 4); o.lat = (s32)rd32(r + 8); o.h = CORRECTION_Y_CIRCUIT[ti % 6];
        const RoadPt &q = TK.road[pt];
        o.node0 = o.node; o.lon0 = o.lon; o.lat0 = o.lat;
        obj_update_world(o);
        // road position for the game logic (collisions, visibility)
        o.pp = pp_make(pt, d_clamp(o.lon / seg_len(pt, q.nx[0]), 0, 0.999)); o.x = o.lat; o.angle = 0;
        o.kind = o.type == 8 ? 2 : 1; o.alive = 1; o.variant = 0;
      }
    }
    sys_free(d);
  }
  return 1;
}

// ------------------------------------------------------------------ 3D helpers (all in original world units, output scaled 1/1000)
static M4 g_proj; static double g_proj_yoff = 0;   // vertical offset of the projection centre (video lines); the original has none
static void track_proj(void) {
  mem_zero(&g_proj, sizeof g_proj);
  float zn = 0.5f, zf = 4000.0f;
  // original projection (M.EXE 0x56B40): F = 407 on a 320 x 350 viewport, the depth is doubled before the division
  float fs = A.focal_pct ? A.focal_pct / 100.0f : 0.5f;
  g_proj.m[0] = fs * 407.0f / 160.0f; g_proj.m[5] = fs * 407.0f * 5.0f / 3.0f / 175.0f;
  g_proj.m[9] = (float)(g_proj_yoff / 175.0);   // vertical offset of the projection centre (video lines, + = down)
  g_proj.m[10] = (zf + zn) / (zn - zf); g_proj.m[11] = -1; g_proj.m[14] = 2 * zf * zn / (zn - zf);
}
// eye-space depth of a world point for the given camera (positive = in front)
static double cam_depth(const double *cam, const double p[3]) { return (p[0] - cam[0]) * cam[9] + (p[1] - cam[1]) * cam[10] + (p[2] - cam[2]) * cam[11]; }
// model -> eye: model axes X,Y,Z map to world vectors R,U,F at world position pos
static M4 world_mv(const double *cam, const double pos[3], const double R[3], const double U[3], const double F[3], double scale) {
  const double *R0 = cam + 3, *R1 = cam + 6, *R2 = cam + 9;
  double d[3] = {pos[0] - cam[0], pos[1] - cam[1], pos[2] - cam[2]};
  const double *cols[3] = {R, U, F};
  M4 m; mem_zero(&m, sizeof m);
  for (int c = 0; c < 3; c++) {
    const double *v = cols[c];
    m.m[c * 4 + 0] = (float)((R0[0] * v[0] + R0[1] * v[1] + R0[2] * v[2]) * scale);
    m.m[c * 4 + 1] = (float)((R1[0] * v[0] + R1[1] * v[1] + R1[2] * v[2]) * scale);
    m.m[c * 4 + 2] = (float)(-(R2[0] * v[0] + R2[1] * v[1] + R2[2] * v[2]) * scale);
  }
  m.m[12] = (float)((R0[0] * d[0] + R0[1] * d[1] + R0[2] * d[2]) * MODEL_SCALE);
  m.m[13] = (float)((R1[0] * d[0] + R1[1] * d[1] + R1[2] * d[2]) * MODEL_SCALE);
  m.m[14] = (float)(-(R2[0] * d[0] + R2[1] * d[1] + R2[2] * d[2]) * MODEL_SCALE);
  m.m[15] = 1;
  return m;
}
// world placement on the road: position (s, lateral x, height h) and heading psi relative to the road
// world placement on the road graph: position (q, lateral x, height h), heading psi relative to the road.
// g_shift (world units) is added to everything drawn in the race: it compensates the step of the video frame.
static double g_shift[3];
static double g_roll;   // roll angle for the next road_mv (rad)
static M4 road_mv(const double *cam, const PPos &q, double x, double h, double psi, double scale, double out_pos[3]) {
  double p[3], rt[3], up[3], fw[3], w; pp_frame(q, p, rt, up, fw, &w);
  double cs = d_cos(psi), sn = d_sin(psi), R[3], F[3];
  for (int k = 0; k < 3; k++) { F[k] = fw[k] * cs + rt[k] * sn; R[k] = rt[k] * cs - fw[k] * sn; p[k] += rt[k] * x + up[k] * h + g_shift[k]; }
  if (g_roll != 0) {   // visual roll of the body (cars only)
    double cr = d_cos(g_roll), sr = d_sin(g_roll), R2[3], U2[3];
    for (int k = 0; k < 3; k++) { R2[k] = R[k] * cr + up[k] * sr; U2[k] = up[k] * cr - R[k] * sr; }
    if (out_pos) for (int k = 0; k < 3; k++) out_pos[k] = p[k];
    return world_mv(cam, p, R2, U2, F, scale);
  }
  if (out_pos) for (int k = 0; k < 3; k++) out_pos[k] = p[k];
  return world_mv(cam, p, R, up, F, scale);
}
static void draw_model_mv(const Model &m, const M4 &mv, float wheel, int frame) {
  if (m.kind == KIND_FRAMES) {
    int f = frame % (m.nn ? m.nn : 1); if (f < 0) f = 0;
    const Mesh &fm = m.nodes[f].lod[0];
    if (!fm.ok) return;
    V3 c = (fm.bmin + fm.bmax) * 0.5f;
    M4 mm = m4_mul(mv, m4_trans(v3(-c.x, -c.y, -c.z)));
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE); glDepthMask(GL_FALSE);
    draw_mesh(m, fm, mm, 2);
    glDisable(GL_BLEND); glDepthMask(GL_TRUE);
    return;
  }
  for (int pass = 0; pass < (g_shadow_pass?1:3); pass++) {
    if (pass == 1) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); }
    if (pass == 2) { glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE); glDepthMask(GL_FALSE); }
    for (int i = 0; i < m.nn; i++) {
      const Node &nd = m.nodes[i];
      if (!nd.lod[0].ok) continue;
      V3 off = v3(0, 0, 0);
      for (int j = i, g = 0; j >= 0 && g < 32; j = m.nodes[j].parent, g++) off = off + m.nodes[j].pos;
      glUniform3f(R.um_damage_offset,off.x,off.y,off.z);glUniform1i(R.um_lamp_node,!nd.spin);
      M4 mm = m4_mul(mv, m4_trans(off));
      if (nd.spin) mm = m4_mul(mm, m4_rotx(wheel));
      draw_mesh(m, nd.lod[0], mm, pass);
    }
    glDisable(GL_BLEND); glDepthMask(GL_TRUE);
  }
}
static void model_shader_begin(const M4 &proj, float gain) {
  glUseProgram(R.prog_model);
  glUniform3f(R.um_visual_strength,g_effect_strength[0],g_effect_strength[1],g_effect_strength[2]);glUniform1f(R.um_damage_strength,g_effect_strength[4]);glUniform1f(R.um_damage,0);glUniform1i(R.um_lamp_mask,2);glUniform1i(R.um_lamp_node,0);
  glUniform1i(R.um_rear_only,0);glUniform1f(R.um_trail_weight,1);
  glUniform1i(R.um_lamps,0);glUniform1i(R.um_lamp_pass,0);
  glUniform1i(R.um_shadow_enabled,0);glUniform1i(R.um_shadow_pass,0);glUniform1i(R.um_oil,0);glUniform3f(R.um_light,0.45f,0.85f,0.35f);
  glUniformMatrix4fv(R.um_vp, 1, GL_FALSE, proj.m);
  glUniform3f(R.um_cam, 0, 0, 0); glUniform1i(R.um_tex, 0); glUniform1f(R.um_lit, 1.0f); glUniform1f(R.um_gain, gain); glUniform1f(R.um_gray, g_gray);
  glActiveTexture(GL_TEXTURE0);
}
