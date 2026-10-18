// Windows platform layer: Win32 window + WGL OpenGL 3.3 core context.
// Built freestanding (no C runtime): only kernel32/user32/gdi32 are imported, opengl32 is loaded at runtime.
#include "sys.h"
#include "gl.h"

// ------------------------------------------------------------------ minimal Win32 declarations
#define WINAPI __stdcall
#define IMP extern "C" __declspec(dllimport)
typedef void *HANDLE; typedef HANDLE HWND, HDC, HGLRC, HINSTANCE, HMODULE, HICON, HCURSOR, HBRUSH, HMENU, HBITMAP, HGDIOBJ, HFONT, HMONITOR;
typedef unsigned int UINT; typedef unsigned long DWORD; typedef int BOOL; typedef unsigned short WORD; typedef unsigned char BYTE;
typedef uintptr_t WPARAM; typedef intptr_t LPARAM; typedef intptr_t LRESULT; typedef long LONG; typedef intptr_t LONG_PTR;
typedef LRESULT(WINAPI *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
struct WNDCLASSEXA { UINT cbSize, style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; const char *lpszMenuName, *lpszClassName; HICON hIconSm; };
struct POINT { LONG x, y; };
struct MSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; DWORD lPrivate; };
struct RECT { LONG left, top, right, bottom; };
struct PIXELFORMATDESCRIPTOR { WORD nSize, nVersion; DWORD dwFlags; BYTE iPixelType, cColorBits, cRedBits, cRedShift, cGreenBits, cGreenShift, cBlueBits, cBlueShift, cAlphaBits, cAlphaShift, cAccumBits, cAccumRedBits, cAccumGreenBits, cAccumBlueBits, cAccumAlphaBits, cDepthBits, cStencilBits, cAuxBuffers, iLayerType, bReserved; DWORD dwLayerMask, dwVisibleMask, dwDamageMask; };
struct FILETIME { DWORD lo, hi; };
struct WIN32_FIND_DATAA { DWORD dwFileAttributes; FILETIME c, a, w; DWORD nFileSizeHigh, nFileSizeLow, r0, r1; char cFileName[260]; char cAlternateFileName[14]; DWORD dwFileType, dwCreatorType; WORD wFinderFlags; };
struct BITMAPINFOHEADER { DWORD biSize; LONG biWidth, biHeight; WORD biPlanes, biBitCount; DWORD biCompression, biSizeImage; LONG biXPelsPerMeter, biYPelsPerMeter; DWORD biClrUsed, biClrImportant; };
struct BITMAPINFO { BITMAPINFOHEADER bmiHeader; DWORD bmiColors[1]; };
struct MONITORINFO { DWORD cbSize; RECT rcMonitor, rcWork; DWORD dwFlags; };
union LARGE_INTEGER { struct { DWORD lo; LONG hi; } s; long long QuadPart; };

IMP HMODULE WINAPI GetModuleHandleA(const char *);
IMP DWORD WINAPI GetModuleFileNameA(HMODULE, char *, DWORD);
IMP char *WINAPI GetCommandLineA(void);
IMP void WINAPI ExitProcess(UINT);
IMP HANDLE WINAPI CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
IMP BOOL WINAPI ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
IMP BOOL WINAPI WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
IMP BOOL WINAPI CloseHandle(HANDLE);
IMP BOOL WINAPI GetFileSizeEx(HANDLE, LARGE_INTEGER *);
IMP HANDLE WINAPI FindFirstFileA(const char *, WIN32_FIND_DATAA *);
IMP BOOL WINAPI FindNextFileA(HANDLE, WIN32_FIND_DATAA *);
IMP BOOL WINAPI FindClose(HANDLE);
IMP DWORD WINAPI GetFileAttributesA(const char *);
IMP HANDLE WINAPI GetProcessHeap(void);
IMP void *WINAPI HeapAlloc(HANDLE, DWORD, size_t);
IMP void *WINAPI HeapReAlloc(HANDLE, DWORD, void *, size_t);
IMP BOOL WINAPI HeapFree(HANDLE, DWORD, void *);
IMP BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *);
IMP BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *);
IMP HMODULE WINAPI LoadLibraryA(const char *);
IMP void *WINAPI GetProcAddress(HMODULE, const char *);
IMP void WINAPI OutputDebugStringA(const char *);
IMP void WINAPI Sleep(DWORD);
IMP BOOL WINAPI CreateDirectoryA(const char *, void *);
IMP BOOL WINAPI DeleteFileA(const char *);
IMP BOOL WINAPI AllocConsole(void);
IMP HANDLE WINAPI GetStdHandle(DWORD);
IMP BOOL WINAPI SetConsoleTitleA(const char *);
IMP BOOL WINAPI SetConsoleTextAttribute(HANDLE, WORD);
IMP BOOL WINAPI SetCurrentDirectoryA(const char *);
IMP DWORD WINAPI GetFullPathNameA(const char *,DWORD,char *,char **);
IMP BOOL WINAPI SetFilePointerEx(HANDLE, LARGE_INTEGER, LARGE_INTEGER *, DWORD);

