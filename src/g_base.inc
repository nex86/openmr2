// ===================================================================================================
// Base services for the game: sound mixer, original 2D assets (HLZ pictures, SPR sprites, fonts),
// a 640x480 canvas (the original screen) that is composed on the CPU and shown as a texture,
// and the UBB video helpers.
// ===================================================================================================

static void fit_rect_px(int W, int H, float aspect, int *x, int *y, int *w, int *h) {
  if ((float)W / H > aspect) { *h = H; *w = (int)(H * aspect + 0.5f); } else { *w = W; *h = (int)(W / aspect + 0.5f); }
  *x = (W - *w) / 2; *y = (H - *h) / 2;
}
// Selected Retro scale caps the output size in both scaling modes.
static void retro_output_rect(int W,int H,int selected,int integer,int *x,int *y,int *w,int *h){
  float fit=m_min(W/640.f,H/480.f),scale=(float)(selected<1?1:selected>4?4:selected);
  if(integer&&fit>=1)fit=m_floor(fit);
  if(scale>fit)scale=fit;
  *w=(int)(640*scale+0.5f);*h=(int)(480*scale+0.5f);if(*w<1)*w=1;if(*h<1)*h=1;
  *x=(W-*w)/2;*y=(H-*h)/2;
}
// ------------------------------------------------------------------ random
static unsigned g_rng = 12345;
static unsigned rnd(void) { g_rng = g_rng * 1103515245u + 12345u; return (g_rng >> 16) & 0x7fff; }
static double frnd(double a, double b) { return a + (b - a) * (rnd() / 32767.0); }
static int irnd(int n) { return n > 0 ? (int)(rnd() % (unsigned)n) : 0; }
// separate generator for sound / presentation: audio-visual randomness must not change the gameplay sequence
static unsigned g_rng_fx = 54321;
static unsigned rnd_fx(void) { g_rng_fx = g_rng_fx * 1103515245u + 12345u; return (g_rng_fx >> 16) & 0x7fff; }
static double frnd_fx(double a, double b) { return a + (b - a) * (rnd_fx() / 32767.0); }
static int irnd_fx(int n) { return n > 0 ? (int)(rnd_fx() % (unsigned)n) : 0; }
static double d_abs(double x) { return x < 0 ? -x : x; }
static double d_clamp(double x, double a, double b) { return x < a ? a : x > b ? b : x; }
static double d_sqrt(double x) { return x > 0 ? __builtin_sqrt(x) : 0; }
static double d_atan2(double y, double x) {   // small, adequate atan2
  if (x == 0 && y == 0) return 0;
  double ax = d_abs(x), ay = d_abs(y), a = (ax > ay ? ay / ax : ax / ay), s = a * a;
  double r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;
  if (ay > ax) r = 1.5707963267948966 - r;
  if (x < 0) r = 3.141592653589793 - r;
  return y < 0 ? -r : r;
}
static double d_sin(double x) { return m_sin((float)x); }
static double d_cos(double x) { return m_cos((float)x); }
// b^e for 0 < b and small e (per-step factors converted to the real time step)
static double d_exp(double x) {   // e^x by range reduction and a series
  if (x < -40) return 0; int n = 0; while (x > 0.5) { x *= 0.5; n++; } while (x < -0.5) { x *= 0.5; n++; }
  double t = 1, s = 1; for (int k = 1; k < 14; k++) { t *= x / k; s += t; } while (n-- > 0) s *= s; return s;
}
static double d_log(double x) {   // natural log for x > 0
  if (x <= 0) return -1e30; int e = 0; while (x > 2) { x *= 0.5; e++; } while (x < 1) { x *= 2; e--; }
  double y = (x - 1) / (x + 1), y2 = y * y, t = y, s = 0; for (int k = 1; k < 40; k += 2) { s += t / k; t *= y2; }
  return 2 * s + e * 0.69314718056;
}
static double d_pow(double b, double e) { if (b <= 0) return 0; if (e == 0) return 1; return d_exp(e * d_log(b)); }

// ------------------------------------------------------------------ sound mixer
struct Sound { int16_t *pcm; int frames, ch; };
enum { AUDIO_SOUNDS, AUDIO_ENGINES, AUDIO_RACE_MUSIC, AUDIO_MENU_MUSIC, AUDIO_MENU_SOUNDS, AUDIO_COUNT };
static int g_audio_percent[AUDIO_COUNT]={100,100,100,100,100};
static int audio_enabled(int bus){return bus>=0 && bus<AUDIO_COUNT && g_audio_percent[bus]>0;}
struct Voice { int bus; const Sound *snd; double pos, rate; float vol, pan; int loop, active, serial; };
enum { NVOICES = 24 };
struct Mixer { Voice v[NVOICES]; int serial; int dev; float music_vol, sfx_vol, voice_vol; int muted; long long filled; int peak, clipped; };
static Mixer MX;
static int g_engine_suppressed;
static Sound g_music, g_fmv_snd;
static int g_music_voice = -1, g_fmv_voice = -1;
static int gameover_audio_silence();

