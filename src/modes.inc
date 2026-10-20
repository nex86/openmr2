// ===================================================================================================
// Game modes. Included from core.cpp.
//   MODE_TRACK  the game (intro, menus, shops, championship, races)  - default when the CD image is found
//   MODE_FMV    video player for all Lance Boyle clips (F3)
//   MODE_VIEWER 3D model viewer (F1)
// ===================================================================================================

#include "g_base.inc"
#include "texture_dump.inc"

static M4 m4_roty(float a) { M4 r = m4_id(); float c = m_cos(a), s = m_sin(a); r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r; }

// ------------------------------------------------------------------ video helpers
static GLuint g_prog_video; static GLint uv_tex, uv_rect, uv_gray; static float g_gray;   // g_gray: grey picture (race pause)
static const char *FS_VIDEO =
  "#version 330 core\n"
  "in vec2 vUV; out vec4 o; uniform sampler2D uTex; uniform float uGray; uniform mat3 uWarp; uniform vec3 uProj; uniform int uWarpOn;\n"
  "void main(){ vec2 uv = vUV;\n"
  "  if (uWarpOn != 0) {\n"   // turn the picture of the video frame to the exact (interpolated) camera
  "    vec2 n = vec2(uv.x, 1.0 - uv.y) * 2.0 - 1.0;\n"
  "    vec3 d = uWarp * vec3(n.x / uProj.x, (n.y + uProj.z) / uProj.y, -1.0);\n"
  "    vec2 m = vec2(uProj.x * d.x, uProj.y * d.y + uProj.z * d.z) / -d.z;\n"
  "    uv = vec2(m.x * 0.5 + 0.5, 0.5 - m.y * 0.5);\n"
  "  }\n"
  "  vec3 c = texture(uTex, uv).rgb; o = vec4(mix(c, vec3(dot(c, vec3(0.3,0.59,0.11))), uGray), 1.0); }\n";
static GLint uv_warp, uv_proj, uv_warpon;
static float g_vwarp[9], g_vproj[3]; static int g_vwarp_on;

static int g_race_tex_frame = -1;   // race video frame currently in TK.vtex (-1: something else)
#include "video_upscale.inc"
static void video_tex_upload(GLuint *tex, const Video &v) {
  g_race_tex_frame = -1;
  if (!*tex) glGenTextures(1, tex);
  glBindTexture(GL_TEXTURE_2D, *tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, v.w, v.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, v.rgba);
  int f = g_smooth_filter ? GL_LINEAR : GL_NEAREST;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  video_fsr_uploaded(*tex,v);
}
static void draw_video_rect(GLuint tex, int W, int H, int x, int y, int w, int h) {
  tex=video_fsr_texture(tex,w,h);
  glViewport(0, 0, W, H);
  glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
  glUseProgram(g_prog_video); glBindVertexArray(R.empty_vao);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,g_smooth_filter?GL_LINEAR:GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,g_smooth_filter?GL_LINEAR:GL_NEAREST);
  glUniform1i(uv_tex, 0); glUniform1f(uv_gray, g_gray);
  glUniform1i(uv_warpon, g_vwarp_on); glUniformMatrix3fv(uv_warp, 1, GL_FALSE, g_vwarp); glUniform3f(uv_proj, g_vproj[0], g_vproj[1], g_vproj[2]);
  glUniform4f(uv_rect, -1 + 2.0f * x / W, -1 + 2.0f * y / H, -1 + 2.0f * (x + w) / W, -1 + 2.0f * (y + h) / H);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static Input g_noinput;

#include "g_track.inc"
#include "g_race.inc"
#include "g_replay.inc"
#include "g_menu.inc"

// ------------------------------------------------------------------ FMV player (F3)
struct FmvState {
  char names[64][40]; int n, cur, loaded, paused;
  Video vid; double t; GLuint vtex;
};
static FmvState FV;