IMP WORD WINAPI RegisterClassExA(const WNDCLASSEXA *);
IMP HWND WINAPI CreateWindowExA(DWORD, const char *, const char *, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void *);
IMP LRESULT WINAPI DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
IMP BOOL WINAPI PeekMessageA(MSG *, HWND, UINT, UINT, UINT);
IMP BOOL WINAPI TranslateMessage(const MSG *);
IMP LRESULT WINAPI DispatchMessageA(const MSG *);
IMP BOOL WINAPI ShowWindow(HWND, int);
IMP HCURSOR WINAPI LoadCursorA(HINSTANCE, const char *);
IMP HICON WINAPI LoadIconA(HINSTANCE, const char *);
IMP HDC WINAPI GetDC(HWND);
IMP BOOL WINAPI SetWindowTextA(HWND, const char *);
IMP void WINAPI PostQuitMessage(int);
IMP BOOL WINAPI GetClientRect(HWND, RECT *);
IMP int WINAPI MessageBoxA(HWND, const char *, const char *, UINT);
IMP HWND WINAPI SetCapture(HWND);
IMP BOOL WINAPI ReleaseCapture(void);
IMP BOOL WINAPI AdjustWindowRect(RECT *, DWORD, BOOL);
IMP BOOL WINAPI SetProcessDPIAware(void);
IMP int WINAPI GetSystemMetrics(int);
IMP LONG_PTR WINAPI GetWindowLongPtrA(HWND, int);
IMP LONG_PTR WINAPI SetWindowLongPtrA(HWND, int, LONG_PTR);
IMP BOOL WINAPI SetWindowPos(HWND, HWND, int, int, int, int, UINT);
IMP BOOL WINAPI GetWindowRect(HWND, RECT *);
IMP HMONITOR WINAPI MonitorFromWindow(HWND, DWORD);
IMP BOOL WINAPI GetMonitorInfoA(HMONITOR, MONITORINFO *);
IMP int WINAPI DrawTextA(HDC, const char *, int, RECT *, UINT);

IMP int WINAPI ChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR *);
IMP BOOL WINAPI SetPixelFormat(HDC, int, const PIXELFORMATDESCRIPTOR *);
IMP BOOL WINAPI SwapBuffers(HDC);
IMP HDC WINAPI CreateCompatibleDC(HDC);
IMP HBITMAP WINAPI CreateDIBSection(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
IMP HGDIOBJ WINAPI SelectObject(HDC, HGDIOBJ);
IMP BOOL WINAPI DeleteObject(HGDIOBJ);
IMP BOOL WINAPI DeleteDC(HDC);
IMP HFONT WINAPI CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const char *);
IMP DWORD WINAPI SetTextColor(HDC, DWORD);
IMP int WINAPI SetBkMode(HDC, int);

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_CLOSE 0x0010
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_MOUSEWHEEL 0x020A
#define WM_QUIT 0x0012
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_VISIBLE 0x10000000
#define WS_POPUP 0x80000000u
#define GWL_STYLE (-16)
#define PM_REMOVE 1
#define CS_OWNDC 0x0020
#define CS_HREDRAW 2
#define CS_VREDRAW 1
#define PFD_DRAW_TO_WINDOW 4
#define PFD_SUPPORT_OPENGL 0x20
#define PFD_DOUBLEBUFFER 1
#define MB_ICONERROR 0x10
#define MB_ICONINFORMATION 0x40
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu
#define HEAP_ZERO_MEMORY 8
#define DT_LEFT 0
#define DT_NOPREFIX 0x800
#define DT_CALCRECT 0x400
#define TRANSPARENT 1
#define SWP_NOZORDER 4
#define SWP_FRAMECHANGED 0x20
#define SWP_NOOWNERZORDER 0x200
#define MONITOR_DEFAULTTONEAREST 2

// ------------------------------------------------------------------ tiny C runtime replacement
extern "C" {
int _fltused = 0x9875;
int abs(int x){return x<0?-x:x;}
void *memset(void *d, int c, size_t n) { void *r = d; __asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory"); return r; }
void *memcpy(void *d, const void *s, size_t n) { void *r = d; __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(n) : : "memory"); return r; }
void *memmove(void *d, const void *s, size_t n) {
  unsigned char *a = (unsigned char *)d; const unsigned char *b = (const unsigned char *)s;
  if (a < b) { while (n--) *a++ = *b++; } else { a += n; b += n; while (n--) *--a = *--b; }
  return d;
}
int memcmp(const void *x, const void *y, size_t n) { const unsigned char *a = (const unsigned char *)x, *b = (const unsigned char *)y; for (; n; n--, a++, b++) if (*a != *b) return *a - *b; return 0; }
}

