// Real Win32 console plus the production menu callback; no original assets required.
#define WinMainCRTStartup original_gui_entry
#include "../src/win32_main.cpp"
#undef WinMainCRTStartup
#include "../src/core.cpp"
#define STUB(ret,name,args) static ret test_##name args { return (ret)0; }
GL_FUNCS(STUB)
#undef STUB
static int reset_calls;static UINT WINAPI test_reset(HANDLE){reset_calls++;return 0;}
static int failures,assertions;static char report[4000];
static void check(int condition,const char *name){assertions++;if(!condition){failures++;s_cat(report,"FAIL: ",sizeof report);s_cat(report,name,sizeof report);s_cat(report,"\r\n",sizeof report);}}
extern "C" void WinMainCRTStartup(){
  g_heap=GetProcessHeap();
  unsigned silent_count=g_console_history_count;
  sys_log("disabled logging must be discarded");sys_log_asset("disabled.HLZ");
  check(g_console_history_count==silent_count && !g_log,"disabled console neither records nor opens a log");
  verify_begin();verify_update("M.EXE",50,100);
  check(verify_window && verify_bar && verify_send && verify_send(verify_bar,0x408,0,0)==50,"real verification dialog displays byte progress");
  verify_end();
#define USE(ret,name,args) name=test_##name;
  GL_FUNCS(USE)
#undef USE
  g_cv=(uint32_t *)sys_alloc(CW*CH*4);G.state=GS_DEBUG;G.sel=9;G.kbd=1;gf_language=2;
  Input press={};press.pressed[K_ENTER]=1;
  screen_debug(&press);
  check(G.dbg_console && A.debug_console && g_console_window && g_console_visible,"production checkbox opens a real console");
  HWND first=g_console_window;
  typedef BOOL(WINAPI *VisibleFn)(HWND);
  VisibleFn visible=(VisibleFn)GetProcAddress(GetModuleHandleA("user32.dll"),"IsWindowVisible");
  check(visible && visible(first),"console window is visible");
  check(gd_cfg_flag("debug_console"),"enabled setting is written to the real config");
  setup_load();check(G.dbg_console && A.debug_console,"console setting survives settings reload");
  G.sel=9;screen_debug(&press);
  check(!G.dbg_console && !A.debug_console && !visible(first),"same checkbox hides the window without ending the process");
  check(!gd_cfg_flag("debug_console"),"disabled setting persists");
  silent_count=g_console_history_count;sys_log("must not be retained while hidden");
  check(g_console_history_count==silent_count && !g_log,"hiding immediately disables history and disk logging");
  G.sel=9;screen_debug(&press);
  check(g_console_window==first && visible(first),"re-enabling reuses the same console and its history");
  typedef BOOL(WINAPI *ModeFn)(HANDLE,DWORD *);
  ModeFn get_mode=(ModeFn)GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetConsoleMode");DWORD mode=0;
  check(get_mode && get_mode(GetStdHandle((DWORD)-10),&mode) && !(mode&0x40),"Quick Edit cannot pause the game thread");
  typedef HMENU(WINAPI *MenuFn)(HWND,BOOL);typedef UINT(WINAPI *MenuStateFn)(HMENU,UINT,UINT);
  MenuFn menu=(MenuFn)GetProcAddress(GetModuleHandleA("user32.dll"),"GetSystemMenu");
  MenuStateFn menu_state=(MenuStateFn)GetProcAddress(GetModuleHandleA("user32.dll"),"GetMenuState");
  check(menu && menu_state && (menu_state(menu(first,0),0xF060,0)&1),"console Close command cannot terminate the game");
  gf_language=1;go(GS_MESSAGE);
  check(!str_icmp(ui_debug_text("[Zustand] "),"[State] ") && !str_icmp(game_state_name(GS_MESSAGE),"Message"),"diagnostics stay English under French game data");
  sys_write_file("console-write-blocker","x",1);
  check(!sys_write_file("console-write-blocker/file.dat","x",1),"real write failure is detected and logged");sys_delete_file("console-write-blocker");
  sys_log("[error] Console regression marker");
  unsigned asset_count=g_console_history_count;
  sys_log_asset("TRACK1/FACTO.UBB");sys_log_asset("CARS/FAC/CAISSE10.M3D");sys_log_asset("MENU.HLZ");sys_log_asset("SOUNDS/TEST.WAV");sys_log_asset("MUSIC/TEST.16");
  check(g_console_history_count==asset_count+3,"graphics, models and track videos are logged; sound files are excluded");
  mem_zero(&PF.lo,sizeof PF.lo);PF.money=50000;PF.car=0;G.cheat_career.money=0;PF.single=0;
  shop_buy(W_SHIELD);check(PF.lo.shield==100,"real shop buys a shield upgrade");
  shop_buy(W_SHIELD);check(!PF.lo.shield,"real shop sells a shield upgrade");
  shop_buy(-W_KARD);PF.lo.fammo[0]=0;shop_buy(W_KARD);shop_buy(-W_KARD);
  typedef BOOL(WINAPI *ReadFn)(HANDLE,char *,DWORD,DWORD,DWORD *);
  ReadFn read=(ReadFn)GetProcAddress(GetModuleHandleA("kernel32.dll"),"ReadConsoleOutputCharacterA");
  char text[8193]={};DWORD got=0;int read_ok=read && read(g_con,text,8192,0,&got);text[got]=0;
  check(read_ok && str_has_ci(text,"Console regression marker") && str_has_ci(text,"Could not write file"),"real console contains useful English errors");
  check(read_ok && str_has_ci(text,"Debug / Cheats -> Message"),"real console contains state-change events");
  check(read_ok && str_has_ci(text,"Upgrade purchased: Shield") && str_has_ci(text,"Upgrade sold: Shield") && str_has_ci(text,"Ammunition purchased") && str_has_ci(text,"Weapon launcher sold"),"real inventory changes produce English transaction events");
  u8 texture_pixels[]={10,20,30,0,40,50,60,128,70,80,90,255,1,2,3,255};
  png_write("revived_config/texture_replacements/LOCAL/CONSOLE/ALPHA.HLZ.png",texture_pixels,2,2);
  g_custom_textures=1;check(texture_custom("LOCAL/CONSOLE/ALPHA.HLZ.png")!=0,"console texture test loads a real PNG");
  texture_dump_rgba("LOCAL/CONSOLE/ALPHA.HLZ.png",texture_pixels,2,2,"RGBA8 straight alpha");
  texture_cache_clear();g_custom_textures=0;
  text[0]=0;got=0;read_ok=read && read(g_con,text,8192,0,&got);text[got]=0;
  check(read_ok && str_has_ci(text,"Loaded replacement") && str_has_ci(text,"texture_replacements") && str_has_ci(text,"straight alpha") && str_has_ci(text,"texture_dumps") && str_has_ci(text,"cache cleared"),"real console includes English dump/replacement paths, alpha and reload diagnostics");
  sys_delete_file("revived_config/texture_replacements/LOCAL/CONSOLE/ALPHA.HLZ.png");sys_delete_file("revived_config/texture_dumps/LOCAL/CONSOLE/ALPHA.HLZ.png");sys_delete_file("revived_config/texture_dumps/LOCAL/CONSOLE/ALPHA.HLZ.png.txt");
  g_wave=(HANDLE)1;p_waveOutReset=test_reset;g_aud_mem=(int16_t *)sys_alloc(AUD_BUFS*AUD_FRAMES*4);memset(g_aud_mem,255,AUD_BUFS*AUD_FRAMES*4);
  for(int i=0;i<AUD_BUFS;i++)g_hdr[i].dwFlags=0;
  G.state=GS_CLIP;go(GS_GAMEOVER);int clean=reset_calls==1;
  for(int i=0;i<AUD_BUFS*AUD_FRAMES*2;i++)if(g_aud_mem[i])clean=0;
  for(int i=0;i<AUD_BUFS;i++)if(!(g_hdr[i].dwFlags&1))clean=0;
  check(clean,"real Game Over transition flushes and clears all queued waveOut buffers");
  sys_free(g_aud_mem);g_aud_mem=0;g_wave=0;p_waveOutReset=0;
  sys_debug_console(0);
  long long previous_frames=MX.filled;int previous_peak=MX.peak,previous_clipped=MX.clipped;int16_t silent[4];app_audio_fill(silent,2);
  check(MX.filled==previous_frames && MX.peak==previous_peak && MX.clipped==previous_clipped,"disabled console does not accumulate audio diagnostics");
  s_cat(report,"Assertions: ",sizeof report);str_int(report,sizeof report,assertions);s_cat(report,"\r\nFailures: ",sizeof report);str_int(report,sizeof report,failures);s_cat(report,"\r\n",sizeof report);
  sys_write_file("console-results.txt",report,s_len(report));ExitProcess(failures?1:0);
}