static void cb_fmv(void *u, const char *name, uint32_t size) {
  const char *dir = (const char *)u; (void)size;
  size_t ln = str_len(name);
  if (ln < 5 || str_icmp(name + ln - 4, ".UBB") || FV.n >= 64) return;
  for (int i = 0; i < FV.n; i++) { const char *b = FV.names[i]; while (*b && *b != '/') b++; if (!str_icmp(b + 1, name)) return; }
  str_cpy(FV.names[FV.n], dir, 40); str_cat(FV.names[FV.n], "/", 40); str_cat(FV.names[FV.n], name, 40); FV.n++;
}
static int fmv_open(int i) {
  voice_stop();
  if (FV.vid.data) video_close(&FV.vid);
  FV.loaded = 0;
  if (!FV.n) return 0;
  FV.cur = ((i % FV.n) + FV.n) % FV.n;
  if (!video_open_iso(&FV.vid,FV.names[FV.cur],400,audio_enabled(AUDIO_SOUNDS))) { str_cpy(A.status, ui_translate("Video nicht lesbar"), 256); return 0; }
  FV.t = 0; FV.paused = 0; FV.loaded = 1;
  if (FV.vid.pcm) { g_fmv_snd.pcm = FV.vid.pcm; g_fmv_snd.frames = FV.vid.pcm_frames; g_fmv_snd.ch = 2; voice_play(&g_fmv_snd, 1.0f); }
  return 1;
}
static int fmv_frame(int W, int H, float dt, const Input *in) {
  if (!FV.n) {
    iso_list("BOYLE", cb_fmv, (void *)"BOYLE"); iso_list("BOYLE0", cb_fmv, (void *)"BOYLE0"); iso_list("BOYLE2", cb_fmv, (void *)"BOYLE2");
    sort40(FV.names, FV.n);
    if (!FV.n) { A.mode = MODE_VIEWER; return 1; }
    fmv_open(0);
    if (A.start_frame) FV.t = A.start_frame / 25.0;
  }
  if (in->pressed[K_RIGHT]) fmv_open(FV.cur + 1);
  if (in->pressed[K_LEFT]) fmv_open(FV.cur - 1);
  if (in->pressed[K_ENTER]) fmv_open(FV.cur);
  Voice *v = &MX.v[1];
  if (in->pressed[K_SPACE]) { FV.paused ^= 1; if (v->snd == &g_fmv_snd) v->active = !FV.paused && v->pos < g_fmv_snd.frames; }
  if (in->pressed[K_M]) MX.muted ^= 1;
  // F2 is the global texture filter shortcut.
  if (in->pressed[K_H]) A.show_help ^= 1;
  int frame = 0;
  if (FV.loaded) {
    if (!FV.paused) FV.t += dt;
    if (!A.shot_exit && MX.dev && FV.vid.pcm && FV.vid.pcm_frames > 0 && v->snd == &g_fmv_snd && v->active)
      frame = (int)((v->pos > 7000 ? v->pos - 7000 : 0) * FV.vid.nframes / FV.vid.pcm_frames);   // audio is the master clock
    else frame = (int)(FV.t * FV.vid.fps_exact);
    if (frame >= FV.vid.nframes) frame = FV.vid.nframes - 1;
    video_decode(&FV.vid, frame);
    video_tex_upload(&FV.vtex, FV.vid);
  }
  fbo_ensure(W, H);
  glBindFramebuffer(GL_FRAMEBUFFER, R.samples ? R.fbo : 0);
  glViewport(0, 0, W, H); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (FV.loaded) {
    float aspect = (FV.vid.w * 4.0f / 320) / (FV.vid.h * 3.0f / 400);
    int rx, ry, rw, rh; fit_rect_px(W, H, aspect, &rx, &ry, &rw, &rh);
    draw_video_rect(FV.vtex, W, H, rx, ry, rw, rh);
  }
  {
    char t[512]; t[0] = 0;
    str_cat(t, ui_translate("Video: "), 512); str_cat(t, FV.n ? FV.names[FV.cur] : "-", 512);
    str_cat(t, "  [", 512); str_int(t, 512, FV.cur + 1); str_cat(t, "/", 512); str_int(t, 512, FV.n); str_cat(t, "]  ", 512);
    str_int(t, 512, frame); str_cat(t, "/", 512); str_int(t, 512, FV.vid.nframes);
    if (FV.paused) str_cat(t, "  [PAUSE]", 512);
    if (A.show_help) str_cat(t, "\nLeft/Right: video   Space: pause   Enter: restart   M: mute   H: help   Esc: Extras", 512);
    hud_draw(W, H, t);
  }
  if (!frame_finish(W, H)) return 0;
  A.title[0] = 0; str_cat(A.title, OPENMR2_FULLTITLE "  -  Videos", 256);
  return 1;
}

