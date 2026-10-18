// Native, English-only, responsive one-time verification dialog.
static HWND verify_window,verify_filename,verify_bar;
static LRESULT(WINAPI *verify_send)(HWND,UINT,WPARAM,LPARAM);
static LRESULT WINAPI verify_proc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_CLOSE)return 0;return DefWindowProcA(w,m,a,b);}
static void verify_update(const char *path,uint64_t done,uint64_t total){
  const char *name=path;for(const char *p=path;*p;p++)if(*p=='/'||*p=='\\')name=p+1;
  SetWindowTextA(verify_filename,name);if(verify_send)verify_send(verify_bar,0x402,total?(WPARAM)(done*100/total):0,0);
  MSG msg;while(PeekMessageA(&msg,0,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageA(&msg);}
}
static void verify_begin(){
  HINSTANCE instance=GetModuleHandleA(0);HMODULE controls=LoadLibraryA("comctl32.dll");
  typedef void(WINAPI *InitFn)();InitFn init=controls?(InitFn)GetProcAddress(controls,"InitCommonControls"):0;if(init)init();
  verify_send=(LRESULT(WINAPI *)(HWND,UINT,WPARAM,LPARAM))GetProcAddress(GetModuleHandleA("user32.dll"),"SendMessageA");
  WNDCLASSEXA wc={};wc.cbSize=sizeof wc;wc.hInstance=instance;wc.lpfnWndProc=verify_proc;wc.lpszClassName="OpenMR2Verification";wc.hbrBackground=(HBRUSH)6;RegisterClassExA(&wc);
  verify_window=CreateWindowExA(0,wc.lpszClassName,"OpenMR2 - one-time game data verification",0x00C00000|WS_VISIBLE,160,180,760,230,0,0,instance,0);
  CreateWindowExA(0,"STATIC","Checking the original MegaRace 2 files using SHA-256. Please wait.\nThis runs once for this game folder. Future launches only check that files exist.",0x40000000|WS_VISIBLE,18,18,710,48,verify_window,0,instance,0);
  CreateWindowExA(0,"STATIC","Verifying file (progress below applies to this file):",0x40000000|WS_VISIBLE,18,76,710,22,verify_window,0,instance,0);
  verify_filename=CreateWindowExA(0,"STATIC","",0x40000000|WS_VISIBLE,18,103,710,37,verify_window,0,instance,0);
  verify_bar=CreateWindowExA(0,"msctls_progress32","",0x40000000|WS_VISIBLE,18,151,710,24,verify_window,0,instance,0);
  sha256_progress=verify_update;verify_update("Preparing file verification...",0,1);
}
static void verify_end(){
  sha256_progress=0;typedef BOOL(WINAPI *DestroyFn)(HWND);DestroyFn destroy=(DestroyFn)GetProcAddress(GetModuleHandleA("user32.dll"),"DestroyWindow");if(destroy)destroy(verify_window);verify_window=0;
}
