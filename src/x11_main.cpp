// Linux/X11 + GLX platform layer (used for testing; GL is loaded at runtime from libGL.so.1)
#include "sys.h"
#include "gl.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include "gamefiles.h"
int sys_game_language(void) { return gf_language; }
static int x_exists(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }
static Display *g_display;static Window g_window;static int g_fullscreen;
#include "startup_x11.inc"
#include "startup_choice.h"

void *sys_alloc(size_t n) { return calloc(1, n ? n : 1); }
void *sys_realloc(void *p, size_t n) { return realloc(p, n); }
void sys_free(void *p) { free(p); }
uint8_t *sys_read_file(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *b = (uint8_t *)malloc(n + 1); if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return 0; }
  fclose(f); b[n]=0; *size = n; sys_log_asset(path); return b;
}
int sys_delete_file(const char *path) { return remove(path) == 0; }
int sys_write_file(const char *path, const void *data, size_t size) {
  FILE *f = fopen(path, "wb");
  if (!f) { char d[600]; strncpy(d, path, 599); d[599] = 0; for(char *sl=d+1;*sl;sl++)if(*sl=='/'){*sl=0;mkdir(d,0755);*sl='/';} f = fopen(path, "wb"); }
  if (!f) return 0; fwrite(data, 1, size, f); fclose(f); return 1;
}
int sys_dir_exists(const char *path) { struct stat st; return stat(path, &st) == 0 && S_ISDIR(st.st_mode); }
void sys_list_dir(const char *dir, const char *ext, void (*cb)(void *, const char *, int), void *user) {
  DIR *d = opendir(dir); if (!d) return;
  struct dirent *e; int want_dirs = !strcmp(ext, "/");
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char p[1024]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
    int isdir = sys_dir_exists(p);
    if (want_dirs) { if (isdir) cb(user, e->d_name, 1); continue; }
    size_t ln = strlen(e->d_name), le = strlen(ext);
    if (!isdir && ln > le && !strcasecmp(e->d_name + ln - le, ext)) cb(user, e->d_name, 0);
  }
  closedir(d);
}
static int debug_enabled;
int sys_debug_enabled(){return debug_enabled;}
void sys_log(const char *msg) { if(debug_enabled)fprintf(stderr, "%s\n", msg); }
int sys_debug_console(int enabled) { return debug_enabled=enabled?1:0; }
double sys_time(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int sys_render_text(const char *text,int,uint8_t **rgba,int *w,int *h) {
  if(!g_display)return 0;static XFontStruct *font;if(!font)font=XLoadQueryFont(g_display,"fixed");if(!font)return 0;
  int width=0,line=0,lines=1;for(const char *s=text;*s;s++){if(*s=='\n'){if(line>width)width=line;line=0;lines++;}else line++;}if(line>width)width=line;
  width=width*font->max_bounds.width+2;int height=lines*(font->ascent+font->descent)+2;
  if(width<1||width>2048||height<1||height>1024)return 0;
  Pixmap pixmap=XCreatePixmap(g_display,DefaultRootWindow(g_display),width,height,1);GC gc=XCreateGC(g_display,pixmap,0,0);
  XSetForeground(g_display,gc,0);XFillRectangle(g_display,pixmap,gc,0,0,width,height);XSetForeground(g_display,gc,1);XSetFont(g_display,gc,font->fid);
  const char *start=text;int y=font->ascent+1;
  for(const char *s=text;;s++)if(!*s||*s=='\n'){XDrawString(g_display,pixmap,gc,1,y,start,(int)(s-start));y+=font->ascent+font->descent;if(!*s)break;start=s+1;}
  XImage *image=XGetImage(g_display,pixmap,0,0,width,height,1,ZPixmap);XFreeGC(g_display,gc);XFreePixmap(g_display,pixmap);if(!image)return 0;
  uint8_t *pixels=(uint8_t *)sys_alloc((size_t)width*height*4);
  for(int yy=0;yy<height;yy++)for(int xx=0;xx<width;xx++){uint8_t *p=pixels+(yy*width+xx)*4;p[0]=p[1]=p[2]=255;p[3]=XGetPixel(image,xx,yy)?255:0;}
  XDestroyImage(image);*rgba=pixels;*w=width;*h=height;return 1;
}
void *sys_fopen(const char *path) { return fopen(path, "rb"); }
int sys_fread_at(void *f, uint64_t off, void *buf, uint32_t n) { if (fseeko((FILE *)f, (off_t)off, SEEK_SET)) return 0; return fread(buf, 1, n, (FILE *)f) == n; }
uint64_t sys_fsize(void *f) { fseeko((FILE *)f, 0, SEEK_END); return (uint64_t)ftello((FILE *)f); }
void sys_fclose(void *f) { fclose((FILE *)f); }
#include "gamedir_x11.inc"
#include "encoder_x11.inc"
#include "audio_pulse.inc"
int sys_pad_read(PadState *p) { p->connected = 0; return 0; }
void sys_set_fullscreen(int on) {
  if(!g_display || !g_window)return;
  XEvent event;memset(&event,0,sizeof event);event.type=ClientMessage;event.xclient.window=g_window;
  event.xclient.message_type=XInternAtom(g_display,"_NET_WM_STATE",False);event.xclient.format=32;
  event.xclient.data.l[0]=on?1:0;event.xclient.data.l[1]=XInternAtom(g_display,"_NET_WM_STATE_FULLSCREEN",False);event.xclient.data.l[3]=1;
  XSendEvent(g_display,DefaultRootWindow(g_display),False,SubstructureRedirectMask|SubstructureNotifyMask,&event);XFlush(g_display);g_fullscreen=on;
}
int sys_is_fullscreen(void) { return g_fullscreen; }

typedef void *GLXFBConfig;
typedef void *GLXContext;
static void *libgl;
static void *(*p_glXGetProcAddressARB)(const char *);
static GLXFBConfig *(*p_glXChooseFBConfig)(Display *, int, const int *, int *);
static XVisualInfo *(*p_glXGetVisualFromFBConfig)(Display *, GLXFBConfig);
static int (*p_glXMakeCurrent)(Display *, Window, GLXContext);
static void (*p_glXSwapBuffers)(Display *, Window);
typedef GLXContext (*PFNCCA)(Display *, GLXFBConfig, GLXContext, int, const int *);

static void *getproc(const char *n) {
  void *p = p_glXGetProcAddressARB(n);
  if (!p) p = dlsym(libgl, n);
  return p;
}

int main(int argc, char **argv) {
  const char *gamedir = ".";char selected[700]={};int explicit_game=0,validate_only=0;
  int W = 1280, H = 720;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--validate"))validate_only=1;
    else if (!strcmp(argv[i], "--game") && i + 1 < argc){gamedir = argv[++i];explicit_game=1;}
    else if (!strcmp(argv[i], "--program-directory") && i+1<argc)gamedir=argv[++i];
    else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &W, &H);
  }
  char details[10000];int invalid=0;
  if(validate_only){invalid=gf_check(gamedir,x_exists,details,sizeof details);printf("status = %s\nlanguage = %s\n%s",gf_status==GOG_COMPLETE?"complete":gf_status==GOG_INCOMPLETE?"incomplete":"unsupported",gf_language>=0?GOG_LANGUAGES[gf_language]:"unknown",details);return invalid?gf_status==GOG_INCOMPLETE?2:3:0;}
  Display *dpy=XOpenDisplay(0);if(!dpy){fprintf(stderr,"no X display\n");return 1;}
  {size_t cn=0;unsigned char *ct=sys_read_file("revived_config/openmr2_setup.cfg",&cn);char flag[16];cfg_value(ct,cn,"debug_console",flag,sizeof flag);sys_free(ct);sys_debug_console(flag[0]=='1');for(int i=1;i<argc;i++)if(!strcmp(argv[i],"--debug")||!strcmp(argv[i],"-debug"))sys_debug_console(1);}
  verify_display=dpy;gf_progress_begin=lx_verify_begin;gf_progress_end=lx_verify_end;
  static char absolute[700];if(lx_absolute(gamedir,absolute,sizeof absolute))gamedir=absolute;
  invalid=!gf_startup_candidate(gamedir,x_exists);
  if(!explicit_game && invalid && lx_cfg_path(selected,sizeof selected)){
    if(lx_absolute(selected,absolute,sizeof absolute))gamedir=absolute;else gamedir=selected;
    invalid=!gf_startup_candidate(gamedir,x_exists);
  }
  if(invalid && explicit_game){fprintf(stderr,"Game data validation failed.\n");return 2;}
  libgl = dlopen("libGL.so.1", RTLD_NOW);
  if (!libgl) { fprintf(stderr, "libGL.so.1 not found\n"); return 1; }
  p_glXGetProcAddressARB = (void *(*)(const char *))dlsym(libgl, "glXGetProcAddressARB");
  p_glXChooseFBConfig = (GLXFBConfig * (*)(Display *, int, const int *, int *)) dlsym(libgl, "glXChooseFBConfig");
  p_glXGetVisualFromFBConfig = (XVisualInfo * (*)(Display *, GLXFBConfig)) dlsym(libgl, "glXGetVisualFromFBConfig");
  p_glXMakeCurrent = (int (*)(Display *, Window, GLXContext))dlsym(libgl, "glXMakeCurrent");
  p_glXSwapBuffers = (void (*)(Display *, Window))dlsym(libgl, "glXSwapBuffers");

  if(invalid){if(!selected[0])snprintf(selected,sizeof selected,"%s",gamedir);if(!lx_choose_path(dpy,selected,sizeof selected))return 1;gamedir=selected;}
  lx_store_path(gamedir);
  g_display=dpy;if(!startup_before_window(argc,argv)){XCloseDisplay(dpy);return 0;}
  const int GLX_X_RENDERABLE = 0x8012, GLX_DRAWABLE_TYPE = 0x8010, GLX_WINDOW_BIT = 1, GLX_RENDER_TYPE = 0x8011, GLX_RGBA_BIT = 1,
            GLX_DOUBLEBUFFER = 5, GLX_RED_SIZE = 8, GLX_GREEN_SIZE = 9, GLX_BLUE_SIZE = 10, GLX_DEPTH_SIZE = 12;
  int fbattr[] = {GLX_X_RENDERABLE, 1, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DOUBLEBUFFER, 1,
                  GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 24, 0};
  int nfb = 0; GLXFBConfig *fbs = p_glXChooseFBConfig(dpy, DefaultScreen(dpy), fbattr, &nfb);
  if (!fbs || !nfb) { fprintf(stderr, "no fbconfig\n"); return 1; }
  XVisualInfo *vi = p_glXGetVisualFromFBConfig(dpy, fbs[0]);
  XSetWindowAttributes swa; memset(&swa, 0, sizeof swa);
  swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
  swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
  Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, W, H, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
  g_display=dpy;g_window=win;
  XMapWindow(dpy, win);
  PFNCCA cca = (PFNCCA)p_glXGetProcAddressARB("glXCreateContextAttribsARB");
  int ctxattr[] = {0x2091, 3, 0x2092, 3, 0x9126, 1, 0};
  GLXContext ctx = cca(dpy, fbs[0], 0, 1, ctxattr);
  if (!ctx) { fprintf(stderr, "could not create GL 3.3 core context\n"); return 1; }
  p_glXMakeCurrent(dpy, win, ctx);
  const char *miss = gl_load(getproc);
  if (miss) { fprintf(stderr, "missing GL function %s\n", miss); return 1; }
  if (!app_init(gamedir, argc, argv)) return 1;
  Atom wmdel = XInternAtom(dpy, "WM_DELETE_WINDOW", False); XSetWMProtocols(dpy, win, &wmdel, 1);
  Input in; memset(&in, 0, sizeof in);
  double last = sys_time(); int lastx = 0, lasty = 0, have = 0, run = 1;
  char lasttitle[256] = "";
  while (run) {
    memset(in.pressed, 0, sizeof in.pressed); in.mouse_dx = in.mouse_dy = in.wheel = 0; in.lmb_pressed = in.rmb_pressed = 0;
    while (XPending(dpy)) {
      XEvent ev; XNextEvent(dpy, &ev);
      if (ev.type == ConfigureNotify) { W = ev.xconfigure.width; H = ev.xconfigure.height; }
      else if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0]==wmdel) run = 0;
      else if (ev.type == KeyPress || ev.type == KeyRelease) {
        KeySym ks = XLookupKeysym(&ev.xkey, 0); int k = 0;
        // F11 is recording, not fullscreen.
        switch (ks) { case XK_Left: k = K_LEFT; break; case XK_Right: k = K_RIGHT; break; case XK_Up: k = K_UP; break; case XK_Down: k = K_DOWN; break;
          case XK_Page_Up: k = K_PGUP; break; case XK_Page_Down: k = K_PGDN; break; case XK_space: k = K_SPACE; break; case XK_Escape: k = K_ESC; break;
          case XK_F12: k = K_F12; break; case XK_F1: k = K_F1; break; case XK_F2: k = K_F2; break; case XK_F3: k = K_F3; break; case XK_Return: k = K_ENTER; break; case XK_Tab: k = K_TAB; break; case XK_slash: k = K_SLASH; break; case XK_F4: k = K_F4; break; case XK_F5: k=K_F5; break; case XK_F6: k=K_F6; break; case XK_F7: k=K_F7; break; case XK_F8: k=K_F8; break; case XK_F9: k=K_F9; break; case XK_F10: k=K_F10; break; case XK_F11: k=K_F11; break; case XK_Home: k = K_HOME; break; case XK_plus: case XK_KP_Add: k = K_PLUS; break; case XK_minus: case XK_KP_Subtract: k = K_MINUS; break;
          default: if (ks >= XK_a && ks <= XK_z) k = 'A' + (ks - XK_a); else if (ks >= XK_0 && ks <= XK_9) k = '0' + (int)(ks - XK_0); }
        if (k) { if (ev.type == KeyPress) { if (!in.down[k]) in.pressed[k] = 1; in.down[k] = 1; } else in.down[k] = 0; }
      } else if (ev.type == ButtonPress || ev.type == ButtonRelease) {
        int d = ev.type == ButtonPress;
        in.mx = ev.xbutton.x; in.my = ev.xbutton.y;
        if (ev.xbutton.button == 1) { in.lmb = d; if (d) in.lmb_pressed = 1; } else if (ev.xbutton.button == 3) in.rmb = d;
        else if (ev.xbutton.button == 4 && d) in.wheel += 1; else if (ev.xbutton.button == 5 && d) in.wheel -= 1;
      } else if (ev.type == MotionNotify) {
        if (have) { in.mouse_dx += ev.xmotion.x - lastx; in.mouse_dy += ev.xmotion.y - lasty; }
        lastx = ev.xmotion.x; lasty = ev.xmotion.y; have = 1; in.mx = lastx; in.my = lasty;
      }
    }
    {   // vsync on / off as the core wants it (Original vs. 60 FPS); MESA or EXT extension if present
      static int cur = -2; int si = app_swap_interval();
      if (si != cur) { cur = si;
        typedef int (*PFNM)(unsigned); PFNM m = (PFNM)p_glXGetProcAddressARB("glXSwapIntervalMESA");
        if (m) m((unsigned)si);
        else { typedef void (*PFNE)(Display *, Window, int); PFNE e = (PFNE)p_glXGetProcAddressARB("glXSwapIntervalEXT"); if (e) e(dpy, win, si); } }
    }
    for (;;) { double w = app_wait_time(); if (w <= 0.0003) break; audio_pump();struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = (long)((w > 0.004 ? 0.004 : w) * 1e9); nanosleep(&ts, 0); }
    double now = sys_time(); float dt = (float)(now - last); last = now;
    audio_pump();if (!app_frame(W, H, dt, &in)) break;
    p_glXSwapBuffers(dpy, win);
    if (strcmp(lasttitle, app_title())) { strncpy(lasttitle, app_title(), 255); XStoreName(dpy, win, lasttitle); }
  }
  app_capture_stop();
  return 0;
}
