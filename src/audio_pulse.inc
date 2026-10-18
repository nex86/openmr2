// PulseAudio/PipeWire desktop audio. Loaded dynamically; pumped on the game thread,
// like Win32 waveOut, so mixer voices are never accessed concurrently.
struct PulseSpec { int format; uint32_t rate; uint8_t channels; };
struct PulseBuffer { uint32_t maxlength,tlength,prebuf,minreq,fragsize; };
static void *pulse_stream;
static int (*pulse_flush)(void *,int *);
void sys_audio_flush(){if(pulse_stream && pulse_flush){int error=0;pulse_flush(pulse_stream,&error);}}
static int (*pulse_write)(void *,const void *,size_t,int *);
static uint64_t pulse_submitted;
static double pulse_start;
int sys_audio_open(int rate) {
  void *lib=dlopen("libpulse-simple.so.0",RTLD_NOW);if(!lib){sys_log("Audio unavailable: libpulse-simple.so.0 not found");return 0;}
  typedef void *(*New)(const char *,const char *,int,const char *,const char *,const PulseSpec *,const void *,const PulseBuffer *,int *);
  New create=(New)dlsym(lib,"pa_simple_new");pulse_write=(int (*)(void *,const void *,size_t,int *))dlsym(lib,"pa_simple_write");
  pulse_flush=(int (*)(void *,int *))dlsym(lib,"pa_simple_flush");
  if(!create||!pulse_write)return 0;
  PulseSpec spec={3,(uint32_t)rate,2}; // PA_SAMPLE_S16LE
  PulseBuffer attr={0xffffffffu,(uint32_t)(rate*4/20),0xffffffffu,(uint32_t)(rate*4/100),0xffffffffu};
  int error=0;pulse_stream=create(0,"OpenMR2",1,0,"Game audio",&spec,0,&attr,&error);
  if(!pulse_stream){sys_log("Audio unavailable: no PulseAudio/PipeWire playback server");return 0;}
  pulse_submitted=0;pulse_start=sys_time();sys_log("Audio output opened: PulseAudio/PipeWire, 22050 Hz stereo.");return 1;
}
static void audio_pump() {
  if(!pulse_stream)return;
  int64_t due=(int64_t)((sys_time()-pulse_start)*22050.0)-(int64_t)pulse_submitted;
  if(due>4410){pulse_start=sys_time()-(double)pulse_submitted/22050.0;due=512;}
  static int16_t buffer[1024];
  while(due>=256){int n=due>512?512:(int)due;app_audio_fill(buffer,n);int error=0;if(pulse_write(pulse_stream,buffer,(size_t)n*4,&error)<0){pulse_stream=0;sys_log("Audio playback failed");return;}pulse_submitted+=n;due-=n;}
}