// ------------------------------------------------------------------ game mode frame
// ------------------------------------------------------------------ post-processing (Retro Mode scaling, FXAA)
struct Post { GLuint fbo, tex, depth; int w, h; GLuint prog_copy, prog_fxaa; GLint uc_tex, uc_rect, uf_tex, uf_rect, uf_px; };
static Post g_post;
#include "retro_palette.inc"
#include "output_filters.inc"
static const char *FS_COPY = "#version 330 core\nin vec2 vUV; out vec4 o; uniform sampler2D uTex; void main(){ o = vec4(texture(uTex, vec2(vUV.x, 1.0 - vUV.y)).rgb, 1.0); }\n";
// FXAA (after Timothy Lottes' "FXAA 3.11 console" variant)
static const char *FS_FXAA =
  "#version 330 core\nin vec2 vUV; out vec4 o; uniform sampler2D uTex; uniform vec2 uPx;\n"
  "float L(vec3 c){ return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
  "void main(){ vec2 uv = vec2(vUV.x, 1.0 - vUV.y);\n"
  "  vec3 rgbM = texture(uTex, uv).rgb;\n"
  "  float lNW = L(texture(uTex, uv + vec2(-1.0,-1.0) * uPx).rgb), lNE = L(texture(uTex, uv + vec2(1.0,-1.0) * uPx).rgb);\n"
  "  float lSW = L(texture(uTex, uv + vec2(-1.0, 1.0) * uPx).rgb), lSE = L(texture(uTex, uv + vec2(1.0, 1.0) * uPx).rgb), lM = L(rgbM);\n"
  "  float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE))), lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));\n"
  "  if (lMax - lMin < max(0.0312, lMax * 0.125)) { o = vec4(rgbM, 1.0); return; }\n"
  "  vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), ((lNW + lSW) - (lNE + lSE)));\n"
  "  float red = max((lNW + lNE + lSW + lSE) * 0.03125, 1.0 / 128.0);\n"
  "  float rcp = 1.0 / (min(abs(dir.x), abs(dir.y)) + red);\n"
  "  dir = clamp(dir * rcp, vec2(-8.0), vec2(8.0)) * uPx;\n"
  "  vec3 a = 0.5 * (texture(uTex, uv + dir * (1.0/3.0 - 0.5)).rgb + texture(uTex, uv + dir * (2.0/3.0 - 0.5)).rgb);\n"
  "  vec3 b = a * 0.5 + 0.25 * (texture(uTex, uv + dir * -0.5).rgb + texture(uTex, uv + dir * 0.5).rgb);\n"
  "  float lB = L(b); o = vec4((lB < lMin || lB > lMax) ? a : b, 1.0); }\n";
