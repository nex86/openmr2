#include <signal.h>
static int enc_pipe=-1,enc_pcm=-1,enc_failed;static pid_t enc_pid;
static char enc_path[512],enc_video[550],enc_audio[550],enc_log[550];
static int enc_write(int fd,const void *data,size_t bytes){
  const uint8_t *p=(const uint8_t *)data;while(bytes){ssize_t n=write(fd,p,bytes);if(n<0&&errno==EINTR)continue;if(n<=0)return 0;p+=n;bytes-=n;}return 1;
}
static int enc_wait(pid_t pid){int status=0;while(waitpid(pid,&status,0)<0)if(errno!=EINTR)return 0;return WIFEXITED(status)&&WEXITSTATUS(status)==0;}
static pid_t enc_spawn(char *const args[],int input){
  int hand[2];if(pipe(hand))return -1;fcntl(hand[1],F_SETFD,FD_CLOEXEC);
  pid_t pid=fork();if(!pid){close(hand[0]);int log=open(enc_log,O_WRONLY|O_CREAT|O_APPEND,0600);if(log>=0){dup2(log,2);dup2(log,1);close(log);}if(input>=0)dup2(input,0);else{int nil=open("/dev/null",O_RDONLY);dup2(nil,0);close(nil);}for(int fd=3;fd<1024;fd++)if(fd!=hand[1])close(fd);execvp(args[0],args);int err=errno;int sent=enc_write(hand[1],&err,sizeof err);_exit(sent?127:126);}
  close(hand[1]);int error=0;ssize_t n=read(hand[0],&error,sizeof error);close(hand[0]);if(pid<0||n>0){if(pid>0)enc_wait(pid);return -1;}return pid;
}
int sys_encoder_open(const char *path,int w,int h){
  if(enc_pipe>=0)return 0;signal(SIGPIPE,SIG_IGN);snprintf(enc_path,sizeof enc_path,"%s",path);snprintf(enc_video,sizeof enc_video,"%s.video.mp4",path);snprintf(enc_audio,sizeof enc_audio,"%s.audio.s16",path);snprintf(enc_log,sizeof enc_log,"%s.log",path);
  enc_pcm=open(enc_audio,O_WRONLY|O_CREAT|O_EXCL,0600);if(enc_pcm<0)return 0;fcntl(enc_pcm,F_SETFD,FD_CLOEXEC);
  int pipes[2];if(pipe(pipes)){close(enc_pcm);enc_pcm=-1;return 0;}char size[40];snprintf(size,sizeof size,"%dx%d",w,h);
  const char *exe=getenv("OPENMR2_FFMPEG");if(!exe||!*exe)exe="ffmpeg";
  const char *args[]={exe,"-hide_banner","-loglevel","error","-n","-f","rawvideo","-pixel_format","rgba","-video_size",size,"-framerate","30","-i","pipe:0","-vf","vflip","-an","-c:v","libx264","-preset","veryfast","-crf","21","-maxrate","8M","-bufsize","16M","-pix_fmt","yuv420p",enc_video,0};
  enc_pid=enc_spawn((char *const *)args,pipes[0]);close(pipes[0]);if(enc_pid<0){close(pipes[1]);close(enc_pcm);enc_pcm=-1;unlink(enc_audio);return 0;}enc_pipe=pipes[1];fcntl(enc_pipe,F_SETFD,FD_CLOEXEC);enc_failed=0;return 1;
}
int sys_encoder_video(const void *p,size_t n){if(enc_pipe<0)return 0;if(!enc_write(enc_pipe,p,n))enc_failed=1;return !enc_failed;}
void sys_encoder_audio(const int16_t *p,int frames){if(enc_pcm>=0&&!enc_write(enc_pcm,p,(size_t)frames*4))enc_failed=1;}
int sys_encoder_close(){
  if(enc_pipe<0)return 0;close(enc_pipe);enc_pipe=-1;close(enc_pcm);enc_pcm=-1;int ok=enc_wait(enc_pid)&&!enc_failed;enc_pid=0;if(!ok)return 0;
  const char *exe=getenv("OPENMR2_FFMPEG");if(!exe||!*exe)exe="ffmpeg";
  const char *args[]={exe,"-hide_banner","-loglevel","error","-n","-i",enc_video,"-f","s16le","-ar","22050","-ac","2","-i",enc_audio,"-map","0:v:0","-map","1:a:0","-c:v","copy","-c:a","aac","-b:a","128k","-af","apad","-shortest","-movflags","+faststart",enc_path,0};
  pid_t child=enc_spawn((char *const *)args,-1);ok=child>0&&enc_wait(child);if(ok){unlink(enc_video);unlink(enc_audio);}return ok;
}
