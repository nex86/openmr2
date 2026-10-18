static int chooser_calls,chooser_answer,chooser_remember,chooser_default;
static int fake_chooser(int current,const char *,const char *question,const char *a,const char *b,const char *remember,int *save){
  chooser_calls++;chooser_default=current;
  check(!str_icmp(a,"Revived Mode")&&!str_icmp(b,"Retro Mode")&&!str_icmp(remember,"Remember my choice")&&!str_icmp(question,"Choose how you want to start OpenMR2:"),"startup dialog text is always English");
  *save=chooser_remember;return chooser_answer;
}
#ifdef _WIN32
static int dos_spawn_calls,dos_spawn_ok;
static BOOL WINAPI fake_dos_spawn(const char *exe,char *cmd,void *,void *,BOOL inherit,DWORD flags,void *,const char *cwd,DOS_STARTUPINFO *si,DOS_PROCESSINFO *){
  dos_spawn_calls++;
  check(str_icontains(exe,"DOSBOX\\DOSBox.exe")&&str_icontains(cwd,"DOSBOX"),"DOSBox uses the selected installation and the original shortcut working directory");
  check(str_icontains(cmd," -conf \"..\\dosboxmegarace2.conf\" -conf \"..\\dosboxmegarace2_single.conf\" -noconsole -c \"exit\"")&&!inherit&&!flags&&si->cb==sizeof *si,"DOS launcher preserves the exact original arguments without a shell or inherited handles");
  return dos_spawn_ok;
}
#endif
static void test_startup_dos(){
  const char *path="revived_config/openmr2_setup.cfg";size_t n=0;u8 *old=sys_read_file(path,&n);auto real_chooser=startup_choose;startup_choose=fake_chooser;
  const char *initial="# retain comment\ngraphics_mode = 0\nFiles_OK = true\nverified_language = 2\nengines_volume = 17\n";sys_write_file(path,initial,str_len(initial));
  char name[]="OpenMR2";char *args[]={name};chooser_calls=0;chooser_answer=1;chooser_remember=1;
  check(startup_before_window(1,args)&&startup_choice==1&&chooser_calls==1,"first launch asks once and uses the selected Retro mode");
  size_t size=0;u8 *saved=sys_read_file(path,&size);check(saved&&str_icontains((char *)saved,"Files_OK = true")&&str_icontains((char *)saved,"engines_volume = 17")&&str_icontains((char *)saved,"# retain comment"),"remembering startup preserves file validation receipts, volumes and comments");sys_free(saved);
  chooser_answer=0;check(startup_before_window(1,args)&&startup_choice==1&&chooser_calls==1,"remembered mode skips the native chooser on later launches");
  setup_load();setup_save();check(startup_before_window(1,args)&&startup_choice==1&&chooser_calls==1,"normal settings saves retain the remembered launch selection");
  const char *again="startup_mode = 1\nremember_startup_mode = 0\n";sys_write_file(path,again,str_len(again));chooser_remember=0;chooser_answer=0;
  check(startup_before_window(1,args)&&startup_choice==0&&chooser_default==1,"unchecked launch choice still takes effect and offers last selection next time");
  int calls=chooser_calls;check(startup_before_window(1,args)&&chooser_calls==calls+1,"unchecked remember box asks again next launch");
  chooser_answer=-1;calls=chooser_calls;size_t before_n=0;u8 *before=sys_read_file(path,&before_n);
  check(!startup_before_window(1,args)&&startup_choice<0&&chooser_calls==calls+1,"closing the startup chooser cancels startup");
  saved=sys_read_file(path,&size);check(saved&&size==before_n&&!memcmp(before,saved,size),"cancelling leaves configuration unchanged");sys_free(before);sys_free(saved);
  char shot[]="--shot";char *automation[]={name,shot};calls=chooser_calls;check(startup_before_window(2,automation)&&startup_choice<0&&chooser_calls==calls,"automated screenshot entry points do not open a modal dialog");
  startup_choose=real_chooser;startup_choice=-1;
  for(int status=0;status<3;status++)for(int language=-1;language<4;language++){gf_status=status;gf_language=language;check(sys_game_is_gog()==(status==GOG_COMPLETE&&language>=0&&language<3),"DOS entry is enabled only for a recognized complete GOG edition");}
  char error[600];gf_status=GOG_UNKNOWN;check(!sys_run_dos("../../../Megarace 2 English",error,sizeof error),"unknown versions cannot launch DOSBox through the guarded API");
#ifdef _WIN32
  auto real_spawn=dos_create_process;dos_create_process=fake_dos_spawn;dos_spawn_calls=0;gf_status=GOG_COMPLETE;gf_language=0;dos_spawn_ok=0;
  check(!sys_run_dos("../../../Megarace 2 English",error,sizeof error)&&dos_spawn_calls==1,"failed DOSBox process creation is reported instead of quitting");
  dos_spawn_ok=1;check(sys_run_dos("../../../Megarace 2 English",error,sizeof error)&&dos_spawn_calls==2,"successful DOSBox process creation reports success");
  check(!sys_run_dos(".",error,sizeof error)&&dos_spawn_calls==2,"missing GOG DOSBox files prevent process creation");dos_create_process=real_spawn;
#endif
  if(old){sys_write_file(path,old,n);sys_free(old);}else sys_delete_file(path);setup_load();gf_status=GOG_COMPLETE;gf_language=2;
  for(int language=0;language<3;language++){gf_language=language;check(cv_text_w(g_font_h14,"Run DOS Version",1)<290,"original-font DOS menu label fits every translation");}
  gf_language=2;
  for(int i=0;i<6;i++){G.gfx_output_filter=i;setup_save();G.gfx_output_filter=-1;setup_load();check(G.gfx_output_filter==i,"every symbolic output filter survives a config round trip");}
  G.gfx_output_filter=3;graphics_mode_set(1);graphics_mode_set(0);graphics_mode_set(1);check(G.gfx_output_filter==3,"Retro output filter survives switches through Revived mode");graphics_mode_set(0);G.gfx_output_filter=0;startup_choice=-1;
}
