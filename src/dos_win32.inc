struct DOS_STARTUPINFO {DWORD cb;char *reserved,*desktop,*title;DWORD x,y,w,h,cx,cy,fill,flags;WORD show,reserved_bytes;BYTE *reserved_data;HANDLE input,output,error;};
struct DOS_PROCESSINFO {HANDLE process,thread;DWORD process_id,thread_id;};
IMP BOOL WINAPI CreateProcessA(const char *,char *,void *,void *,BOOL,DWORD,void *,const char *,DOS_STARTUPINFO *,DOS_PROCESSINFO *);
static auto dos_create_process=CreateProcessA;
int sys_game_is_gog(){return gf_status==GOG_COMPLETE&&gf_language>=0&&gf_language<3;}
int sys_run_dos(const char *directory,char *error,int cap){
  if(!sys_game_is_gog()){s_cpy(error,"DOS launch requires a recognized GOG edition.",cap);return 0;}
  char base[1024],exe[1100],cwd[1100],path[1100],command[1400];
  DWORD n=GetFullPathNameA(directory,sizeof base,base,0);
  if(!n||n>=sizeof base){s_cpy(error,"Game directory is too long or invalid.",cap);return 0;}
  s_cpy(cwd,base,sizeof cwd);s_cat(cwd,"\\DOSBOX",sizeof cwd);s_cpy(exe,cwd,sizeof exe);s_cat(exe,"\\DOSBox.exe",sizeof exe);
  const char *required[]={"\\dosboxmegarace2.conf","\\dosboxmegarace2_single.conf"};
  if(!gd_exists(exe)){s_cpy(error,"DOSBOX/DOSBox.exe was not found in the selected GOG installation.",cap);return 0;}
  for(int i=0;i<2;i++){s_cpy(path,base,sizeof path);s_cat(path,required[i],sizeof path);if(!gd_exists(path)){s_cpy(error,"The original GOG DOSBox configuration files were not found.",cap);return 0;}}
  s_cpy(command,"\"",sizeof command);s_cat(command,exe,sizeof command);s_cat(command,"\" -conf \"..\\dosboxmegarace2.conf\" -conf \"..\\dosboxmegarace2_single.conf\" -noconsole -c \"exit\"",sizeof command);
  DOS_STARTUPINFO si={};si.cb=sizeof si;DOS_PROCESSINFO pi={};
  if(!dos_create_process(exe,command,0,0,0,0,0,cwd,&si,&pi)){s_cpy(error,"DOSBox could not be started. OpenMR2 will remain open.",cap);return 0;}
  if(pi.thread)CloseHandle(pi.thread);if(pi.process)CloseHandle(pi.process);return 1;
}
