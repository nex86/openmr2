// Platform abstraction used by the portable core (implemented by win32_main.cpp and x11_main.cpp)
#pragma once
#include <stddef.h>
#include <stdint.h>

#define OPENMR2_NAME "OpenMR2"
#define OPENMR2_VERSION "0.7.14-dev"
#define OPENMR2_FULLTITLE "OpenMR2 MegaRace2 Revived  v" OPENMR2_VERSION

void *sys_alloc(size_t n);          // zeroed
void *sys_realloc(void *p, size_t n);
void sys_free(void *p);
// reads a whole file; returns 0 if not found. *size receives length.
uint8_t *sys_read_file(const char *path, size_t *size);
int sys_write_file(const char *path, const void *data, size_t size);   // creates a missing parent folder
int sys_delete_file(const char *path);
int sys_dir_exists(const char *path);
// list files in dir matching extension (e.g. ".M3D", case-insensitive). callback gets bare file name.
void sys_list_dir(const char *dir, const char *ext, void (*cb)(void *user, const char *name, int is_dir), void *user);
void sys_log(const char *msg);
int sys_debug_enabled(void);
static inline void sys_log_asset(const char *path){
  if(!sys_debug_enabled())return;
  const char *ext=0;for(const char *p=path;*p;p++)if(*p=='.')ext=p;
  if(!ext)return;const char *types[]={".hlz",".spr",".m3d",".ubb",".pal",".bmp",".png"};int match=0;
  for(int i=0;i<7;i++){const char *a=ext,*b=types[i];while(*a && *b && ((*a|32)==*b)){a++;b++;}if(!*a&&!*b)match=1;}
  if(!match)return;char m[800];int n=0;const char *prefix="[load] ";while(*prefix)m[n++]=*prefix++;while(*path&&n<798)m[n++]=*path++;m[n]=0;sys_log(m);
}
int sys_debug_console(int enabled); // Windows: show/hide game-owned console; Linux: detailed terminal logs.
double sys_time(void);
int sys_choose_mode(int current,const char *title,const char *question,const char *revived,const char *retro,const char *remember,int *save);
int sys_startup_mode();
int sys_game_is_gog();
int sys_run_dos(const char *directory,char *error,int cap);
int sys_game_language(void); // 0 English, 1 French, 2 German, -1 unknown
// Optional: rasterize UI text into an RGBA buffer (white text, alpha coverage). Return 0 if unsupported.
int sys_render_text(const char *text, int font_px, uint8_t **rgba, int *w, int *h);
// random access file reading (large files, e.g. the CD image)
void *sys_fopen(const char *path);
int sys_fread_at(void *f, uint64_t off, void *buf, uint32_t n);
uint64_t sys_fsize(void *f);
void sys_fclose(void *f);
// audio output: 22050 Hz stereo s16, pulled from app_audio_fill(). returns 0 if unavailable.
void sys_audio_flush(void); // discard queued audio at the Game Over boundary
int sys_audio_open(int rate);
void app_audio_fill(int16_t *dst, int frames);
// Game-owned FFmpeg child: raw bottom-up RGBA, 30 fps, mixed stereo PCM.
int sys_encoder_open(const char *path,int w,int h);
int sys_encoder_video(const void *rgba,size_t bytes);
void sys_encoder_audio(const int16_t *pcm,int frames);
int sys_encoder_close();
void app_capture_stop();

// game controller (XInput on Windows): first connected pad. buttons use the XInput bit layout.
struct PadState { int connected; unsigned buttons; float lx, ly, rx, ry, lt, rt; };
int sys_pad_read(PadState *p);   // returns 1 if a pad is connected
// fullscreen window (borderless, the size of the monitor)
void sys_set_fullscreen(int on);
int sys_is_fullscreen(void);

// ---- input from platform to core ----
enum Key {
  K_NONE = 0, K_LEFT, K_RIGHT, K_UP, K_DOWN, K_PGUP, K_PGDN, K_SPACE, K_ESC, K_F12, K_HOME,
  K_A = 'A', K_B, K_C, K_D, K_E, K_F, K_G, K_H, K_I, K_J, K_K, K_L, K_M, K_N, K_O, K_P, K_Q, K_R, K_S, K_T, K_U, K_V, K_W, K_X, K_Y, K_Z,
  K_PLUS = 200, K_MINUS, K_TAB, K_F1, K_F2, K_F3, K_ENTER, K_SLASH, K_BACKSPACE, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_COUNT = 256
};
struct Input {
  float mouse_dx, mouse_dy, wheel;
  int lmb, rmb, mmb;
  int mx, my;                      // absolute mouse position in window pixels
  int lmb_pressed, rmb_pressed;    // edge-triggered this frame
  unsigned char pressed[K_COUNT];  // edge-triggered this frame
  unsigned char down[K_COUNT];
};

// ---- core entry points ----
int app_init(const char *gamedir, int argc, char **argv);
// returns 0 to request quit
int app_frame(int width, int height, float dt, const Input *in);
const char *app_title(void);
// frame pacing requested by the core: swap interval (1 = vsync, 0 = off) and the time in seconds the
// platform should wait before it starts the next frame (0 = at once)
int app_swap_interval(void);
double app_wait_time(void);
int app_wants_screenshot_exit(void);
