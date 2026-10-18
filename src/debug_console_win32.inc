// A game-owned console. Hiding preserves its history; no shell process is launched.
static HANDLE g_con;
static HWND g_console_window;
static int g_console_visible;
static char g_console_history[64][384];
static unsigned g_console_history_count;
static int str_has_ci(const char *s,const char *word) {
  for(;*s;s++){const char *a=s,*b=word;while(*a && *b && ((*a|32)==(*b|32))){a++;b++;}if(!*b)return 1;}return 0;
}
static void console_line(const char *msg) {
  int error=str_has_ci(msg,"error")||str_has_ci(msg,"failed")||str_has_ci(msg,"missing")||str_has_ci(msg,"invalid")||str_has_ci(msg,"unsupported")||str_has_ci(msg,"incomplete");
  SetConsoleTextAttribute(g_con,error?0x0C:str_has_ci(msg,"warning")?0x0E:0x07);
  DWORD written;WriteFile(g_con,msg,(DWORD)s_len(msg),&written,0);WriteFile(g_con,"\r\n",2,&written,0);
  SetConsoleTextAttribute(g_con,0x07);
}
int sys_debug_enabled(){return g_console_visible;}
void sys_log(const char *msg) {
  if(!g_console_visible)return;
  OutputDebugStringA(msg);OutputDebugStringA("\n");
  s_cpy(g_console_history[g_console_history_count++%64],msg,384);
  if(g_con)console_line(msg);
  if(g_log && g_log!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(g_log,msg,(DWORD)s_len(msg),&written,0);WriteFile(g_log,"\r\n",2,&written,0);}
}
int sys_debug_console(int enabled) {
  enabled=enabled?1:0;if(enabled==g_console_visible)return enabled;
  if(enabled && !g_console_window){
    typedef HWND(WINAPI *ConsoleWindowFn)(void);
    ConsoleWindowFn get_window=(ConsoleWindowFn)GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetConsoleWindow");
    if(!get_window || !AllocConsole()){sys_log("[error] Could not open the debug console.");return 0;}
    g_console_window=get_window();g_con=GetStdHandle((DWORD)-11);
    if(g_con==INVALID_HANDLE_VALUE)g_con=0;
    SetConsoleTitleA(OPENMR2_FULLTITLE " - Debug console");
    // Disable the console Close command: it would otherwise terminate the game.
    HMODULE user=GetModuleHandleA("user32.dll");
    typedef HMENU(WINAPI *SystemMenuFn)(HWND,BOOL);
    typedef UINT(WINAPI *EnableMenuFn)(HMENU,UINT,UINT);
    SystemMenuFn get_menu=(SystemMenuFn)GetProcAddress(user,"GetSystemMenu");
    EnableMenuFn enable_menu=(EnableMenuFn)GetProcAddress(user,"EnableMenuItem");
    if(get_menu && enable_menu)enable_menu(get_menu(g_console_window,0),0xF060,1);
    // Quick Edit can freeze the writing game thread while text is selected.
    typedef BOOL(WINAPI *GetModeFn)(HANDLE,DWORD *);
    typedef BOOL(WINAPI *SetModeFn)(HANDLE,DWORD);
    HMODULE kernel=GetModuleHandleA("kernel32.dll");
    GetModeFn get_mode=(GetModeFn)GetProcAddress(kernel,"GetConsoleMode");
    SetModeFn set_mode=(SetModeFn)GetProcAddress(kernel,"SetConsoleMode");
    DWORD mode=0;HANDLE input=GetStdHandle((DWORD)-10);
    if(get_mode && set_mode && get_mode(input,&mode))set_mode(input,(mode|0x80)&~0x40u);
    if(g_con){unsigned count=g_console_history_count<64?g_console_history_count:64;
      for(unsigned i=g_console_history_count-count;i<g_console_history_count;i++)console_line(g_console_history[i%64]);}
  }
  if(g_console_window)ShowWindow(g_console_window,enabled?4:0);
  g_console_visible=enabled;
  if(enabled && !g_log){make_parent_dir("revived_config/openmr2.log");g_log=CreateFileA("revived_config/openmr2.log",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);}
  if(!enabled && g_log){if(g_log!=INVALID_HANDLE_VALUE)CloseHandle(g_log);g_log=0;}
  sys_log(enabled?"[event] Debug console enabled. Use Debug / Cheats to hide it; errors are red.":"[event] Debug console hidden. Logging disabled.");
  return enabled;
}