static void post_ensure(int w, int h) {
  if (!g_post.prog_copy) {
    g_post.prog_copy = shader_build(VS_FULL, FS_COPY); g_post.uc_tex = glGetUniformLocation(g_post.prog_copy, "uTex"); g_post.uc_rect = glGetUniformLocation(g_post.prog_copy, "uRect");
    g_post.prog_fxaa = shader_build(VS_FULL, FS_FXAA); g_post.uf_tex = glGetUniformLocation(g_post.prog_fxaa, "uTex"); g_post.uf_rect = glGetUniformLocation(g_post.prog_fxaa, "uRect"); g_post.uf_px = glGetUniformLocation(g_post.prog_fxaa, "uPx");
  }
  if (g_post.fbo && g_post.w == w && g_post.h == h) return;
  if (g_post.fbo) { glDeleteFramebuffers(1, &g_post.fbo); glDeleteTextures(1, &g_post.tex); glDeleteRenderbuffers(1, &g_post.depth); }
  glGenTextures(1, &g_post.tex); glBindTexture(GL_TEXTURE_2D, g_post.tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &g_post.fbo); glBindFramebuffer(GL_FRAMEBUFFER, g_post.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_post.tex, 0);
  glGenRenderbuffers(1, &g_post.depth); glBindRenderbuffer(GL_RENDERBUFFER, g_post.depth);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, 0, GL_DEPTH_COMPONENT24, w, h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_post.depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) sys_log("Error: framebuffer incomplete");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  g_post.w = w; g_post.h = h;
}
static void post_present(int W, int H, int dx, int dy, int dw, int dh, int fxaa, int nearest) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, W, H); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_post.tex);
  int filter=nearest?G.gfx_output_filter:0;
  int f = nearest ? (filter==1||filter==5?GL_LINEAR:GL_NEAREST) : (dw == g_post.w && dh == g_post.h)?GL_NEAREST:GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
  glBindVertexArray(R.empty_vao);
  // rect in NDC; the picture is upside down in the texture (top-left origin is handled in the shaders)
  float x0 = -1 + 2.0f * dx / W, x1 = -1 + 2.0f * (dx + dw) / W, y0 = -1 + 2.0f * dy / H, y1 = -1 + 2.0f * (dy + dh) / H;
  if(filter>=2 && output_filter_draw(filter,g_post.w,g_post.h,dx,dy,dw,dh,x0,y0,x1,y1))return;
  if (fxaa) { glUseProgram(g_post.prog_fxaa); glUniform1i(g_post.uf_tex, 0); glUniform2f(g_post.uf_px, 1.0f / g_post.w, 1.0f / g_post.h); glUniform4f(g_post.uf_rect, x0, y0, x1, y1); }
  else { glUseProgram(g_post.prog_copy); glUniform1i(g_post.uc_tex, 0); glUniform4f(g_post.uc_rect, x0, y0, x1, y1); }
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static int g_game_ready, g_shot_wanted, g_game_frames;
static int game_mode_frame(int W, int H, float dt, const Input *in) {
  if (A.shot_exit) dt = 1.0f / (A.shot_fps > 0 ? A.shot_fps : 30);   // deterministic for screenshots (test: other output rates)
  Input demo_in;
  if (A.demo_time > 0) {   // test mode: no keyboard needed
    mem_zero(&demo_in, sizeof demo_in); demo_in.mx = in->mx; demo_in.my = in->my; in = &demo_in;
  }
  // graphics pipeline: the game draws into an off-screen picture (window size in "Revived", 640x480 or
  // 320x400 in "Retro Mode"), optionally with MSAA; it is then shown with FXAA or integer scaling
  graphics_options_apply();
  int old = G.gfx_mode == 1;
  int racing = G.state == GS_RACE || G.state == GS_REPLAY;
  int RW = old ? (racing ? 320 : 640) : W, RH = old ? (racing ? 400 : 480) : H;
  int dw = W, dh = H, dx = 0, dy = 0;
  if(old)retro_output_rect(W,H,G.gfx_scale,G.gfx_intscale,&dx,&dy,&dw,&dh);
  g_scr_fill = old; g_tex_nearest = old; g_smooth_filter = old ? 0 : G.opts_gfx == 0;
  int want = old ? 0 : (G.gfx_msaa > R.max_samples ? R.max_samples : G.gfx_msaa); if (want < 2) want = 0;
  R.samples = want;
  if(sys_debug_enabled()){
    static int previous_mode=-1,previous_w,previous_h;
    if(previous_mode!=old||previous_w!=RW||previous_h!=RH){
      previous_mode=old;previous_w=RW;previous_h=RH;char event[160]="[event] Render target: ";str_int(event,sizeof event,RW);str_cat(event,"x",sizeof event);str_int(event,sizeof event,RH);str_cat(event,old?"; Retro Mode; MSAA/FXAA disabled; Original framerate.":"; Revived.",sizeof event);sys_log(event);
    }
  }
  post_ensure(RW, RH);
  fbo_ensure(RW, RH);
  glBindFramebuffer(GL_FRAMEBUFFER, R.samples ? R.fbo : g_post.fbo);
  Input min = *in;   // mouse in picture coordinates
  min.mx = (int)((in->mx - dx) * (float)RW / dw); min.my = (int)((in->my - dy) * (float)RH / dh);
  min.mouse_dx = in->mouse_dx; min.mouse_dy = in->mouse_dy;
  // Automated captures must not depend on the user's live desktop pointer/keys.
  if(A.shot_exit)mem_zero(&min,sizeof min);
  int alive = game_frame(RW, RH, dt, &min);
  texture_dump_overlay_draw(RW,RH);
  if (R.samples) { glBindFramebuffer(GL_READ_FRAMEBUFFER, R.fbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_post.fbo); glBlitFramebuffer(0, 0, RW, RH, 0, 0, RW, RH, GL_COLOR_BUFFER_BIT, GL_NEAREST); }
  GLuint original_post_texture=g_post.tex;
  if(old&&g_retro_palette_allowed)g_post.tex=retro_palette_apply();
  post_present(W, H, dx, dy, dw, dh, old?0:G.gfx_fxaa, old);
  g_post.tex=original_post_texture;
  g_post_done = 1;
  {   // fullscreen: the setting changes the window, F11 / Alt+Enter change the setting
    static int last = -1; int now = sys_is_fullscreen();
    if (!A.shot_exit) {
      if (G.gfx_full != last && last != -2) { if (now != G.gfx_full) sys_set_fullscreen(G.gfx_full); last = G.gfx_full; }
      else if (now != G.gfx_full) { G.gfx_full = now; last = now; setup_save(); }
    }
  }
  if (sys_debug_enabled() && A.audiolog) {
    static double t0 = -1, last = 0; double now = sys_time(); if (t0 < 0) t0 = now;
    if (now - last >= 1.0) { last = now;
      char m[256] = "audio: wall_ms="; str_int(m, 256, (long)((now - t0) * 1000)); str_cat(m, " filled_ms=", 256); str_int(m, 256, (long)(MX.filled * 1000 / 22050));
      str_cat(m, " peak=", 256); str_int(m, 256, MX.peak); str_cat(m, " clipped=", 256); str_int(m, 256, MX.clipped);
      str_cat(m, " state=", 256); str_int(m, 256, G.state); str_cat(m, " dt_ms=", 256); str_int(m, 256, (long)(dt * 1000));
      if (G.state == GS_CLIP && G.clip.nframes) { str_cat(m, " clip_t_ms=", 256); str_int(m, 256, (long)(G.clip_t * 1000)); str_cat(m, " vpos_ms=", 256); str_int(m, 256, (long)(MX.v[1].pos * 1000 / 22050));
        str_cat(m, " vact=", 256); str_int(m, 256, MX.v[1].active); str_cat(m, " frames=", 256); str_int(m, 256, G.clip.nframes); str_cat(m, " pcm=", 256); str_int(m, 256, G.clip.pcm_frames); }
      sys_log(m); MX.peak = 0; MX.clipped = 0; }
  }
  if (A.debug_console) {   // OpenMR2.exe -debug: OpenGL errors and state changes
    static int last_state = -1;
    const char *state_name=game_state_name(G.state);
    if (G.state != last_state) { char m[128] = ""; str_cpy(m, ui_debug_text("[Zustand] "), 128); str_cat(m, state_name, 128); if (G.state == GS_CLIP && G.clip.nframes) { str_cat(m, "  (", 128); str_int(m, 128, G.clip.nframes); str_cat(m, ui_debug_text(" Bilder)"), 128); } sys_log(m); last_state = G.state; }
    GLenum e = glGetError(); if (e) { char m[64] = ""; str_cpy(m, ui_debug_text("[OpenGL Fehler] 0x"), 64); static const char *hx = "0123456789abcdef"; char h[5] = {hx[(e >> 12) & 15], hx[(e >> 8) & 15], hx[(e >> 4) & 15], hx[e & 15], 0}; str_cat(m, h, 64); sys_log(m); }
  }

  if (A.quit_after > 0) { static double tq = -1; double now = sys_time(); if (tq < 0) tq = now; if (now - tq > A.quit_after) return 0; }
  g_game_frames++;
  if (g_shot_wanted) {
    int ready;
    if (A.start_laps > 0) ready = G.state == GS_RESULTS && G.t > 0.5;
    else if (G.state == GS_RACE) { ready = RC.t >= (A.demo_time > 0 ? A.demo_time : 1.0);
      if (!str_icmp(A.start_screen, "pause") && ready && !RC.paused) { RC.paused = 1; RC.pause_sel = 1; ready = 0; static int w; w = 0; }
      if (RC.paused) { static int pw; ready = ++pw > 15; } }
    else if (G.state == GS_REPLAY) { static int rw; ready = ++rw > 60; }
    else if (G.state == GS_CLIP) ready = G.clip_t >= (A.demo_time > 0 ? A.demo_time : 2.0);
    else ready = A.graphics_test==20&&G.state==GS_TRACKINFO?1:g_game_frames > 20;
    if (ready) { A.shot_pending = 1; g_shot_wanted = 0; }
  }
  if(A.native_shot_path[0]&&A.shot_pending&&A.frames_rendered+1>=(A.shot_after>0?A.shot_after:2)){
    glBindFramebuffer(GL_READ_FRAMEBUFFER,old&&g_retro_palette_allowed?g_retro_palette.fbo:g_post.fbo);write_bmp(A.native_shot_path,g_post.w,g_post.h);A.native_shot_path[0]=0;glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
  }
  if (!frame_finish(W, H)) return 0;
  A.title[0] = 0; str_cat(A.title, OPENMR2_FULLTITLE, 256);
  if (G.state == GS_RACE) { str_cat(A.title, "  -  ", 256); str_cat(A.title, ui_translate(TRACKS[TK.idx].name), 256); }
  if (!alive) return 0;
  return 1;
}

