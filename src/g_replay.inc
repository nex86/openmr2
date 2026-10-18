// ===================================================================================================
// Replays ("Wiedergabe"): every race is recorded (one sample per original update with its interval: video frame,
// camera, cars, missiles, mines, explosions) and can be watched again from the pause menu or the replay screen.
// The last races are kept in the files mr2gl_replay_NN.dat (20 slots, as in the original menu).
// ===================================================================================================

enum { RP_SLOTS = 20 };
struct RpHeader { char magic[8]; int track, heat, ncar, laps, nframes, cars[8], place, size; };
struct RpBuf { uint8_t *d; size_t n, cap; };
static RpBuf g_rec;             // recording of the running race
static RpHeader g_rec_h;
static double g_rec_next;


static void rp_put(RpBuf &b, const void *p, size_t n) {
  if (b.n + n > b.cap) { b.cap = (b.n + n) * 2 + 65536; b.d = (uint8_t *)sys_realloc(b.d, b.cap); }
  mem_cpy(b.d + b.n, p, n); b.n += n;
}
template <class T> static void rp_put(RpBuf &b, const T &v) { rp_put(b, &v, sizeof v); }
template <class T> static T rp_get(const uint8_t *&p) { T v; mem_cpy(&v, p, sizeof v); p += sizeof v; return v; }

static void rec_start(void) {
  g_rec.n = 0; g_rec_next = 0;
  mem_zero(&g_rec_h, sizeof g_rec_h); mem_cpy(g_rec_h.magic, "MR2RPL3", 8);
  g_rec_h.track = TK.idx; g_rec_h.heat = TK.heat; g_rec_h.ncar = RC.ncar; g_rec_h.laps = RC.laps;
  for (int i = 0; i < RC.ncar; i++) g_rec_h.cars[i] = RC.car[i].car;
}
static void rp_put_pp(RpBuf &b, const PPos &q) { rp_put(b, (uint16_t)q.i); rp_put(b, (uint16_t)q.nx); rp_put(b, (float)q.t); }
static PPos rp_get_pp(const uint8_t *&p) { PPos q; q.i = rp_get<uint16_t>(p); q.nx = rp_get<uint16_t>(p); q.t = rp_get<float>(p); return q; }

// one sample of the running race (after race_draw, so camera and frame are known)
static void rec_capture(void) {
  if (RC.phase == PH_FLYBY) return;
  RpBuf &b = g_rec;
  rp_put(b, (uint16_t)RC.frame);
  rp_put(b, (uint16_t)RC.delta_units);   // interval until the next update (version 2)
  for (int k = 0; k < 12; k++) rp_put(b, (float)RC.vcam[k]);
  rp_put(b, (float)RC.gain);
  for (int i = 0; i < RC.ncar; i++) {
    const RCar &c = RC.car[i];
    rp_put_pp(b, c.pp); rp_put(b, (float)c.x); rp_put(b, (float)c.psi); rp_put(b, (float)c.wheel);
    uint8_t fl = (c.out ? 1 : 0) | (c.flash > 0 && ((int)(RC.t * 20) & 1) ? 2 : 0); rp_put(b, fl);
  }
  uint8_t n = 0; for (int i = 0; i < 32; i++) if (RC.proj[i].alive) n++;
  rp_put(b, n);
  for (int i = 0; i < 32; i++) if (RC.proj[i].alive) { const Proj &p = RC.proj[i]; rp_put_pp(b, p.pp); rp_put(b, (float)p.x); rp_put(b, (float)p.psi); rp_put(b, (uint8_t)p.kind); }
  n = 0; for (int i = 0; i < 48; i++) if (RC.haz[i].alive) n++;
  rp_put(b, n);
  for (int i = 0; i < 48; i++) if (RC.haz[i].alive) { const Hazard &h = RC.haz[i]; rp_put_pp(b, h.pp); rp_put(b, (float)h.x); rp_put(b, (uint8_t)h.kind); }
  n = 0; for (int i = 0; i < 24; i++) if (RC.fx[i].alive) n++;
  rp_put(b, n);
  for (int i = 0; i < 24; i++) if (RC.fx[i].alive) { const Fx &f = RC.fx[i]; for (int k = 0; k < 3; k++) rp_put(b, (float)f.W[k]); rp_put(b, (uint8_t)f.model); rp_put(b, (float)f.t); rp_put(b, (float)f.scale); }
  uint64_t alive = 0; for (int i = 0; i < TK.nobj && i < 64; i++) if (TK.obj[i].alive) alive |= 1ull << i;
  rp_put(b, alive);
  rp_put(b, (uint8_t)player().place); rp_put(b, (uint8_t)((int)(player().prog / TK.L) + 1));
  // version 3: road objects that were pushed or thrown (position in their node's frame, turn and tumble)
  uint8_t nm = 0; for (int i = 0; i < TK.nobj && i < 64; i++) if (TK.obj[i].touched) nm++;
  rp_put(b, nm);
  for (int i = 0; i < TK.nobj && i < 64; i++) if (TK.obj[i].touched) { const TrackObj &o = TK.obj[i];
    rp_put(b, (uint8_t)i); rp_put(b, (uint16_t)o.node); rp_put(b, (float)(o.lon + o.dlon)); rp_put(b, (float)(o.lat + o.dlat)); rp_put(b, (float)o.hz); rp_put(b, (float)o.yaw); rp_put(b, (float)o.pitch); }
  g_rec_h.nframes++;
}