void app_audio_fill(int16_t *dst, int frames) {
  // The static Game Over screen has no audio. Enforce the actual screen state,
  // independently of sample names, voice buses and retained playback handles.
  if(gameover_audio_silence()){
    for(int k=0;k<NVOICES;k++)MX.v[k].active=0;
    mem_zero(dst,(size_t)frames*2*sizeof(int16_t));
    sys_encoder_audio(dst,frames);return;
  }
  const int diagnostics=sys_debug_enabled();if(diagnostics)MX.filled += frames;
  for (int i = 0; i < frames; i++) {
    float l = 0, r = 0;
    for (int k = 0; k < NVOICES; k++) {
      Voice &v = MX.v[k];
      if(v.bus==AUDIO_ENGINES&&g_engine_suppressed){v.active=0;continue;}
      if (!v.active || !v.snd || !v.snd->pcm) continue;
      int p = (int)v.pos;
      if (p >= v.snd->frames) { if (v.loop) { v.pos -= v.snd->frames; p = (int)v.pos; if (p >= v.snd->frames) p = 0; } else { v.active = 0; continue; } }
      float sl, sr;
      if (v.snd->ch == 2) { sl = v.snd->pcm[p * 2]; sr = v.snd->pcm[p * 2 + 1]; }
      else { sl = sr = v.snd->pcm[p]; }
      float pl = v.pan <= 0 ? 1 : 1 - v.pan, pr = v.pan >= 0 ? 1 : 1 + v.pan;
      float gain=v.vol*g_audio_percent[v.bus]*0.01f; l += sl * gain * pl; r += sr * gain * pr;
      v.pos += v.rate;
    }
    if (MX.muted) l = r = 0;
    if(diagnostics){ float al = l < 0 ? -l : l, ar = r < 0 ? -r : r; float m = al > ar ? al : ar; if (m > MX.peak) MX.peak = (int)m; if (m > 32767) MX.clipped++; }
    if (l > 32767) l = 32767; if (l < -32768) l = -32768; if (r > 32767) r = 32767; if (r < -32768) r = -32768;
    dst[i * 2] = (int16_t)l; dst[i * 2 + 1] = (int16_t)r;
  }
  sys_encoder_audio(dst,frames);
}
// returns a voice handle (index + 256*serial) or -1
static int snd_play(const Sound *s, float vol, float pitch, int loop, float pan, int bus=AUDIO_SOUNDS) {
  if (!audio_enabled(bus)||(bus==AUDIO_ENGINES&&g_engine_suppressed))return -1;
  if (!s || !s->pcm || s->frames <= 0) return -1;
  int best = -1;
  for (int k = 2; k < NVOICES; k++) if (!MX.v[k].active) { best = k; break; }   // 0 = music, 1 = video/voice
  if (best < 0) best = 2 + irnd_fx(NVOICES - 2);
  Voice &v = MX.v[best]; v.bus=bus;
  v.snd = s; v.pos = 0; v.rate = pitch; v.vol = vol * MX.sfx_vol; v.pan = pan; v.loop = loop; v.serial = ++MX.serial; v.active = 1;
  return best + 256 * (v.serial & 0xffff);
}
static Voice *snd_voice(int h) { if (h < 0) return 0; Voice &v = MX.v[h & 255]; return (v.active && (v.serial & 0xffff) == (h >> 8)) ? &v : 0; }
static void snd_stop(int h) { Voice *v = snd_voice(h); if (v) v->active = 0; }
static void snd_stop_all_sfx(void) { for (int k = 2; k < NVOICES; k++) MX.v[k].active = 0; }
static void music_play(const Sound *s, int loop, int bus=AUDIO_RACE_MUSIC) {
  Voice &v = MX.v[0]; v.active = 0; v.bus=bus;
  if(!audio_enabled(bus))return;
  if (!s || !s->pcm) return;
  v.snd = s; v.pos = 0; v.rate = 1; v.vol = MX.music_vol; v.pan = 0; v.loop = loop; v.active = 1; v.serial = ++MX.serial;
}
static void music_stop(void) { MX.v[0].active = 0; }
static void voice_play(const Sound *s, float vol) {   // channel 1: video sound track or Boyle voice
  Voice &v = MX.v[1]; v.active = 0; v.bus=AUDIO_SOUNDS;
  if(!audio_enabled(AUDIO_SOUNDS))return;
  if (!s || !s->pcm) return;
  v.snd = s; v.pos = 0; v.rate = 1; v.vol = vol; v.pan = 0; v.loop = 0; v.active = 1; v.serial = ++MX.serial;
}
static void voice_stop(void) { MX.v[1].active = 0; }

