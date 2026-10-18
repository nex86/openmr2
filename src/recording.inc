
static void shortcut_notice(const char *text){str_cpy(g_shortcut_notice,text,sizeof g_shortcut_notice);g_shortcut_until=sys_time()+3;g_shortcut_right=0;sys_log(text);}
static void capture_notice(const char *text){str_cpy(g_shortcut_notice,ui_translate(text),sizeof g_shortcut_notice);g_shortcut_until=sys_time()+3;g_shortcut_right=1;sys_log(text);}
struct Capture {int active,w,h,frames;double start;GLuint fbo,tex;u8 *pixels;char path[512];};
static Capture g_capture;
void app_capture_stop(){
  if(!g_capture.active)return;g_capture.active=0;
  int ok=sys_encoder_close();char message[600]="";str_cpy(message,ok?"Recording saved: ":"Recording failed; recovery files/log: ",sizeof message);str_cat(message,g_capture.path,sizeof message);sys_log(message);capture_notice(ok?"Recording stopped":"Recording failed");
  sys_free(g_capture.pixels);g_capture.pixels=0;if(g_capture.fbo)glDeleteFramebuffers(1,&g_capture.fbo);if(g_capture.tex)glDeleteTextures(1,&g_capture.tex);g_capture.fbo=g_capture.tex=0;
}
static void capture_toggle(int W,int H){
  if(g_capture.active){app_capture_stop();return;}
  float k=m_min(1.f,m_min(1920.f/W,1080.f/H));int w=(int)(W*k)&~1,h=(int)(H*k)&~1;if(w<2||h<2)return;
  char path[512];capture_filename(path,sizeof path,"revived_config/recordings",".mp4");
  // Creates the directory without overwriting any user media.
  if(!sys_write_file("revived_config/recordings/.directory","",0)){shortcut_notice("Cannot create revived_config/recordings.");return;}
  if(!sys_encoder_open(path,w,h)){shortcut_notice("Recording unavailable: install FFmpeg with libx264 (tools/ffmpeg.exe on Windows). See RECORDING.md.");return;}
  g_capture.w=w;g_capture.h=h;g_capture.frames=0;g_capture.start=sys_time();str_cpy(g_capture.path,path,sizeof g_capture.path);g_capture.pixels=(u8 *)sys_alloc((size_t)w*h*4);
  glGenTextures(1,&g_capture.tex);glBindTexture(GL_TEXTURE_2D,g_capture.tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glGenFramebuffers(1,&g_capture.fbo);glBindFramebuffer(GL_FRAMEBUFFER,g_capture.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,g_capture.tex,0);int ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;glBindFramebuffer(GL_FRAMEBUFFER,0);
  g_capture.active=1;if(!ok){app_capture_stop();shortcut_notice("Recording failed: capture framebuffer unavailable.");return;}
  int16_t silence[2]={};sys_encoder_audio(silence,1);sys_log("Recording started: H.264 / AAC, 30fps, up to 1080p. F11 stops.");capture_notice("Recording started");
}
static void capture_frame(int W,int H){
  Capture &c=g_capture;if(!c.active)return;int due=(int)((sys_time()-c.start)*30)+1;if(due<=c.frames)return;
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,c.fbo);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
  int x,y,w,h;fit_rect_px(c.w,c.h,(float)W/H,&x,&y,&w,&h);glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glBlitFramebuffer(0,0,W,H,x,y,x+w,y+h,GL_COLOR_BUFFER_BIT,GL_LINEAR);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,c.fbo);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,c.w,c.h,GL_RGBA,GL_UNSIGNED_BYTE,c.pixels);glBindFramebuffer(GL_FRAMEBUFFER,0);
  // A fixed timeline repeats frames when Original mode renders below 30 fps.
  while(c.frames<due){if(!sys_encoder_video(c.pixels,(size_t)c.w*c.h*4)){app_capture_stop();return;}c.frames++;}
  if(!MX.dev){static int16_t silence[1470];sys_encoder_audio(silence,735);}
}
