#include "game_directory.h"
static int lx_absolute(const char *path,char *out,size_t cap){
  // realpath requires PATH_MAX storage when a buffer is supplied. Let libc allocate it.
  char *absolute=realpath(path,0);if(!absolute)return 0;
  size_t n=strlen(absolute);int ok=n<cap;if(ok)memcpy(out,absolute,n+1);free(absolute);return ok;
}
static Display *verify_display;static Window verify_window;static GC verify_gc;
static void lx_verify_update(const char *path,uint64_t done,uint64_t total){
  while(XPending(verify_display)){XEvent e;XNextEvent(verify_display,&e);}
  XSetForeground(verify_display,verify_gc,0x202020);XFillRectangle(verify_display,verify_window,verify_gc,0,0,800,200);
  XSetForeground(verify_display,verify_gc,0xffffff);
  const char *lines[]={"Checking original game data using SHA-256. Please wait.","This runs once for this folder. Future launches only check that files exist.",path,"Progress for the current file:"};
  for(int i=0;i<4;i++){const char *s=lines[i];size_t n=strlen(s);if(n>105)s+=n-105;XDrawString(verify_display,verify_window,verify_gc,15,25+i*30,s,(int)strlen(s));}
  XDrawRectangle(verify_display,verify_window,verify_gc,15,135,765,24);XFillRectangle(verify_display,verify_window,verify_gc,17,137,total?(unsigned)(done*761/total):0,20);XFlush(verify_display);
}
static void lx_verify_begin(){
  verify_window=XCreateSimpleWindow(verify_display,DefaultRootWindow(verify_display),100,150,800,185,1,0xffffff,0x202020);
  XStoreName(verify_display,verify_window,"OpenMR2 - one-time game data verification");XSelectInput(verify_display,verify_window,ExposureMask);XMapRaised(verify_display,verify_window);verify_gc=XCreateGC(verify_display,verify_window,0,0);
  sha256_progress=lx_verify_update;lx_verify_update("Preparing file verification...",0,1);
}
static void lx_verify_end(){sha256_progress=0;XFreeGC(verify_display,verify_gc);XDestroyWindow(verify_display,verify_window);XFlush(verify_display);}
static int lx_cfg_path(char *out,size_t cap) {
  size_t n=0;unsigned char *text=(unsigned char *)sys_read_file("revived_config/openmr2_setup.cfg",&n);
  int ok=game_directory_from_config(text,n,out,(int)cap);sys_free(text);return ok;
}
static void lx_store_path(const char *path) {
  size_t n=0;char *old=(char *)sys_read_file("revived_config/openmr2_setup.cfg",&n);char *result=(char *)sys_alloc(n+1200);
  int len=snprintf(result,n+1200,"# Original game data directory\ngame_directory = %s\n",path);
  if(old){char *p=old;while(*p){char *end=strchr(p,'\n');size_t count=end?(size_t)(end-p)+1:strlen(p);if(strncmp(p,"game_directory",14)&&strncmp(p,"spielverzeichnis",16)&&strncmp(p,"# Original game data directory",30)){memcpy(result+len,p,count);len+=(int)count;}p+=count;}}
  sys_write_file("revived_config/openmr2_setup.cfg",result,len);sys_free(result);sys_free(old);
}
// Xlib path-selection dialog, without a mandatory GTK/Qt/zenity dependency.
static int lx_choose_path(Display *display,char *path,size_t cap) {
  Window win=XCreateSimpleWindow(display,DefaultRootWindow(display),100,100,760,170,1,0xffffff,0x202020);
  XStoreName(display,win,"OpenMR2 - select GOG game folder");XSelectInput(display,win,ExposureMask|KeyPressMask|StructureNotifyMask);
  Atom close=XInternAtom(display,"WM_DELETE_WINDOW",False);XSetWMProtocols(display,win,&close,1);XMapRaised(display,win);
  GC gc=XCreateGC(display,win,0,0);int done=0,ok=0;char status[256]="Enter the folder path. Enter: validate and select. Esc: cancel.";
  while(!done){XEvent e;XNextEvent(display,&e);
    if(e.type==ClientMessage)done=1;
    if(e.type==KeyPress){char text[64];KeySym sym;int count=XLookupString(&e.xkey,text,sizeof text,&sym,0);size_t n=strlen(path);
      if(sym==XK_Escape)done=1;
      else if(sym==XK_Return){char detail[2000];char absolute[700];if(lx_absolute(path,absolute,sizeof absolute))snprintf(path,cap,"%s",absolute);if(!gf_startup_check(path,x_exists,detail,sizeof detail)){done=ok=1;}else snprintf(status,sizeof status,"%s",gf_status==GOG_INCOMPLETE?"Recognized GOG installation is incomplete or damaged. Check the selected folder.":"Unsupported version or language. Select German, English or French GOG data.");}
      else if(sym==XK_BackSpace){if(n)path[n-1]=0;}
      else if(count>0 && n+(size_t)count<cap){memcpy(path+n,text,count);path[n+count]=0;}
      XClearWindow(display,win);
    }
    XSetForeground(display,gc,0xffffff);XDrawString(display,win,gc,15,25,"MegaRace 2 GOG data folder",25);
    XDrawRectangle(display,win,gc,10,45,735,30);const char *shown=path;size_t length=strlen(shown);if(length>100)shown+=length-100;
    XDrawString(display,win,gc,15,65,shown,(int)strlen(shown));XDrawString(display,win,gc,15,110,status,(int)strlen(status));XFlush(display);
  }
  XFreeGC(display,gc);XDestroyWindow(display,win);return ok;
}
