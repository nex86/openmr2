struct ENC_SECURITY {DWORD bytes;void *descriptor;BOOL inherit;};
IMP BOOL WINAPI CreatePipe(HANDLE *,HANDLE *,ENC_SECURITY *,DWORD);
IMP BOOL WINAPI SetHandleInformation(HANDLE,DWORD,DWORD);
IMP DWORD WINAPI WaitForSingleObject(HANDLE,DWORD);
IMP BOOL WINAPI GetExitCodeProcess(HANDLE,DWORD *);
static HANDLE enc_pipe,enc_pcm,enc_process;static int enc_failed;
static char enc_path[512],enc_video[550],enc_audio[550],enc_log[550],enc_exe[1100];
static void enc_int(char *out,int value){char b[24];int n=0;do{b[n++]=(char)('0'+value%10);value/=10;}while(value);char v[24];int k=0;while(n)v[k++]=b[--n];v[k]=0;s_cat(out,v,4096);}
static int enc_write(HANDLE file,const void *data,size_t bytes){const BYTE *p=(const BYTE *)data;while(bytes){DWORD n=0,part=bytes>1048576?1048576:(DWORD)bytes;if(!WriteFile(file,p,part,&n,0)||!n)return 0;p+=n;bytes-=n;}return 1;}
static HANDLE enc_spawn(const char *args,HANDLE input){
  char command[4096]="\"";s_cat(command,enc_exe,sizeof command);s_cat(command,"\" ",sizeof command);s_cat(command,args,sizeof command);
  ENC_SECURITY sa={sizeof sa,0,1};HANDLE log=CreateFileA(enc_log,GENERIC_WRITE,FILE_SHARE_READ,&sa,4,0,0);LARGE_INTEGER end;end.QuadPart=0;SetFilePointerEx(log,end,0,2);
  HANDLE nil=0;if(!input){nil=CreateFileA("NUL",GENERIC_READ,FILE_SHARE_READ,&sa,OPEN_EXISTING,0,0);input=nil;}
  DOS_STARTUPINFO si={};si.cb=sizeof si;si.flags=0x101;si.show=0;si.input=input;si.output=si.error=log;DOS_PROCESSINFO pi={};
  BOOL ok=CreateProcessA(0,command,0,0,1,0x08000000,0,0,&si,&pi);if(log&&log!=INVALID_HANDLE_VALUE)CloseHandle(log);if(nil&&nil!=INVALID_HANDLE_VALUE)CloseHandle(nil);if(!ok)return 0;CloseHandle(pi.thread);return pi.process;
}
static int enc_wait(HANDLE process){if(!process)return 0;WaitForSingleObject(process,0xffffffffu);DWORD code=1;GetExitCodeProcess(process,&code);CloseHandle(process);return code==0;}
int sys_encoder_open(const char *path,int w,int h){
  if(enc_pipe)return 0;s_cpy(enc_path,path,sizeof enc_path);s_cpy(enc_video,path,sizeof enc_video);s_cat(enc_video,".video.mp4",sizeof enc_video);s_cpy(enc_audio,path,sizeof enc_audio);s_cat(enc_audio,".audio.s16",sizeof enc_audio);s_cpy(enc_log,path,sizeof enc_log);s_cat(enc_log,".log",sizeof enc_log);
  GetModuleFileNameA(0,enc_exe,sizeof enc_exe);int n=(int)s_len(enc_exe);while(n&&enc_exe[n-1]!='\\'&&enc_exe[n-1]!='/')n--;enc_exe[n]=0;s_cat(enc_exe,"tools\\ffmpeg.exe",sizeof enc_exe);if(!gd_exists(enc_exe))s_cpy(enc_exe,"ffmpeg.exe",sizeof enc_exe);
  enc_pcm=CreateFileA(enc_audio,GENERIC_WRITE,FILE_SHARE_READ,0,1,0,0);if(enc_pcm==INVALID_HANDLE_VALUE){enc_pcm=0;return 0;}
  ENC_SECURITY sa={sizeof sa,0,1};HANDLE read=0;if(!CreatePipe(&read,&enc_pipe,&sa,1048576)){CloseHandle(enc_pcm);enc_pcm=0;return 0;}SetHandleInformation(enc_pipe,1,0);
  char args[4096]="-hide_banner -loglevel error -n -f rawvideo -pixel_format rgba -video_size ";enc_int(args,w);s_cat(args,"x",sizeof args);enc_int(args,h);s_cat(args," -framerate 30 -i pipe:0 -vf vflip -an -c:v libx264 -preset veryfast -crf 21 -maxrate 8M -bufsize 16M -pix_fmt yuv420p \"",sizeof args);s_cat(args,enc_video,sizeof args);s_cat(args,"\"",sizeof args);
  enc_process=enc_spawn(args,read);CloseHandle(read);if(!enc_process){CloseHandle(enc_pipe);CloseHandle(enc_pcm);enc_pipe=enc_pcm=0;DeleteFileA(enc_audio);return 0;}enc_failed=0;return 1;
}
int sys_encoder_video(const void *p,size_t n){if(!enc_pipe)return 0;if(!enc_write(enc_pipe,p,n))enc_failed=1;return !enc_failed;}
void sys_encoder_audio(const int16_t *p,int frames){if(enc_pcm&&!enc_write(enc_pcm,p,(size_t)frames*4))enc_failed=1;}
int sys_encoder_close(){
  if(!enc_pipe)return 0;CloseHandle(enc_pipe);CloseHandle(enc_pcm);enc_pipe=enc_pcm=0;int ok=enc_wait(enc_process)&&!enc_failed;enc_process=0;if(!ok)return 0;
  char args[4096]="-hide_banner -loglevel error -n -i \"";s_cat(args,enc_video,sizeof args);s_cat(args,"\" -f s16le -ar 22050 -ac 2 -i \"",sizeof args);s_cat(args,enc_audio,sizeof args);s_cat(args,"\" -map 0:v:0 -map 1:a:0 -c:v copy -c:a aac -b:a 128k -af apad -shortest -movflags +faststart \"",sizeof args);s_cat(args,enc_path,sizeof args);s_cat(args,"\"",sizeof args);
  ok=enc_wait(enc_spawn(args,0));if(ok){DeleteFileA(enc_video);DeleteFileA(enc_audio);}return ok;
}