static size_t s_len(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static void s_cpy(char *d, const char *s, size_t cap) { size_t i = 0; for (; s[i] && i + 1 < cap; i++) d[i] = s[i]; d[i] = 0; }
static void s_cat(char *d, const char *s, size_t cap) { size_t n = s_len(d); s_cpy(d + n, s, cap > n ? cap - n : 0); }
static int s_ieq(const char *a, const char *b) {
  for (;; a++, b++) { char x = *a, y = *b; if (x >= 'a' && x <= 'z') x -= 32; if (y >= 'a' && y <= 'z') y -= 32; if (x != y) return 0; if (!x) return 1; }
}

static HANDLE g_heap, g_log;
void *sys_alloc(size_t n) { return HeapAlloc(g_heap, HEAP_ZERO_MEMORY, n ? n : 1); }
void *sys_realloc(void *p, size_t n) { return p ? HeapReAlloc(g_heap, HEAP_ZERO_MEMORY, p, n ? n : 1) : sys_alloc(n); }
void sys_free(void *p) { if (p) HeapFree(g_heap, 0, p); }
uint8_t *sys_read_file(const char *path, size_t *size) {
  HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (f == INVALID_HANDLE_VALUE) return 0;
  LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
  uint8_t *b = (uint8_t *)sys_alloc((size_t)sz.QuadPart + 1);
  DWORD got = 0; ReadFile(f, b, (DWORD)sz.QuadPart, &got, 0); CloseHandle(f);
  *size = got; sys_log_asset(path); return b;
}
static void make_parent_dir(const char *path){
  char d[600];s_cpy(d,path,sizeof d);
  for(size_t i=1;d[i];i++)if(d[i]=='/' || d[i]=='\\'){char c=d[i];d[i]=0;if(i>2 || d[1]!=':')CreateDirectoryA(d,0);d[i]=c;}
}
int sys_delete_file(const char *path) { return DeleteFileA(path) ? 1 : 0; }
int sys_write_file(const char *path, const void *data, size_t size) {
  HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (f == INVALID_HANDLE_VALUE) { make_parent_dir(path); f = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0); }
  if (f == INVALID_HANDLE_VALUE) { if(sys_debug_enabled()){char error[700]="[error] Could not write file: ";s_cat(error,path,sizeof error);sys_log(error);}return 0; }
  DWORD w = 0; WriteFile(f, data, (DWORD)size, &w, 0); CloseHandle(f); return w == size;
}
int sys_dir_exists(const char *path) { DWORD a = GetFileAttributesA(path); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
void sys_list_dir(const char *dir, const char *ext, void (*cb)(void *, const char *, int), void *user) {
  char pat[600]; s_cpy(pat, dir, 600); s_cat(pat, "/*", 600);
  WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  int want_dirs = ext[0] == '/' && !ext[1];
  do {
    if (fd.cFileName[0] == '.') continue;
    int isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (want_dirs) { if (isdir) cb(user, fd.cFileName, 1); continue; }
    size_t ln = s_len(fd.cFileName), le = s_len(ext);
    if (!isdir && ln > le && s_ieq(fd.cFileName + ln - le, ext)) cb(user, fd.cFileName, 0);
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}
#include "debug_console_win32.inc"

double sys_time(void) {
  static LARGE_INTEGER f; if (!f.QuadPart) QueryPerformanceFrequency(&f);
  LARGE_INTEGER c; QueryPerformanceCounter(&c); return (double)c.QuadPart / (double)f.QuadPart;
}
void *sys_fopen(const char *path) {
  HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  return f == INVALID_HANDLE_VALUE ? 0 : f;
}
int sys_fread_at(void *f, uint64_t off, void *buf, uint32_t n) {
  LARGE_INTEGER li; li.QuadPart = (long long)off;
  if (!SetFilePointerEx((HANDLE)f, li, 0, 0)) return 0;
  uint8_t *b = (uint8_t *)buf;
  while (n) { DWORD chunk = n > (64u << 20) ? (64u << 20) : n, got = 0; if (!ReadFile((HANDLE)f, b, chunk, &got, 0) || !got) return 0; b += got; n -= got; }
  return 1;
}
uint64_t sys_fsize(void *f) { LARGE_INTEGER sz; GetFileSizeEx((HANDLE)f, &sz); return (uint64_t)sz.QuadPart; }
void sys_fclose(void *f) { CloseHandle((HANDLE)f); }

// ---- audio: winmm waveOut, loaded at runtime, pumped from the main loop
struct WAVEFORMATEX_ { WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec; WORD nBlockAlign, wBitsPerSample, cbSize; };
struct WAVEHDR_ { char *lpData; DWORD dwBufferLength, dwBytesRecorded; uintptr_t dwUser; DWORD dwFlags, dwLoops; WAVEHDR_ *lpNext; uintptr_t reserved; };
typedef UINT(WINAPI *PFN_waveOutOpen)(HANDLE *, UINT, const WAVEFORMATEX_ *, uintptr_t, uintptr_t, DWORD);
typedef UINT(WINAPI *PFN_waveOutHdr)(HANDLE, WAVEHDR_ *, UINT);
static PFN_waveOutOpen p_waveOutOpen;
static PFN_waveOutHdr p_waveOutPrepareHeader, p_waveOutWrite;
static HANDLE g_wave;
static UINT(WINAPI *p_waveOutReset)(HANDLE);
enum { AUD_BUFS = 6, AUD_FRAMES = 1536 };
static WAVEHDR_ g_hdr[AUD_BUFS];
static int16_t *g_aud_mem;
void sys_audio_flush(){
  if(!g_wave || !p_waveOutReset)return;
  p_waveOutReset(g_wave);
  for(int i=0;i<AUD_BUFS;i++)g_hdr[i].dwFlags|=1;
  if(g_aud_mem)memset(g_aud_mem,0,AUD_BUFS*AUD_FRAMES*4);
}
int sys_audio_open(int rate) {
  HMODULE mm = LoadLibraryA("winmm.dll");
  if (!mm) {sys_log("[error] Audio unavailable: winmm.dll was not found.");return 0;}
  p_waveOutReset=(UINT(WINAPI *)(HANDLE))GetProcAddress(mm,"waveOutReset");
  p_waveOutOpen = (PFN_waveOutOpen)GetProcAddress(mm, "waveOutOpen");
  p_waveOutPrepareHeader = (PFN_waveOutHdr)GetProcAddress(mm, "waveOutPrepareHeader");
  p_waveOutWrite = (PFN_waveOutHdr)GetProcAddress(mm, "waveOutWrite");
  if (!p_waveOutOpen || !p_waveOutPrepareHeader || !p_waveOutWrite) {sys_log("[error] Audio unavailable: required waveOut functions are missing.");return 0;}
  WAVEFORMATEX_ wf; memset(&wf, 0, sizeof wf);
  wf.wFormatTag = 1; wf.nChannels = 2; wf.nSamplesPerSec = (DWORD)rate; wf.wBitsPerSample = 16; wf.nBlockAlign = 4; wf.nAvgBytesPerSec = (DWORD)rate * 4;
  if (p_waveOutOpen(&g_wave, 0xFFFFFFFFu, &wf, 0, 0, 0) != 0) { g_wave = 0;sys_log("[error] Could not open the Windows waveOut audio device.");return 0; }
  sys_log("[event] Audio output opened: Windows waveOut, 22050 Hz stereo.");
  g_aud_mem = (int16_t *)sys_alloc(AUD_BUFS * AUD_FRAMES * 4);
  for (int i = 0; i < AUD_BUFS; i++) {
    g_hdr[i].lpData = (char *)(g_aud_mem + i * AUD_FRAMES * 2); g_hdr[i].dwBufferLength = AUD_FRAMES * 4;
    p_waveOutPrepareHeader(g_wave, &g_hdr[i], sizeof(WAVEHDR_));
    g_hdr[i].dwFlags |= 1;  // treat as done -> gets filled by the first pump
  }
  return 1;
}
static void audio_pump(void) {
  if (!g_wave) return;
  for (int i = 0; i < AUD_BUFS; i++) {
    if (!(g_hdr[i].dwFlags & 1)) continue;  // WHDR_DONE
    app_audio_fill((int16_t *)g_hdr[i].lpData, AUD_FRAMES);
    g_hdr[i].dwFlags &= ~1u;
    p_waveOutWrite(g_wave, &g_hdr[i], sizeof(WAVEHDR_));
  }
}

int sys_render_text(const char *text, int font_px, uint8_t **rgba, int *w, int *h) {
  HDC dc = CreateCompatibleDC(0); if (!dc) return 0;
  HFONT font = CreateFontA(-font_px, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 4 /*ANTIALIASED*/, 0, "Consolas");
  HGDIOBJ oldf = SelectObject(dc, font);
  RECT r = {0, 0, 4096, 4096};
  DrawTextA(dc, text, -1, &r, DT_LEFT | DT_NOPREFIX | DT_CALCRECT);
  int W = (int)(r.right + 4), H = (int)(r.bottom + 4);
  BITMAPINFO bi; memset(&bi, 0, sizeof bi);
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
  bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
  void *bits = 0; HBITMAP bmp = CreateDIBSection(dc, &bi, 0, &bits, 0, 0);
  if (!bmp) { SelectObject(dc, oldf); DeleteObject(font); DeleteDC(dc); return 0; }
  HGDIOBJ oldb = SelectObject(dc, bmp);
  memset(bits, 0, (size_t)W * H * 4);
  SetBkMode(dc, TRANSPARENT); SetTextColor(dc, 0x00FFFFFF);
  RECT r2 = {2, 2, W, H};
  DrawTextA(dc, text, -1, &r2, DT_LEFT | DT_NOPREFIX);
  uint8_t *out = (uint8_t *)sys_alloc((size_t)W * H * 4);
  const uint8_t *src = (const uint8_t *)bits;
  for (int i = 0; i < W * H; i++) {
    uint8_t a = src[i * 4]; if (src[i * 4 + 1] > a) a = src[i * 4 + 1]; if (src[i * 4 + 2] > a) a = src[i * 4 + 2];
    out[i * 4] = out[i * 4 + 1] = out[i * 4 + 2] = out[i * 4 + 3] = a;
  }
  SelectObject(dc, oldb); SelectObject(dc, oldf); DeleteObject(bmp); DeleteObject(font); DeleteDC(dc);
  *rgba = out; *w = W; *h = H; return 1;
}

// ------------------------------------------------------------------ window + GL
static HMODULE g_opengl;
static HGLRC(WINAPI *p_wglCreateContext)(HDC);
static BOOL(WINAPI *p_wglMakeCurrent)(HDC, HGLRC);
static BOOL(WINAPI *p_wglDeleteContext)(HGLRC);
static void *(WINAPI *p_wglGetProcAddress)(const char *);
static void *gl_getproc(const char *name) {
  void *p = p_wglGetProcAddress(name);
  intptr_t v = (intptr_t)p;
  if (v == 0 || v == 1 || v == 2 || v == 3 || v == -1) p = GetProcAddress(g_opengl, name);
  return p;
}

static Input g_in;
static int g_w = 1280, g_h = 720, g_running = 1, g_lastx, g_lasty, g_havepos, g_fullscreen;
static RECT g_prev_rect;
static LONG_PTR g_prev_style;

static int map_vk(WPARAM vk) {
  switch (vk) {
    case 0x25: return K_LEFT; case 0x27: return K_RIGHT; case 0x26: return K_UP; case 0x28: return K_DOWN;
    case 0x21: return K_PGUP; case 0x22: return K_PGDN; case 0x20: return K_SPACE; case 0x1B: return K_ESC;
    case 0x7B: return K_F12; case 0x70: return K_F1; case 0x71: return K_F2; case 0x72: return K_F3; case 0x73: return K_F4; case 0x74: return K_F5; case 0x75: return K_F6; case 0x76: return K_F7; case 0x77: return K_F8; case 0x78: return K_F9; case 0x79: return K_F10; case 0x7A: return K_F11; case 0x0D: return K_ENTER; case 0x09: return K_TAB; case 0xBF: case 0x6F: return K_SLASH; case 0x08: return K_BACKSPACE; case 0x24: return K_HOME; case 0x6B: case 0xBB: return K_PLUS; case 0x6D: case 0xBD: return K_MINUS;
  }
  if (vk >= 'A' && vk <= 'Z') return (int)vk;
  if (vk >= '0' && vk <= '9') return (int)vk;
  if (vk >= 0x60 && vk <= 0x69) return (int)('0' + vk - 0x60);
  return 0;
}

static void toggle_fullscreen(HWND hwnd) {
  if (!g_fullscreen) {
    g_prev_style = GetWindowLongPtrA(hwnd, GWL_STYLE); GetWindowRect(hwnd, &g_prev_rect);
    MONITORINFO mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    SetWindowLongPtrA(hwnd, GWL_STYLE, (LONG_PTR)(WS_POPUP | WS_VISIBLE));
    SetWindowPos(hwnd, 0, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    g_fullscreen = 1;
  } else {
    SetWindowLongPtrA(hwnd, GWL_STYLE, g_prev_style);
    SetWindowPos(hwnd, 0, g_prev_rect.left, g_prev_rect.top, g_prev_rect.right - g_prev_rect.left, g_prev_rect.bottom - g_prev_rect.top, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    g_fullscreen = 0;
  }
}

static HWND g_hwnd;
void sys_set_fullscreen(int on) { if (g_hwnd && (on ? 1 : 0) != g_fullscreen) toggle_fullscreen(g_hwnd); }
int sys_is_fullscreen(void) { return g_fullscreen; }

static LRESULT WINAPI wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CLOSE: case WM_DESTROY: g_running = 0; PostQuitMessage(0); return 0;
    case WM_SIZE: g_w = (int)(lp & 0xffff); g_h = (int)((lp >> 16) & 0xffff); return 0;
    case WM_SYSKEYDOWN:
      if (wp == 0x0D) { toggle_fullscreen(hwnd); return 0; }  // Alt+Enter
      if(wp>=0x70&&wp<=0x7b&&!((lp>>29)&1)){int k=map_vk(wp);if(k){if(!((lp>>30)&1))g_in.pressed[k]=1;g_in.down[k]=1;}return 0;}
      break;
    case 0x0105: // WM_SYSKEYUP: F10 must not activate the Windows system menu.
      if(wp>=0x70&&wp<=0x7b&&!((lp>>29)&1)){g_in.down[map_vk(wp)]=0;return 0;}break;
    case WM_KEYDOWN: {
      // F11 is recording; Alt+Enter still toggles fullscreen.
      int k = map_vk(wp);
      if (k) { if (!((lp >> 30) & 1)) g_in.pressed[k] = 1; g_in.down[k] = 1; }
      return 0;
    }
    case WM_KEYUP: { int k = map_vk(wp); if (k) g_in.down[k] = 0; return 0; }
    case WM_LBUTTONDOWN: g_in.lmb = 1; g_in.lmb_pressed = 1; SetCapture(hwnd); return 0;
    case WM_LBUTTONUP: g_in.lmb = 0; ReleaseCapture(); return 0;
    case WM_RBUTTONDOWN: g_in.rmb = 1; g_in.rmb_pressed = 1; return 0;
    case WM_RBUTTONUP: g_in.rmb = 0; return 0;
    case WM_MOUSEWHEEL: g_in.wheel += (float)(short)((wp >> 16) & 0xffff) / 120.0f; return 0;
    case WM_MOUSEMOVE: {
      int x = (short)(lp & 0xffff), y = (short)((lp >> 16) & 0xffff);
      if (g_havepos) { g_in.mouse_dx += (float)(x - g_lastx); g_in.mouse_dy += (float)(y - g_lasty); }
      g_lastx = x; g_lasty = y; g_havepos = 1; g_in.mx = x; g_in.my = y; return 0;
    }
  }
  return DefWindowProcA(hwnd, msg, wp, lp);
}

static int parse_args(char *cmd, char **argv, int max) {
  int argc = 0;
  while (*cmd && argc < max) {
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    if (!*cmd) break;
    char *out = cmd; argv[argc++] = out; int q = 0;
    while (*cmd && (q || (*cmd != ' ' && *cmd != '\t'))) { if (*cmd == '"') { q = !q; cmd++; continue; } *out++ = *cmd++; }
    if (*cmd) cmd++;
    *out = 0;
  }
  return argc;
}

#include "gamedir_win32.inc"
#include "verify_progress_win32.inc"
int sys_game_language(void) { return gf_language; }
#include "startup_win32.inc"
#include "startup_choice.h"
#include "dos_win32.inc"
#include "encoder_win32.inc"
#include "i18n.h"
static int is_gamedir(const char *d) { return gd_usable(d); }

// ---- XInput (Xbox controllers), loaded at run time
struct XIGamepad { WORD buttons; BYTE lt, rt; short lx, ly, rx, ry; };
struct XIState { DWORD packet; XIGamepad pad; };
static DWORD(WINAPI *p_XInputGetState)(DWORD, XIState *);
static int g_xi_tried; static double g_xi_retry; static int g_xi_user = -1;
static float xi_axis(short v, int dz) { int a = v < 0 ? -v : v; if (a <= dz) return 0; float f = (float)(a - dz) / (32767 - dz); if (f > 1) f = 1; return v < 0 ? -f : f; }
int sys_pad_read(PadState *p) {
  p->connected = 0; p->buttons = 0; p->lx = p->ly = p->rx = p->ry = p->lt = p->rt = 0;
  if (!g_xi_tried) {
    g_xi_tried = 1;
    const char *dll[3] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    for (int i = 0; i < 3 && !p_XInputGetState; i++) { HMODULE m = LoadLibraryA(dll[i]); if (m) p_XInputGetState = (DWORD(WINAPI *)(DWORD, XIState *))GetProcAddress(m, "XInputGetState"); }
  }
  if (!p_XInputGetState) return 0;
  double now = sys_time();
  if (g_xi_user < 0) {   // look for a pad (not every frame: XInputGetState is slow for empty slots)
    if (now < g_xi_retry) return 0;
    g_xi_retry = now + 1.5;
    for (DWORD u = 0; u < 4; u++) { XIState st; if (p_XInputGetState(u, &st) == 0) { g_xi_user = (int)u; break; } }
    if (g_xi_user < 0) return 0;
  }
  XIState st;
  if (p_XInputGetState((DWORD)g_xi_user, &st) != 0) { g_xi_user = -1; return 0; }
  p->connected = 1; p->buttons = st.pad.buttons;
  p->lx = xi_axis(st.pad.lx, 7849); p->ly = xi_axis(st.pad.ly, 7849); p->rx = xi_axis(st.pad.rx, 8689); p->ry = xi_axis(st.pad.ry, 8689);
  p->lt = st.pad.lt > 30 ? (st.pad.lt - 30) / 225.0f : 0; p->rt = st.pad.rt > 30 ? (st.pad.rt - 30) / 225.0f : 0;
  return 1;
}

static void fatal(const char *msg,const char *debug_msg=0) { sys_log(ui_debug_text(debug_msg?debug_msg:msg));msg=ui_translate(msg); MessageBoxA(0, msg, OPENMR2_FULLTITLE, MB_ICONERROR); ExitProcess(1); }

extern "C" void WinMainCRTStartup(void) {
  g_heap = GetProcessHeap();
  char exedir[600]; GetModuleFileNameA(0, exedir, 600);
  { size_t n = s_len(exedir); while (n && exedir[n - 1] != '\\' && exedir[n - 1] != '/') n--; exedir[n ? n - 1 : 0] = 0; }
  SetCurrentDirectoryA(exedir);
  if (str_has_ci(GetCommandLineA(), "-debug")) sys_debug_console(1);

  char *argv[64]; char cmd[4096]; s_cpy(cmd, GetCommandLineA(), sizeof cmd);
  int argc = parse_args(cmd, argv, 64);

  // ---- locate the MegaRace 2 installation
  static char gamedir[600]; gamedir[0] = 0;
  int validate_only=0;
  for (int i = 1; i < argc; i++) {
    if (s_ieq(argv[i], "--validate")) validate_only=1;
    else if (s_ieq(argv[i], "--game") && i + 1 < argc) s_cpy(gamedir, argv[++i], 600);
    else if (s_ieq(argv[i], "--size") && i + 1 < argc) {
      const char *s = argv[++i]; int a = 0, b = 0; while (*s >= '0' && *s <= '9') a = a * 10 + (*s++ - '0'); if (*s == 'x') s++; while (*s >= '0' && *s <= '9') b = b * 10 + (*s++ - '0');
      if (a > 64 && b > 64) { g_w = a; g_h = b; }
    }
  }
  if(validate_only){
    if(!gamedir[0] && !gd_cfg_read(gamedir,sizeof gamedir))s_cpy(gamedir,exedir,sizeof gamedir);
    char details[12000];int errors=gd_check(gamedir[0]?gamedir:exedir,details,sizeof details);
    char report[14000]="status = ";s_cat(report,gf_status==GOG_COMPLETE?"complete":gf_status==GOG_INCOMPLETE?"incomplete":"unsupported",sizeof report);
    s_cat(report,"\r\nlanguage = ",sizeof report);s_cat(report,gf_language>=0?GOG_LANGUAGES[gf_language]:"unknown",sizeof report);s_cat(report,"\r\n",sizeof report);s_cat(report,details,sizeof report);
    sys_write_file("validation.txt",report,s_len(report));ExitProcess(errors?gf_status==GOG_INCOMPLETE?2:3:0);
  }
  gf_progress_begin=verify_begin;gf_progress_end=verify_end;
  if(gd_cfg_flag("debug_console"))sys_debug_console(1);
  if (!gamedir[0]) gd_find(exedir,gamedir,sizeof gamedir);
  if (!gamedir[0]) {
    sys_log("[event] No complete game data found locally or at the saved path; opening folder selection.");
    MessageBoxA(0, "The MegaRace 2 game files were not found next to OpenMR2.exe or at the saved game location.\n\n"
                   "In the next window, select the folder where MegaRace 2 is installed\n"
                   "(the folder containing M.EXE and MEGARACE2.gog, for example C:\\GOG Games\\Megarace 2).\n\n"
                   "Your selection will be saved and used automatically on the next launch.", OPENMR2_FULLTITLE, MB_ICONINFORMATION);
    if (!gd_ask(gamedir,600,OPENMR2_FULLTITLE)){sys_log("[event] Game folder selection cancelled.");ExitProcess(0);}
  }
  {char absolute[600];if(gamedir[0] && GetFullPathNameA(gamedir,sizeof absolute,absolute,0))s_cpy(gamedir,absolute,sizeof gamedir);}
  {   // Cached launches check presence only; unverified folders are hashed once.
    static char list[3000]; int miss = gf_startup_check(gamedir,gd_exists,list,3000);
    if (miss) {
      char m[3600];s_cpy(m,gf_text("Game data validation failed:\n\n","Donn\xe9""es du jeu incompl\xe8tes ou endommag\xe9""es :\n\n","Spieldaten sind unvollst\xe4ndig oder besch\xe4""digt:\n\n"),sizeof m);s_cat(m, list, 3600);
      sys_log("[error] Game data validation failed. See the dialog for affected files.");sys_log(gamedir);
      MessageBoxA(0, m, OPENMR2_FULLTITLE, 0x10); ExitProcess(1);
    }
  }
  { char absolute[600]; if(GetFullPathNameA(gamedir,sizeof absolute,absolute,0))s_cpy(gamedir,absolute,sizeof gamedir); }
  gd_cfg_write(gamedir);
  sys_log("[event] " OPENMR2_FULLTITLE " starting");
  sys_log("Validated game data directory:");sys_log(gamedir);sys_log(GOG_LANGUAGES[gf_language]);

  // Native chooser runs before any game window or OpenGL/audio setup.
  SetProcessDPIAware();
  if(!startup_before_window(argc,argv))ExitProcess(0);
  // ---- window
  HINSTANCE inst = GetModuleHandleA(0);
  WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
  wc.cbSize = sizeof wc; wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW; wc.lpfnWndProc = wndproc; wc.hInstance = inst;
  wc.hCursor = LoadCursorA(0, (const char *)(uintptr_t)32512); wc.hIcon = LoadIconA(inst, (const char *)(uintptr_t)1);
  wc.lpszClassName = "OpenMR2Window";
  RegisterClassExA(&wc);
  RECT r = {0, 0, g_w, g_h}; AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, 0);
  int ww = r.right - r.left, wh = r.bottom - r.top;
  int sx = GetSystemMetrics(0), sy = GetSystemMetrics(1);
  HWND hwnd = CreateWindowExA(0, wc.lpszClassName, OPENMR2_FULLTITLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              (sx - ww) / 2 > 0 ? (sx - ww) / 2 : 0, (sy - wh) / 2 > 0 ? (sy - wh) / 2 : 0, ww, wh, 0, 0, inst, 0);
  if (!hwnd) fatal("Fenster konnte nicht erstellt werden.");
  g_hwnd = hwnd;
  HDC dc = GetDC(hwnd);

  // ---- OpenGL context
  g_opengl = LoadLibraryA("opengl32.dll");
  if (!g_opengl) fatal("opengl32.dll fehlt.");
  p_wglCreateContext = (HGLRC(WINAPI *)(HDC))GetProcAddress(g_opengl, "wglCreateContext");
  p_wglMakeCurrent = (BOOL(WINAPI *)(HDC, HGLRC))GetProcAddress(g_opengl, "wglMakeCurrent");
  p_wglDeleteContext = (BOOL(WINAPI *)(HGLRC))GetProcAddress(g_opengl, "wglDeleteContext");
  p_wglGetProcAddress = (void *(WINAPI *)(const char *))GetProcAddress(g_opengl, "wglGetProcAddress");
  PIXELFORMATDESCRIPTOR pfd; memset(&pfd, 0, sizeof pfd);
  pfd.nSize = sizeof pfd; pfd.nVersion = 1; pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.cColorBits = 32; pfd.cAlphaBits = 8; pfd.cDepthBits = 24; pfd.cStencilBits = 8;
  int pf = ChoosePixelFormat(dc, &pfd);
  if (!pf || !SetPixelFormat(dc, pf, &pfd)) fatal("Kein passendes OpenGL-Pixelformat gefunden.");
  HGLRC legacy = p_wglCreateContext(dc);
  if (!legacy || !p_wglMakeCurrent(dc, legacy)) fatal("OpenGL-Kontext konnte nicht erstellt werden. Bitte Grafiktreiber aktualisieren.");
  typedef HGLRC(WINAPI * PFNCCA)(HDC, HGLRC, const int *);
  PFNCCA cca = (PFNCCA)p_wglGetProcAddress("wglCreateContextAttribsARB");
  if (!cca) fatal("Der Grafiktreiber unterstuetzt kein modernes OpenGL (wglCreateContextAttribsARB fehlt).");
  const int attrs[] = {0x2091, 3, 0x2092, 3, 0x9126, 1 /*core*/, 0};
  HGLRC ctx = cca(dc, 0, attrs);
  if (!ctx) fatal("OpenGL 3.3 wird von diesem Grafiktreiber nicht unterstuetzt.");
  p_wglMakeCurrent(dc, ctx); p_wglDeleteContext(legacy);
  const char *miss = gl_load(gl_getproc);
  if (miss) { char m[300];s_cpy(m,ui_translate("OpenGL-Funktion fehlt: "),sizeof m); s_cat(m, miss, 300);sys_log(miss);fatal(m,"Missing OpenGL function. See the error dialog for its name."); }
  typedef BOOL(WINAPI * PFNSI)(int);
  PFNSI swapint = (PFNSI)p_wglGetProcAddress("wglSwapIntervalEXT");
  if (swapint) swapint(1);

  if (!app_init(gamedir, argc, argv)) fatal("Keine MegaRace-2-Modelle gefunden (erwartet: CARS\\<SATZ>\\*.M3D).");

  RECT cr; GetClientRect(hwnd, &cr); g_w = cr.right; g_h = cr.bottom;
  // 1 ms timer resolution for the frame pacing of the race (winmm, loaded at runtime)
  { HMODULE wm = LoadLibraryA("winmm.dll"); typedef unsigned(WINAPI * PFNTBP)(unsigned);
    PFNTBP tbp = wm ? (PFNTBP)GetProcAddress(wm, "timeBeginPeriod") : 0; if (tbp) tbp(1); }
  int cur_si = 1;
  double last = sys_time();
  char lasttitle[256] = "";
  while (g_running) {
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) { if (msg.message == WM_QUIT) g_running = 0; TranslateMessage(&msg); DispatchMessageA(&msg); }
    if (!g_running) break;
    if (g_w < 8 || g_h < 8) { audio_pump(); Sleep(30); continue; }
    { int si = app_swap_interval(); if (si != cur_si && swapint) { swapint(si); cur_si = si; } }
    for (;;) {   // wait until the core wants the next frame (Original race cadence / 60 FPS limit)
      double w = app_wait_time(); if (w <= 0.0003) break;
      audio_pump();
      if (w > 0.0025) Sleep((DWORD)((w > 0.006 ? 0.004 : w - 0.0015) * 1000)); else Sleep(0);
    }
    double now = sys_time(); float dt = (float)(now - last); last = now;
    audio_pump();
    if (!app_frame(g_w, g_h, dt, &g_in)) break;
    SwapBuffers(dc);
    memset(g_in.pressed, 0, sizeof g_in.pressed); g_in.mouse_dx = g_in.mouse_dy = g_in.wheel = 0; g_in.lmb_pressed = g_in.rmb_pressed = 0;
    const char *t = app_title();
    int same = 1; for (int i = 0;; i++) { if (t[i] != lasttitle[i]) { same = 0; break; } if (!t[i]) break; }
    if (!same) { s_cpy(lasttitle, t, 256); SetWindowTextA(hwnd, lasttitle); }
  }
  app_capture_stop();
  if (g_log && g_log != INVALID_HANDLE_VALUE) CloseHandle(g_log);
  ExitProcess(0);
}