// --- playback
struct Replay { RpHeader h; uint8_t *d; size_t n; uint32_t *ofs; double *at; int pos_frame; double t; int paused, own; };
static Replay RPL;
// explicit format versions: "MR2RPL1" (25 samples/s), "MR2RPL2" (+ interval per update), "MR2RPL3" (+ moved objects)
static int replay_version_of(const RpHeader &h) {
  static const char base[7] = {'M', 'R', '2', 'R', 'P', 'L', 0};
  for (int i = 0; i < 6; i++) if (h.magic[i] != base[i]) return 0;
  if (h.magic[7] != 0 && h.magic[7] != ' ') return 0;
  int v = h.magic[6] - '0';
  return v >= 1 && v <= 3 ? v : 0;   // unknown (future) versions are refused, not read as version 3
}
static int replay_header_ok(const RpHeader &h) {
  if (!replay_version_of(h)) return 0;
  if (h.ncar < 1 || h.ncar > 8 || h.track < 0 || h.track > 5 || h.heat < 0 || h.heat > 2 || h.nframes < 0 || h.nframes > 2000000) return 0;
  for (int i = 0; i < h.ncar; i++) if (h.cars[i] < 0 || h.cars[i] >= NCARS) return 0;
  return 1;
}
static int replay_v2(void) { return replay_version_of(RPL.h) >= 2; }
static int replay_v3(void) { return replay_version_of(RPL.h) >= 3; }
// index of all complete samples; every list length is checked against the remaining data before it is used,
// an incomplete last sample is dropped (a damaged file cannot make the program read past its end)
static void replay_index(void) {
  sys_free(RPL.ofs); RPL.ofs = 0; sys_free(RPL.at); RPL.at = 0;
  if (!replay_header_ok(RPL.h)) { RPL.h.nframes = 0; return; }
  RPL.ofs = (uint32_t *)sys_alloc(sizeof(uint32_t) * (RPL.h.nframes + 1));
  RPL.at = (double *)sys_alloc(sizeof(double) * (RPL.h.nframes + 1));
  const uint8_t *p = RPL.d, *e = RPL.d + RPL.n; int k = 0; double t = 0;
  int v = replay_version_of(RPL.h);
  auto need = [&](size_t bytes) { return (size_t)(e - p) >= bytes; };
  while (k < RPL.h.nframes) {
    const uint8_t *start = p; double dt = 1.0 / 25;
    size_t fixed = 2 + (v >= 2 ? 2 : 0) + 48 + 4 + (size_t)RPL.h.ncar * (8 + 13);
    if (!need(fixed)) break;
    if (v >= 2) { uint16_t du; mem_cpy(&du, p + 2, 2); if (du == 0 || du > 8192) break; dt = du / 51200.0; }
    p += fixed;
    if (!need(1)) break; int n = *p++; if (n > 32 || !need((size_t)n * 17)) break; p += n * 17;   // projectiles
    if (!need(1)) break; n = *p++; if (n > 48 || !need((size_t)n * 13)) break; p += n * 13;        // mines / oil
    if (!need(1)) break; n = *p++; if (n > 24 || !need((size_t)n * 21)) break; p += n * 21;        // explosions
    if (!need(8 + 2)) break; p += 8 + 2;                                                          // objects alive, place, lap
    if (v >= 3) { if (!need(1)) break; n = *p++; if (n > 64 || !need((size_t)n * 23)) break; p += n * 23; }   // moved objects
    RPL.at[k] = t; RPL.ofs[k++] = (uint32_t)(start - RPL.d); t += dt;
  }
  if (k < RPL.h.nframes) { char m[96] = "[warning] Replay: incomplete data; recovered frames: "; str_int(m, 96, k); sys_log(m); }
  RPL.h.nframes = k;
}
// apply one recorded sample to the race state so that race_draw can show it
static void replay_apply(int f) {
  if (f < 0) f = 0; if (f >= RPL.h.nframes) f = RPL.h.nframes - 1; if (f < 0) return;
  const uint8_t *p = RPL.d + RPL.ofs[f];
  RC.frame = rp_get<uint16_t>(p);
  if (replay_v2()) RC.delta_units = rp_get<uint16_t>(p);
  for (int k = 0; k < 12; k++) RC.vcam[k] = rp_get<float>(p);
  RC.gain = rp_get<float>(p);
  RC.ncar = RPL.h.ncar;
  for (int i = 0; i < RC.ncar; i++) {
    RCar &c = RC.car[i]; c.car = RPL.h.cars[i]; c.player = i == 0;
    c.pp = rp_get_pp(p); c.x = rp_get<float>(p); c.psi = rp_get<float>(p); c.wheel = rp_get<float>(p);
    uint8_t fl = rp_get<uint8_t>(p); c.out = fl & 1; c.flash = (fl & 2) ? 1 : 0;
    car_update_world(c);
  }
  for (int i = 0; i < 32; i++) RC.proj[i].alive = 0;
  int n = rp_get<uint8_t>(p);
  for (int i = 0; i < n; i++) { Proj &q = RC.proj[i]; q.alive = 1; q.pp = rp_get_pp(p); q.x = rp_get<float>(p); q.psi = rp_get<float>(p); q.kind = rp_get<uint8_t>(p); }
  for (int i = 0; i < 48; i++) RC.haz[i].alive = 0;
  n = rp_get<uint8_t>(p);
  for (int i = 0; i < n; i++) { Hazard &h = RC.haz[i]; h.alive = 1; h.pp = rp_get_pp(p); h.x = rp_get<float>(p); h.kind = rp_get<uint8_t>(p); }
  for (int i = 0; i < 24; i++) RC.fx[i].alive = 0;
  n = rp_get<uint8_t>(p);
  for (int i = 0; i < n; i++) { Fx &x = RC.fx[i]; x.alive = 1; for (int k = 0; k < 3; k++) x.W[k] = rp_get<float>(p); x.model = rp_get<uint8_t>(p); x.t = rp_get<float>(p); x.scale = rp_get<float>(p); }
  uint64_t alive = rp_get<uint64_t>(p);
  for (int i = 0; i < TK.nobj && i < 64; i++) TK.obj[i].alive = (alive >> i) & 1;
  RC.car[0].place = rp_get<uint8_t>(p); (void)rp_get<uint8_t>(p);
  // every sample sets all objects completely (independent of the sample shown before): original place, then
  // the moved ones of this sample; world point and road position are derived together
  for (int i = 0; i < TK.nobj; i++) { TrackObj &o = TK.obj[i];
    o.node = o.node0; o.lon = o.lon0; o.lat = o.lat0; o.dlon = o.dlat = o.hz = o.yaw = o.pitch = 0;
    o.vlon = o.vlat = o.vz = o.yawr = o.pitchr = o.hit_cd = 0; o.moving = 0; o.touched = 0; obj_update_world(o); obj_sync_pp(o); }
  if (replay_v3()) {
    int nm = rp_get<uint8_t>(p);
    for (int k = 0; k < nm; k++) {
      int i = rp_get<uint8_t>(p); int node = rp_get<uint16_t>(p); float lon = rp_get<float>(p), lat = rp_get<float>(p), hz = rp_get<float>(p), yaw = rp_get<float>(p), pitch = rp_get<float>(p);
      int ok = i < TK.nobj && node < TK.nroad && d_abs(lon) < 1e7 && d_abs(lat) < 1e7 && hz >= 0 && hz < 1e6 && d_abs(yaw) < 1e4 && d_abs(pitch) < 1e4;   // also rejects NaN
      if (ok) { TrackObj &o = TK.obj[i]; o.node = node; o.lon = 0; o.lat = 0; o.dlon = lon; o.dlat = lat; o.hz = hz; o.yaw = yaw; o.pitch = pitch; o.touched = 1; obj_update_world(o); obj_sync_pp(o); }
    }
  }
  RC.t = RPL.at ? RPL.at[f] : f / 25.0; RC.phase = PH_RACE; RC.msg_t = 0; RC.paused = 0;
}