static void mode_set(int mode) {
  if (mode != MODE_VIEWER && (!iso_is_open() || !A.have_mexe || !g_game_ready)) {
    str_cpy(A.status, ui_translate("Fuer das Spiel werden MEGARACE2.gog und M.EXE im Spielordner benoetigt."), 256);
    return;
  }
  if (A.mode == mode) return;
  if (A.mode == MODE_FMV) { voice_stop(); if (FV.vid.data) { video_close(&FV.vid); FV.loaded = 0; } }
  if (A.mode == MODE_TRACK) { snd_stop_all_sfx(); music_stop(); voice_stop(); if (G.state == GS_RACE) RC.paused = 1; G.menu_music_on = 0; }
  A.mode = mode;
  if (mode == MODE_FMV && FV.n) fmv_open(FV.cur);
  if (mode == MODE_TRACK && G.state != GS_RACE && G.state != GS_CLIP) go(G.state == GS_QUIT ? GS_MENU : G.state);
}

static void media_init(void) {
  g_prog_video = shader_build(VS_FULL, FS_VIDEO);
  uv_tex = glGetUniformLocation(g_prog_video, "uTex"); uv_rect = glGetUniformLocation(g_prog_video, "uRect"); uv_gray = glGetUniformLocation(g_prog_video, "uGray");
  uv_warp = glGetUniformLocation(g_prog_video, "uWarp"); uv_proj = glGetUniformLocation(g_prog_video, "uProj"); uv_warpon = glGetUniformLocation(g_prog_video, "uWarpOn");
  g_prog_canvas = shader_build(VS_FULL, FS_CANVAS);
  uc_sdf=glGetUniformLocation(g_prog_canvas,"uSDF");uc_clip=glGetUniformLocation(g_prog_canvas,"uClip");uc_uv=glGetUniformLocation(g_prog_canvas,"uUV");uc_tint=glGetUniformLocation(g_prog_canvas,"uTint");uc_tex = glGetUniformLocation(g_prog_canvas, "uTex"); uc_rect = glGetUniformLocation(g_prog_canvas, "uRect");
  g_cv = (uint32_t *)sys_alloc(CW * CH * 4);
  char p[512];
  const char *isonames[] = {"/MEGARACE2.gog", "/MEGARACE2.GOG", "/megarace2.gog", "/MEGARACE2.iso", "/game.gog", "/game.iso"};
  for (unsigned i = 0; i < sizeof isonames / sizeof isonames[0] && !iso_is_open(); i++) { str_cpy(p, g_gamedir, 512); str_cat(p, isonames[i], 512); iso_open(p); }
  str_cpy(p, g_gamedir, 512); str_cat(p, "/M.EXE", 512);
  A.have_mexe = mexe_load(p);
  if (!iso_is_open()) sys_log("CD image (MEGARACE2.gog) not found - game and videos disabled");
  if (!A.have_mexe) sys_log("M.EXE not usable - game and videos disabled");
  g_rng = (unsigned)(sys_time() * 1000) | 1;
  MX.dev = sys_audio_open(22050);
  if (iso_is_open() && A.have_mexe) { game_init();if(A.export_textures)texture_dump_start(); g_game_ready = 1; }
}