static int sound_load_wav(Sound *s, const char *path) {
  mem_zero(s, sizeof *s);
  size_t n; uint8_t *d = sys_read_file(path, &n);
  if (!d) return 0;
  int fr, ch, rate; int16_t *pcm = wav_decode(d, n, &fr, &ch, &rate); sys_free(d);
  if (!pcm) return 0;
  if (rate != 22050 && rate > 0) {   // resample to the mixer rate (nearest)
    int nf = (int)((long long)fr * 22050 / rate);
    int16_t *o = (int16_t *)sys_alloc((size_t)nf * ch * 2 + 4);
    for (int i = 0; i < nf; i++) { int si = (int)((long long)i * rate / 22050); for (int c = 0; c < ch; c++) o[i * ch + c] = pcm[si * ch + c]; }
    sys_free(pcm); pcm = o; fr = nf;
  }
  s->pcm = pcm; s->frames = fr; s->ch = ch;
  return 1;
}
static void sound_free(Sound *s) { sys_free(s->pcm); mem_zero(s, sizeof *s); }

// ------------------------------------------------------------------ original pictures / sprites / fonts
struct Pic { char texture_key[512]; int w, h; uint8_t *px; uint8_t pal[768]; };
struct Sprite { char texture_source[512]; int frame,source_x,source_y; int x, y, w, h; uint8_t *px; };
struct SprFile { Sprite f[48]; int n; };

static void game_path(char *out, const char *rel) { str_cpy(out, g_gamedir, 512); str_cat(out, "/", 512); str_cat(out, rel, 512); }
// everything OpenMR2 saves (saves, high scores, settings, replays) lives in the folder revived_config
// (next to OpenMR2.exe: the program runs in its own folder, the game files may be elsewhere)
static void cfg_path(char *out, const char *rel) { str_cpy(out, "revived_config/", 512); str_cat(out, rel, 512); }

static int pic_from_mem(Pic *p, uint8_t *d, size_t n) {
  mem_zero(p, sizeof *p);
  if (!d || n < 776) { sys_free(d); return 0; }
  mem_cpy(p->pal, d, 768);
  p->w = rd16(d + 768); p->h = rd16(d + 770);
  size_t np = (size_t)p->w * p->h;
  p->px = (uint8_t *)sys_alloc(np + 16);
  int got = hlz_unpack(d, n, 772, p->px, np);
  sys_free(d);
  if (got < 0) { sys_free(p->px); p->px = 0; return 0; }
  return 1;
}
static int pic_load(Pic *p, const char *rel) {          // from the game folder
  char path[512]; game_path(path, rel);
  size_t n; uint8_t *d = sys_read_file(path, &n);
  int ok=pic_from_mem(p,d,n);texture_key(p->texture_key,rel,0);str_cat(p->texture_key,".png",512);return ok;
}
static int pic_load_cd(Pic *p, const char *cdpath) {    // from the CD image
  size_t n; uint8_t *d = iso_load(cdpath, &n);
  int ok=pic_from_mem(p,d,n);texture_key(p->texture_key,cdpath,1);str_cat(p->texture_key,".png",512);return ok;
}
static void pic_free(Pic *p) { sys_free(p->px); mem_zero(p, sizeof *p); }

static int spr_from_mem(SprFile *s, uint8_t *d, size_t n) {
  mem_zero(s, sizeof *s);
  if (!d) return 0;
  size_t o = 0;
  while (o + 24 <= n && s->n < 48) {
    u32 sz = rd32(d + o + 4), x0 = rd32(d + o + 8), x1 = rd32(d + o + 12), y0 = rd32(d + o + 16), y1 = rd32(d + o + 20);
    if (x1 < x0 || y1 < y0) break;
    u32 w = x1 - x0, h = y1 - y0;
    if (sz != w * h + 16 || o + 24 + (size_t)w * h > n) break;
    Sprite &f = s->f[s->n++];
    f.source_x=f.x = (int)x0; f.source_y=f.y = (int)y0; f.w = (int)w; f.h = (int)h;
    f.px = (uint8_t *)sys_alloc((size_t)w * h + 1); mem_cpy(f.px, d + o + 24, (size_t)w * h);
    o += 8 + sz;
  }
  sys_free(d);
  return s->n > 0;
}
static int spr_load(SprFile *s, const char *rel) {
  char path[512]; game_path(path, rel);
  size_t n = 0; uint8_t *d = sys_read_file(path, &n);
  int ok=spr_from_mem(s,d,n);for(int i=0;i<s->n;i++){texture_key(s->f[i].texture_source,rel,0);s->f[i].frame=i;}return ok;
}
static int spr_load_cd(SprFile *s, const char *rel) {
  size_t n = 0; uint8_t *d = iso_load(rel, &n);
  int ok=spr_from_mem(s,d,n);for(int i=0;i<s->n;i++){texture_key(s->f[i].texture_source,rel,1);s->f[i].frame=i;}return ok;
}
static void spr_free(SprFile *s) { for (int i = 0; i < s->n; i++) sys_free(s->f[i].px); mem_zero(s, sizeof *s); }