static void replay_path(char *out, int slot) { char n[64] = "openmr2_replay_"; if (slot + 1 < 10) str_cat(n, "0", 64); str_int(n, 64, slot + 1); str_cat(n, ".dat", 64); cfg_path(out, n); }
static int replay_slot_info(int slot, RpHeader *h) {
  char p[512]; replay_path(p, slot);
  void *f = sys_fopen(p); if (!f) return 0;
  int ok = sys_fsize(f) >= sizeof(RpHeader) && sys_fread_at(f, 0, h, sizeof *h) && replay_header_ok(*h);
  sys_fclose(f); return ok;
}
static int replay_save(int slot) {
  if (g_rec_h.nframes < 10) return 0;
  char p[512]; replay_path(p, slot);
  g_rec_h.place = player().place; g_rec_h.size = (int)(RC.t * 1000);   // version 2: duration in ms
  uint8_t *buf = (uint8_t *)sys_alloc(sizeof g_rec_h + g_rec.n);
  mem_cpy(buf, &g_rec_h, sizeof g_rec_h); mem_cpy(buf + sizeof g_rec_h, g_rec.d, g_rec.n);
  int ok = sys_write_file(p, buf, sizeof g_rec_h + g_rec.n); sys_free(buf); return ok;
}
// after a race: keep it in the first free slot, or replace the oldest one
static int g_rp_next_slot;
static void replay_autosave(void) {
  RpHeader h; int slot = -1;
  (void)h;
  for (int i = 0; i < RP_SLOTS; i++) {   // free = no file or an emptied one (damaged / unknown files are kept)
    char p[512]; replay_path(p, i); void *f = sys_fopen(p);
    if (!f) { slot = i; break; }
    uint64_t sz = sys_fsize(f); sys_fclose(f); if (sz == 0) { slot = i; break; }
  }
  if (slot < 0) { slot = g_rp_next_slot % RP_SLOTS; g_rp_next_slot++; }
  replay_save(slot);
}
static void replay_free(void) { if (RPL.own) sys_free(RPL.d); sys_free(RPL.ofs); sys_free(RPL.at); mem_zero(&RPL, sizeof RPL); }
// sample shown at replay time t (the samples keep the original update cadence)
static int replay_frame_at(double t) {
  if (!RPL.at || RPL.h.nframes <= 0) return (int)(t * 25);
  int lo = 0, hi = RPL.h.nframes - 1;
  if (t >= RPL.at[hi] + 0.2) return RPL.h.nframes;   // end
  while (lo < hi) { int m = (lo + hi + 1) / 2; if (RPL.at[m] <= t) lo = m; else hi = m - 1; }
  return lo;
}
static int replay_open_slot(int slot) {
  replay_free();
  char p[512]; replay_path(p, slot);
  size_t n; uint8_t *d = sys_read_file(p, &n);
  if (!d || n < sizeof(RpHeader)) { sys_free(d); return 0; }
  mem_cpy(&RPL.h, d, sizeof RPL.h);
  if (!replay_header_ok(RPL.h)) { sys_free(d); mem_zero(&RPL, sizeof RPL); sys_log("[error] Replay: corrupted file or unsupported version"); return 0; }
  RPL.n = n - sizeof RPL.h; RPL.d = (uint8_t *)sys_alloc(RPL.n + 16); mem_cpy(RPL.d, d + sizeof RPL.h, RPL.n); sys_free(d);
  RPL.own = 1; replay_index();
  return RPL.h.nframes > 0;
}
static int replay_open_current(void) {   // the race that is running (from the pause menu)
  replay_free();
  RPL.h = g_rec_h; RPL.d = g_rec.d; RPL.n = g_rec.n; RPL.own = 0; replay_index();
  return RPL.h.nframes > 0;
}