static void app_start_media(void) {
  if (A.debug_console) sys_log(OPENMR2_FULLTITLE " - Debug console (asset loads and events; errors in red)");
  media_init();
  if (A.shot_exit) { g_rng = 12345; }
  if (A.start_mode == MODE_FMV) { mode_set(MODE_FMV); return; }
  if (!g_game_ready || A.start_mode == MODE_VIEWER_FORCED) { A.mode = MODE_VIEWER; return; }
  A.mode = MODE_TRACK;
  if (A.shot_exit) { A.shot_pending = 0; g_shot_wanted = 1; }
  // test entry points
  if (A.dumpvideo > 0) {   // test tool: all frames of a track video as 80x88 grey thumbnails
    int ti = (A.dumpvideo - 1) % 6;
    if (track_load(ti, 0)) {
      int n = TK.vid.nframes; uint8_t *out = (uint8_t *)sys_alloc((size_t)n * 80 * 88);
      for (int f = 0; f < n; f++) { video_decode(&TK.vid, f);
        for (int y = 0; y < 88; y++) for (int x = 0; x < 80; x++) { int sum = 0; for (int yy = 0; yy < 4; yy++) for (int xx = 0; xx < 4; xx++) { int py = y * 4 + yy; if (py >= TK.vid.h) py = TK.vid.h - 1; const uint8_t *c = TK.vid.rgba + (py * TK.vid.w + x * 4 + xx) * 4; sum += c[0] * 3 + c[1] * 6 + c[2]; } out[(size_t)f * 7040 + y * 80 + x] = (uint8_t)(sum / 160); } }
      sys_write_file("videodump.raw", out, (size_t)n * 7040);
    }
    G.state = GS_QUIT; return;
  }
  if (!str_icmp(A.start_screen, "wrongway")) g_test_wrongway = 1;
  if (!str_icmp(A.start_screen, "edge")) g_test_wrongway = 2;
  if (A.retro_scale) { graphics_mode_set(1); G.gfx_scale = A.retro_scale; }
  if (A.fxaa_test) G.gfx_fxaa = 1;
  if (A.start_screen[0] && str_icmp(A.start_screen, "pause") && str_icmp(A.start_screen, "wrongway") && str_icmp(A.start_screen, "edge")) {
    const char *s = A.start_screen;
    if (!str_icmp(s, "fonts")) { G.state = GS_FONTTEST; return; }
    if (!str_icmp(s, "menu")) { go(GS_MENU); return; }
    if(!str_icmp(s,"credits")){go(GS_MENU);credits_play_then(GS_MENU);G.clip_t=A.start_frame/25.0;return;}
    if (s[0] == 'c' && s[1] == 'l' && s[2] == 'i' && s[3] == 'p' && s[4] == ':') { go(GS_MENU); clip_queue(s + 5); clip_play_then(GS_MENU); return; }
    profile_new(); PF.car = 2; PF.field[0] = 2; default_loadout(&PF.lo); PF.money = 9000; PF.damage = 35;
    if (!str_icmp(s, "carshop")) {G.shop_car=A.start_car>=0 && A.start_car<NCARS?A.start_car:0;go(GS_CARSHOP);}
    else if (!str_icmp(s,"single")){G.single_car=A.start_car>=0&&A.start_car<NCARS?A.start_car:0;go(GS_TRACKSELECT);}
    else if (!str_icmp(s, "shop")) go(GS_WEAPONSHOP);
    else if(!str_icmp(s,"shopmessage")){go(GS_CARSHOP);show_message("Dieser Wagen ist noch nicht erh\xe4ltlich.",GS_CARSHOP);}
    else if(!str_icmp(s,"extras"))go(GS_EXTRAS);
    else if (!str_icmp(s, "info")) go(GS_TRACKINFO);
    else if (!str_icmp(s, "options")) go(GS_OPTIONS);
    else if (!str_icmp(s, "sound")) go(GS_RETRO_SOUND);
    else if (!str_icmp(s, "hiscore")) go(GS_HISCORE);
    else if (!str_icmp(s, "select")) go(GS_TRACKSELECT);
    else if (!str_icmp(s, "save")) go(GS_SAVELOAD);
    else if (!str_icmp(s, "prize")) go(GS_PRIZE);
    else if (!str_icmp(s, "gameover")) go(GS_GAMEOVER);
    else if (!str_icmp(s, "intro")) game_start();
    else if (!str_icmp(s, "mksave")) { PF.active = 1; save_game(); go(GS_MENU); }   // test: championship with a car saved
    else if (!str_icmp(s, "quit")) { go(GS_MENU); G.state = GS_QUITDLG; }
    else if (!str_icmp(s, "menu2")) go(GS_MENU);
    else if (!str_icmp(s, "replays")) go(GS_REPLAYS);
    else if (!str_icmp(s,"effects")) {go(GS_OPTIONS);G.state=GS_REVIVE_EFFECTS;}
    else if (!str_icmp(s, "gfx")) { go(GS_OPTIONS); G.state = GS_GFX; }
    else if (!str_icmp(s, "debug")) { go(GS_OPTIONS); G.state = GS_DEBUG; }
    else if (!str_icmp(s, "credits")) { go(GS_MENU); credits_play_then(GS_MENU); }
    else if (!str_icmp(s, "padmap")) { go(GS_OPTIONS); G.state = GS_PADMAP; G.pad_wait = -1; }
    else if (!str_icmp(s, "replay1")) { go(GS_REPLAYS); if (replay_open_slot(0)) replay_start(GS_REPLAYS); }
    else go(GS_MENU);
    return;
  }
  if (A.start_track > 0) {   // --track N: single race on championship track N (1..6)
    profile_new(); PF.single = 1; PF.car = A.start_car >= 0 && A.start_car < NCARS ? A.start_car : 2; PF.field[0] = PF.car; default_loadout(&PF.lo);
    PF.lo.front[1] = W_HITRAX; PF.lo.fammo[1] = 5; PF.lo.rear[1] = W_OIL; PF.lo.rammo[1] = 3; PF.lo.shield = 1;
    G.single_track = (A.start_track - 1) % 6;
    int pool[NCARS], n = 0; for (int i = 0; i < NCARS; i++) if (i != PF.car) pool[n++] = i;
    for (int i = 1; i < 8; i++) PF.field[i] = pool[i - 1];
    G.opts_clips = 0; if (A.start_laps > 0) { G.opts_laps = A.start_laps; G.single_laps = A.start_laps; }
    start_race();
    return;
  }
  game_start();
}