// bitmap fonts of the original game (TEXTE\*.HLZ, FONTE21)
#include "revived_font_data.h"
static int g_revived_text=1;
struct Glyph { int x, y, w, h; };
struct Font { Pic pic; Glyph g[160]; int ink1, ink2, height, space, revived_height; };
static int font_load(Font *f, const char *rel) {
  mem_zero(f, sizeof *f);
  if (!pic_load(&f->pic, rel)) return 0;
  const uint8_t *px = f->pic.px; int W = f->pic.w, H = f->pic.h;
  int fonte = 0;   // FONTE21 style: background 0, separators 255, ink 250
  { int n0 = 0, n255 = 0; for (int i = 0; i < W * H; i++) { if (px[i] == 0) n0++; if (px[i] == 255) n255++; } fonte = n0 > n255; }
  int bg = fonte ? 0 : 255;
  f->ink1 = fonte ? 250 : 254; f->ink2 = fonte ? -1 : 1;
  int ch = 32, y = 0;
  while (y < H && ch < 32 + 160) {
    int y0, y1;
    if (fonte) {   // bands separated by full separator rows
      auto seprow = [&](int yy) { int c = 0; for (int x = 0; x < W; x++) if (px[yy * W + x] == 255) c++; return c > W * 3 / 4; };
      while (y < H && seprow(y)) y++;
      y0 = y;
      while (y < H && !seprow(y)) y++;
      y1 = y;
      if (y1 - y0 < 4) break;
    } else {
      while (y < H) { int any = 0; for (int x = 0; x < W; x++) if (px[y * W + x] != bg) { any = 1; break; } if (any) break; y++; }
      y0 = y;
      while (y < H) { int any = 0; for (int x = 0; x < W; x++) if (px[y * W + x] != bg) { any = 1; break; } if (!any) break; y++; }
      y1 = y;
      if (y1 - y0 < 4 || y0 > 380) break;
    }
    int x = 0;
    while (x < W && ch < 32 + 160) {
      // cell = run of columns that are not entirely separator/background
      auto colsep = [&](int cx) { for (int yy = y0; yy < y1; yy++) { uint8_t c = px[yy * W + cx]; if (fonte ? c != 255 : c != bg) return 0; } return 1; };
      while (x < W && colsep(x)) x++;
      int x0 = x;
      while (x < W && !colsep(x)) x++;
      if (x - x0 < 1) break;
      if (!fonte && x - x0 <= 2) continue;   // marker column at the left edge of the CLR fonts
      if (fonte) { int ink = 0; for (int yy = y0; yy < y1 && !ink; yy++) for (int xx = x0; xx < x; xx++) if (px[yy * W + xx] == 250) { ink = 1; break; } if (!ink) continue; }
      // FONTE21 has no cell for the space character
      if (fonte && ch == 32) { f->g[0].w = 0; ch++; }
      Glyph &g = f->g[ch - 32]; g.x = x0; g.y = y0; g.w = x - x0; g.h = y1 - y0;
      ch++;
    }
  }
  f->height = 0;
  for (int i = 1; i < 95; i++) if (f->g[i].h > f->height) f->height = f->g[i].h;
  f->space = f->g['n' - 32].w ? f->g['n' - 32].w : 8;
  // Bitmap cells include separator/leading rows; use their intended type sizes.
  f->revived_height=!str_icmp(rel,"TEXTE/HNDEL14.HLZ")?14:!str_icmp(rel,"TEXTE/HNDEL10.HLZ")?10:!str_icmp(rel,"TEXTE/CLR6_13.HLZ")?10:!str_icmp(rel,"TEXTE/LATIN11.HLZ")?11:!str_icmp(rel,"TEXTE/FONTE21.HLZ")?18:f->height;
  return 1;
}

