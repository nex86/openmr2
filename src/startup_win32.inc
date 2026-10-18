// Native modal startup chooser. Closing it cancels startup without saving a choice.
IMP BOOL WINAPI DestroyWindow(HWND);
IMP BOOL WINAPI EnableWindow(HWND,BOOL);
IMP HWND WINAPI SetFocus(HWND);
IMP LRESULT WINAPI SendMessageA(HWND,UINT,WPARAM,LPARAM);
IMP BOOL WINAPI IsDialogMessageA(HWND,MSG *);
static int mode_result,mode_remember;
static HWND mode_check;
static LRESULT WINAPI mode_proc(HWND window,UINT msg,WPARAM wp,LPARAM lp){
  if(msg==0x0111){int id=(int)(wp&65535);if(id==101||id==102){mode_remember=(int)SendMessageA(mode_check,0x00F0,0,0)==1;mode_result=id-101;return 0;}}
  if(msg==WM_CLOSE){mode_result=-1;return 0;}
  return DefWindowProcA(window,msg,wp,lp);
}
int sys_choose_mode(int current,const char *title,const char *question,const char *revived,const char *retro,const char *remember,int *save){
  WNDCLASSEXA wc={};wc.cbSize=sizeof wc;wc.lpfnWndProc=mode_proc;wc.hInstance=GetModuleHandleA(0);wc.hCursor=LoadCursorA(0,(const char *)32512);wc.hbrBackground=(HBRUSH)16;wc.lpszClassName="OpenMR2ModeChoice";RegisterClassExA(&wc);
  RECT parent={};if(g_hwnd)GetWindowRect(g_hwnd,&parent);else{parent.right=GetSystemMetrics(0);parent.bottom=GetSystemMetrics(1);}int width=480,height=205;
  HWND win=CreateWindowExA(1,wc.lpszClassName,title,0x80C80000u,(parent.left+parent.right-width)/2,(parent.top+parent.bottom-height)/2,width,height,g_hwnd,0,wc.hInstance,0);
  if(!win){*save=0;return -1;}
  HFONT font=CreateFontA(-16,0,0,0,400,0,0,0,1,0,0,0,0,"Segoe UI");
  auto child=[&](const char *kind,const char *text,DWORD style,int x,int y,int w,int h,int id){HWND c=CreateWindowExA(0,kind,text,0x50000000u|((kind[0]=='B')?0x10000u:0)|style,x,y,w,h,win,(HMENU)(intptr_t)id,wc.hInstance,0);SendMessageA(c,0x0030,(WPARAM)font,1);return c;};
  child("STATIC",question,0,22,18,430,35,0);
  HWND a=child("BUTTON",revived,current==0?1:0,22,62,207,38,101);
  HWND b=child("BUTTON",retro,current==1?1:0,243,62,207,38,102);
  mode_check=child("BUTTON",remember,3,22,119,430,28,103);SendMessageA(mode_check,0x00F1,*save?1:0,0);
  mode_result=-2;mode_remember=0;if(g_hwnd)EnableWindow(g_hwnd,0);ShowWindow(win,5);SetFocus(current?b:a);
  while(mode_result==-2){MSG message;while(PeekMessageA(&message,0,0,0,PM_REMOVE)){
    if(message.message==WM_QUIT){mode_result=-1;PostQuitMessage((int)message.wParam);break;}
    if(message.message==WM_KEYDOWN&&message.wParam==27){mode_result=-1;break;}
    if(!IsDialogMessageA(win,&message)){TranslateMessage(&message);DispatchMessageA(&message);}
  }if(mode_result==-2)Sleep(10);}
  *save=mode_remember;if(g_hwnd)EnableWindow(g_hwnd,1);DestroyWindow(win);if(g_hwnd)SetFocus(g_hwnd);if(font)DeleteObject(font);return mode_result;
}