// ------------------------------------------------------------------ frame pacing of the race
// Original: one picture per original update (FIX 40 ms, MOB 0x1B0000 / camera speed, 5 ms ticks), with vsync.
// 60fps: the same updates, the last package is shown more often (vsync off).
static double g_frame_sys, g_next60;
static int race_paced(void) { return A.mode == MODE_TRACK && G.state == GS_RACE && RC.active && !RC.paused && RC.phase != PH_FLYBY && !A.shot_exit; }
int app_swap_interval(void) { return race_paced() && g_out_mode != 0 ? 0 : 1; }
double app_wait_time(void) {
  if (!race_paced()) return 0;
  double now = sys_time();
  if (g_out_mode == 0) return race_next_due() - (now - g_frame_sys);
  if (g_out_mode == 1) { if (g_next60 < now - 0.05 || g_next60 > now + 0.05) g_next60 = now; return g_next60 - now; }
  return 0;
}
static void pacing_frame_start(void) {   // called by app_frame: the race clock advances with this frame
  g_frame_sys = sys_time();
  {   // output mode changed: reset only the output plan (deadline, FPS window), never the simulation
    static int last_mode = -1;
    if (g_out_mode != last_mode) { last_mode = g_out_mode; g_next60 = g_frame_sys; A.fps_acc = 0; A.fps_n = 0; }
  }
  if (g_out_mode == 1) { g_next60 += 1.0 / 60; if (g_next60 < g_frame_sys) g_next60 = g_frame_sys + 1.0 / 60; }
}

#include "recording.inc"
#include "global_shortcuts.inc"