// ------------------------------------------------------------------ 640x480 canvas
enum { CW = 640, CH = 480 };
static uint32_t *g_cv;          // RGBA, 0 alpha = transparent
static GLuint g_cv_tex;
static int g_cv_dirty;
static inline uint32_t rgba(int r, int g, int b, int a) { return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24); }
static inline uint32_t pal_rgba(const uint8_t *pal, int i) { return rgba(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], 255); }
static void texture_canvas_reset();
static int texture_canvas_original_picture(const Pic &p,int x,int y,int w,int h);
static int texture_canvas_image(const char *key,int x,int y,int w,int h,float u=0,float v=0,float uw=1,float vh=1,uint32_t tint=0xffffffffu);
static void texture_dump_sprite(const Sprite &s,const u8 *pal);
static void cv_clear(uint32_t c) { texture_canvas_reset(); for (int i = 0; i < CW * CH; i++) g_cv[i] = c; g_cv_dirty = 1; }
static void cv_pic(const Pic &p, int dx, int dy, int scale) {   // scale 2 for the 320-wide pictures
  if (!p.px) return;
  if(texture_canvas_image(p.texture_key,dx,dy,p.w*scale,p.h*scale))return;
  if(g_smooth_filter&&texture_canvas_original_picture(p,dx,dy,p.w*scale,p.h*scale))return;
  for (int y = 0; y < p.h * scale; y++) {
    int ty = dy + y; if (ty < 0 || ty >= CH) continue;
    const uint8_t *row = p.px + (y / scale) * p.w;
    for (int x = 0; x < p.w * scale; x++) { int tx = dx + x; if (tx < 0 || tx >= CW) continue; g_cv[ty * CW + tx] = pal_rgba(p.pal, row[x / scale]); }
  }
  g_cv_dirty = 1;
}
// picture stretched to an arbitrary rectangle (e.g. 320x80 menu bar of the 320x400 mode)
static void cv_pic_rect(const Pic &p, int dx, int dy, int dw, int dh) {
  if (!p.px) return;
  if(texture_canvas_image(p.texture_key,dx,dy,dw,dh))return;
  if(g_smooth_filter&&texture_canvas_original_picture(p,dx,dy,dw,dh))return;
  for (int y = 0; y < dh; y++) { int ty = dy + y; if (ty < 0 || ty >= CH) continue; const uint8_t *row = p.px + (y * p.h / dh) * p.w;
    for (int x = 0; x < dw; x++) { int tx = dx + x; if (tx < 0 || tx >= CW) continue; g_cv[ty * CW + tx] = pal_rgba(p.pal, row[x * p.w / dw]); } }
  g_cv_dirty = 1;
}
static void cv_spr_rect(const Sprite &s, const uint8_t *pal, double sx, double sy, int dy = 0, int dx=0) {   // sprite of a 320x400 screen with scale sx, sy (moved down by dy)
  if (!s.px) return;
  int w = (int)(s.w * sx), h = (int)(s.h * sy), x0 = (int)(s.x * sx)+dx, y0 = (int)(s.y * sy) + dy;
  if(g_dump_active)texture_dump_sprite(s,pal);
  if(g_custom_textures||g_revived_ui){char key[512];texture_sprite_key(key,s.texture_source,s.frame,pal);if(texture_canvas_image(key,x0,y0,w,h))return;}
  for (int y = 0; y < h; y++) { int ty = y0 + y; if (ty < 0 || ty >= CH) continue; const uint8_t *row = s.px + (int)(y / sy) * s.w;
    for (int x = 0; x < w; x++) { uint8_t c = row[(int)(x / sx)]; if (!c) continue; int tx = x0 + x; if (tx < 0 || tx >= CW) continue; g_cv[ty * CW + tx] = pal_rgba(pal, c); } }
  g_cv_dirty = 1;
}
static void cv_spr(const Sprite &s, const uint8_t *pal, int dx, int dy, int scale) {
  if (!s.px) return;
  if(g_dump_active)texture_dump_sprite(s,pal);
  if(g_custom_textures||g_revived_ui){char key[512];texture_sprite_key(key,s.texture_source,s.frame,pal);if(texture_canvas_image(key,s.x*scale+dx,s.y*scale+dy,s.w*scale,s.h*scale))return;}
  for (int y = 0; y < s.h * scale; y++) {
    int ty = s.y * scale + dy + y;
    if (ty < 0 || ty >= CH) continue;
    const uint8_t *row = s.px + (y / scale) * s.w;
    for (int x = 0; x < s.w * scale; x++) {
      uint8_t c = row[x / scale]; if (!c) continue;
      int tx = s.x * scale + dx + x; if (tx < 0 || tx >= CW) continue;
      g_cv[ty * CW + tx] = pal_rgba(pal, c);
    }
  }
  g_cv_dirty = 1;
}
static void cv_rect(int x, int y, int w, int h, uint32_t c) {
  int a = (int)(c >> 24);
  for (int yy = y; yy < y + h; yy++) {
    if (yy < 0 || yy >= CH) continue;
    for (int xx = x; xx < x + w; xx++) {
      if (xx < 0 || xx >= CW) continue;
      uint32_t &d = g_cv[yy * CW + xx];
      if (a >= 255 || (d >> 24) == 0) { d = c; continue; }
      int da = 255 - a;
      int r = ((c & 255) * a + (d & 255) * da) / 255, g = (((c >> 8) & 255) * a + ((d >> 8) & 255) * da) / 255, b = (((c >> 16) & 255) * a + ((d >> 16) & 255) * da) / 255;
      d = rgba(r, g, b, 255);
    }
  }
  g_cv_dirty = 1;
}
// German umlauts (Latin-1) are at fixed places after '~' in the fonts of the original
static unsigned char glyph_code(unsigned char c) {
  if(c==0xb0)return 0; // degree sign is composed below
  switch(c){case 0xe0:case 0xe2:case 0xc0:case 0xc2:return c<0xe0?'A':'a';
    case 0xe9:case 0xe8:case 0xea:case 0xeb:return 'e';case 0xc9:case 0xc8:case 0xca:case 0xcb:return 'E';
    case 0xee:case 0xef:return 'i';case 0xce:case 0xcf:return 'I';case 0xf4:return 'o';case 0xd4:return 'O';
    case 0xf9:case 0xfb:return 'u';case 0xd9:case 0xdb:return 'U';case 0xe7:return 'c';case 0xc7:return 'C';}
  switch (c) { case 0xe4: return 0x8b; case 0xf6: return 0x89; case 0xfc: return 0x8a; case 0xdf: return 0x88; case 0xc4: return 'A'; case 0xd6: return 'O'; case 0xdc: return 'U'; }
  return c;
}
static int revived_font_enabled(const Font &f){return g_revived_text && !(g_custom_textures && texture_custom(f.pic.texture_key));}
static int revived_font_height(const Font &f){return f.revived_height?f.revived_height:f.height;}
static void revived_font_draw(const Font &f,float x,float y,unsigned char c,uint32_t col,int scale);
static int font_char_w(const Font &f, unsigned char c) {
  if(revived_font_enabled(f) && c>=32)return (int)(REVIVED_GLYPHS[c-32].advance*revived_font_height(f)/(float)REVIVED_CAP+0.5f)+1;
  if(c==0xb0)return 5;
  c = glyph_code(c); if (c == ' ') return f.space; if (c < 32 || c >= 32 + 160) return 0; return f.g[c - 32].w ? f.g[c - 32].w + 1 : f.space; }
static int cv_text_w_raw(const Font &f, const char *s, int scale) {int w = 0; for (; *s && *s != '\n'; s++) w += font_char_w(f, (unsigned char)*s) * scale; return w; }
// align: 0 left, 1 center, 2 right. shadow drawn 1px down/right in black
static int cv_text_w(const Font &f,const char *s,int scale) { return cv_text_w_raw(f,ui_translate(s),scale); }
static void cv_text_raw(const Font &f, int x, int y, const char *s, uint32_t col, int align, int scale) {
  if (!f.pic.px) return;
  int lx = x;
  while (*s) {
    int w = cv_text_w_raw(f, s, scale);
    int cx = align == 1 ? lx - w / 2 : align == 2 ? lx - w : lx;
    for (; *s && *s != '\n'; s++) {
      unsigned char c0 = (unsigned char)*s, c = glyph_code(c0);
      if(revived_font_enabled(f)){
        revived_font_draw(f,(float)cx,(float)y,c0,col,scale);
        cx+=font_char_w(f,c0)*scale;continue;
      }
      if (c > 32 && c < 32 + 160 && f.g[c - 32].w) {
        const Glyph &g = f.g[c - 32];
        if(g_custom_textures && texture_custom(f.pic.texture_key)){
          texture_canvas_image(f.pic.texture_key,cx+scale,y+scale,g.w*scale,g.h*scale,g.x/(float)f.pic.w,g.y/(float)f.pic.h,g.w/(float)f.pic.w,g.h/(float)f.pic.h,0xff000000u);
          texture_canvas_image(f.pic.texture_key,cx,y,g.w*scale,g.h*scale,g.x/(float)f.pic.w,g.y/(float)f.pic.h,g.w/(float)f.pic.w,g.h/(float)f.pic.h,col);
        }else for (int pass = 0; pass < 2; pass++)
          for (int yy = 0; yy < g.h * scale; yy++)
            for (int xx = 0; xx < g.w * scale; xx++) {
              uint8_t p = f.pic.px[(g.y + yy / scale) * f.pic.w + g.x + xx / scale];
              if (p != f.ink1 && (int)p != f.ink2) continue;
              int tx = cx + xx + (pass == 0 ? scale : 0), ty = y + yy + (pass == 0 ? scale : 0);
              if (tx < 0 || tx >= CW || ty < 0 || ty >= CH) continue;
              g_cv[ty * CW + tx] = pass == 0 ? rgba(0, 0, 0, 255) : ((int)p == f.ink2 ? rgba(((col & 255) * 3) / 5, (((col >> 8) & 255) * 3) / 5, (((col >> 16) & 255) * 3) / 5, 255) : col);
            }
      }
      if (c0 == 0xc4 || c0 == 0xd6 || c0 == 0xdc) {   // capital umlauts: the fonts have none, so two dots go over the letter
        int gw = font_char_w(f, c0) * scale, d = scale * (f.height >= 14 ? 2 : 1) + (f.height >= 20 ? 1 : 0);
        for (int pass = 0; pass < 2; pass++) for (int k = 0; k < 2; k++) {
          int dx = cx + gw * (k ? 2 : 1) / 3 - d - (k ? 0 : 1) + (pass == 0 ? scale : 0), dy = y - d - 2 * scale + (pass == 0 ? scale : 0);
          for (int yy = 0; yy < d; yy++) for (int xx = 0; xx < d; xx++) { int tx = dx + xx, ty = dy + yy; if (tx >= 0 && tx < CW && ty >= 0 && ty < CH) g_cv[ty * CW + tx] = pass == 0 ? rgba(0, 0, 0, 255) : col; }
        }
      }
      // The shared original font has German umlauts but no French accented letters.
      // Compose diacritics over its original base glyph; leave source assets unchanged.
      int accent=0;
      switch(c0){case 0xe9:case 0xc9:accent=1;break;case 0xe0:case 0xc0:case 0xe8:case 0xc8:case 0xf9:case 0xd9:accent=2;break;
        case 0xe2:case 0xc2:case 0xea:case 0xca:case 0xee:case 0xce:case 0xf4:case 0xd4:case 0xfb:case 0xdb:accent=3;break;
        case 0xeb:case 0xcb:case 0xef:case 0xcf:accent=4;break;case 0xe7:case 0xc7:accent=5;break;}
      if(accent){
        int center=cx+(font_char_w(f,c0)-1)*scale/2,top=y-3*scale;
        for(int yy=0;yy<3;yy++)for(int xx=0;xx<5;xx++){
          int on=accent==1?(xx==3-yy):accent==2?(xx==1+yy):accent==3?(yy==0?xx==2:yy==1?(xx==1||xx==3):0):accent==4?(yy==1&&(xx==0||xx==4)):(yy==0?xx==2:yy==1?xx==3:xx==1||xx==2);
          if(on)for(int sy=0;sy<scale;sy++)for(int sx=0;sx<scale;sx++){int tx=center+(xx-2)*scale+sx,ty=(accent==5?y+f.height*scale:top)+yy*scale+sy;if(tx>=0&&tx<CW&&ty>=0&&ty<CH)g_cv[ty*CW+tx]=col;}
        }
      }
      if(c0==0xb0){for(int yy=0;yy<3;yy++)for(int xx=0;xx<3;xx++)if((xx==1)!=(yy==1))cv_rect(cx+xx*scale,y+yy*scale,scale,scale,col);}
      cx += font_char_w(f, c0) * scale;
    }
    if (*s == '\n') { s++; y += (f.height + 2) * scale; }
  }
  if(!revived_font_enabled(f))g_cv_dirty = 1;
}

static void cv_text(const Font &f,int x,int y,const char *s,uint32_t col,int align,int scale) { cv_text_raw(f,x,y,ui_translate(s),col,align,scale); }

// draw the canvas (full 640x480 virtual screen, letterboxed 4:3 in the window)
static GLuint g_prog_canvas; static GLint uc_tex, uc_rect,uc_uv,uc_tint,uc_clip,uc_sdf;
static const char *FS_CANVAS =
  "#version 330 core\n"
  "in vec2 vUV; out vec4 o; uniform sampler2D uTex; uniform vec4 uUV,uTint,uClip; uniform int uSDF;\n"
  "void main(){ if(gl_FragCoord.x<uClip.x || gl_FragCoord.y<uClip.y || gl_FragCoord.x>=uClip.z || gl_FragCoord.y>=uClip.w)discard; vec4 t=texture(uTex,uUV.xy+vUV*uUV.zw); if(uSDF!=0){float d=t.r; float aa=max(fwidth(d)*0.7,0.003); t=vec4(1,1,1,smoothstep(0.5-aa,0.5+aa,d));} o=t*uTint; }\n";
struct Screen { int x, y, w, h; };   // pixel rect of the 640x480 virtual screen in the window
static Screen g_scr;
static int g_scr_fill;   // the target already has the 4:3 shape (Retro Mode: 640x480, or 320x400 with wide pixels in the race)
static void screen_compute(int W, int H) {
  if (g_scr_fill) { g_scr.x = 0; g_scr.y = 0; g_scr.w = W; g_scr.h = H; return; }
  fit_rect_px(W, H, 4.0f / 3.0f, &g_scr.x, &g_scr.y, &g_scr.w, &g_scr.h);
}
static void canvas_premultiply(uint32_t *dst,const uint32_t *src,int count){
  for(int i=0;i<count;i++){uint32_t c=src[i],a=c>>24;dst[i]=a==255?c:((c&255)*a+127)/255|((((c>>8)&255)*a+127)/255<<8)|((((c>>16)&255)*a+127)/255<<16)|(a<<24);}
}
static uint32_t *g_cv_upload;
#include "texture_canvas.inc"
static void canvas_draw(int W, int H) {
  if (!g_cv_tex) { glGenTextures(1, &g_cv_tex); g_cv_dirty = 1; }
  glBindTexture(GL_TEXTURE_2D, g_cv_tex);
  if (g_cv_dirty || g_texture_layers.n) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if(g_smooth_filter){if(!g_cv_upload)g_cv_upload=(uint32_t *)sys_alloc(CW*CH*4);canvas_premultiply(g_cv_upload,g_cv,CW*CH);}
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, CW, CH, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_smooth_filter?g_cv_upload:g_cv);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, g_smooth_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_cv_dirty = 0;
  }
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,g_smooth_filter?GL_LINEAR:GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,g_smooth_filter?GL_LINEAR:GL_NEAREST);
  glViewport(0, 0, W, H); glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(g_prog_canvas); glBindVertexArray(R.empty_vao);
  glActiveTexture(GL_TEXTURE0); glUniform1i(uc_tex, 0);glUniform4f(uc_clip,(float)g_scr.x,(float)g_scr.y,(float)(g_scr.x+g_scr.w),(float)(g_scr.y+g_scr.h));
  for(int i=0;i<g_texture_layers.n;i++){
    TextureLayer &l=g_texture_layers.p[i];GLuint tex=l.texture;
    if(l.slice>=0){CanvasSlice &slice=g_canvas_slices.p[l.slice];if(!slice.texture)glGenTextures(1,&slice.texture);tex=slice.texture;
      glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,CW,CH,0,GL_RGBA,GL_UNSIGNED_BYTE,slice.pixels);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,g_smooth_filter?GL_LINEAR:GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,g_smooth_filter?GL_LINEAR:GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
    glBlendFunc(l.slice>=0&&g_smooth_filter?GL_ONE:GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glUniform1i(uc_sdf,l.sdf);
    texture_canvas_draw_layer(tex,W,H,l.x,l.y,l.w,l.h,l.u,l.v,l.uw,l.vh,l.tint);
  }
  glBlendFunc(g_smooth_filter?GL_ONE:GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glUniform1i(uc_sdf,0);glBindTexture(GL_TEXTURE_2D,g_cv_tex);glUniform4f(uc_uv,0,0,1,1);glUniform4f(uc_tint,1,1,1,1);
  glUniform4f(uc_rect, -1 + 2.0f * g_scr.x / W, -1 + 2.0f * g_scr.y / H, -1 + 2.0f * (g_scr.x + g_scr.w) / W, -1 + 2.0f * (g_scr.y + g_scr.h) / H);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisable(GL_BLEND);
}
// convert a rect of the virtual 640x480 screen (top-left origin) to a GL viewport
static void vrect_viewport(int W, int H, float x, float y, float w, float h, int *vx, int *vy, int *vw, int *vh) {
  float sx = g_scr.w / 640.0f, sy = g_scr.h / 480.0f;
  *vx = g_scr.x + (int)(x * sx + 0.5f); *vw = (int)(w * sx + 0.5f); *vh = (int)(h * sy + 0.5f);
  *vy = H - (g_scr.y + (int)((y + h) * sy + 0.5f));
  (void)W;
}
static void mouse_virtual(const Input *in, int H, int *x, int *y) {
  float sx = g_scr.w / 640.0f, sy = g_scr.h / 480.0f; if (sx <= 0) sx = 1; if (sy <= 0) sy = 1;
  // window y is top-down, GL rect uses bottom-up: g_scr.y is the bottom offset; for symmetric letterboxing both are equal
  int top = H - (g_scr.y + g_scr.h);
  *x = (int)((in->mx - g_scr.x) / sx); *y = (int)((in->my - top) / sy);
}
