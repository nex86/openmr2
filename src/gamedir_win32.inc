// Finding / choosing the MegaRace 2 game folder on Windows.
// Needs: GetFileAttributesA, LoadLibraryA, GetProcAddress, MessageBoxA, sys_read_file, sys_write_file, sys_free,
// s_cpy / s_cat / s_len. The chosen folder is stored as "game_directory = ..." in revived_config/openmr2_setup.cfg
// next to the program.
#include "gamefiles.h"
#include "game_directory.h"

static int gd_exists(const char *p) { DWORD a=GetFileAttributesA(p);return a!=0xFFFFFFFFu && !(a&0x10); }
// minimum to run at all: M.EXE (video decoder) and the CD image
static int gd_usable(const char *d) {
  char p[700]; s_cpy(p, d, 700); s_cat(p, "/M.EXE", 700); if (!gd_exists(p)) return 0;
  for (int i = 0; i < GF_NIMAGES; i++) { s_cpy(p, d, 700); s_cat(p, "/", 700); s_cat(p, GF_IMAGES[i], 700); if (gd_exists(p)) return 1; }
  return 0;
}
static const char *GD_CFG = "revived_config/openmr2_setup.cfg";
static int gd_cfg_read(char *out, int cap) {
  size_t n=0;unsigned char *text=(unsigned char *)sys_read_file(GD_CFG,&n);
  int ok=game_directory_from_config(text,n,out,cap);sys_free(text);return ok;
}
static void gd_cfg_write(const char *dir) {   // replace / add the line, keep everything else
  size_t n = 0; char *t = (char *)sys_read_file(GD_CFG, &n);
  size_t cap = n + 1200; char *o = (char *)sys_alloc(cap); size_t on = 0;
  auto put = [&](const char *s, size_t k) { if (on + k < cap) { for (size_t i = 0; i < k; i++) o[on++] = s[i]; } };
  const char *hdr = "# Original game data directory\r\ngame_directory = ";
  put(hdr, s_len(hdr)); put(dir, s_len(dir)); put("\r\n", 2);
  size_t a = 0;
  while (t && a < n) {
    size_t e = a; while (e < n && t[e] != '\n') e++;
    const char *l = t + a; size_t L = e - a + (e < n ? 1 : 0); a = e + 1;
    const char *q = l; while (*q == ' ' || *q == '\t') q++;
    int skip = 0;
    { const char *key = "spielverzeichnis"; size_t i = 0; while (key[i] && cfg_ascii_lower(q[i]) == key[i]) i++; if (!key[i]) skip = 1; }
    { const char *key = "game_directory"; size_t i = 0; while (key[i] && cfg_ascii_lower(q[i]) == key[i]) i++; if (!key[i]) skip = 1; }
    { const char *key = "# Original game data directory"; size_t i = 0; while (key[i] && q[i] == key[i]) i++; if (!key[i]) skip = 1; }
    { const char *c = "# Ordner mit den Spieldateien"; size_t i = 0; while (c[i] && q[i] == c[i]) i++; if (!c[i]) skip = 1; }
    if (!skip) put(l, L);
  }
  sys_write_file(GD_CFG, o, on);
  sys_free(o); sys_free(t);
}
// folder dialog (shell32 / ole32 loaded at run time)
struct GD_BROWSEINFOA { void *hwndOwner; void *pidlRoot; char *pszDisplayName; const char *lpszTitle; unsigned ulFlags; void *lpfn; intptr_t lParam; int iImage; };
static int gd_browse(void *owner, const char *title, char *out) {
  HMODULE ole = LoadLibraryA("ole32.dll"), sh = LoadLibraryA("shell32.dll");
  if (!sh) return 0;
  typedef long(__stdcall * PCOI)(void *); PCOI coinit = ole ? (PCOI)GetProcAddress(ole, "CoInitialize") : 0;
  if (coinit) coinit(0);
  typedef void *(__stdcall * PBRW)(GD_BROWSEINFOA *); typedef int(__stdcall * PPATH)(void *, char *);
  PBRW brw = (PBRW)GetProcAddress(sh, "SHBrowseForFolderA"); PPATH gp = (PPATH)GetProcAddress(sh, "SHGetPathFromIDListA");
  if (!brw || !gp) return 0;
  char disp[600]; GD_BROWSEINFOA bi; char *z = (char *)&bi; for (size_t i = 0; i < sizeof bi; i++) z[i] = 0;
  bi.hwndOwner = owner; bi.pszDisplayName = disp; bi.lpszTitle = title; bi.ulFlags = 0x0001 | 0x0040;   // only folders, new dialog style
  void *pidl = brw(&bi); if (!pidl) return 0;
  int ok = gp(pidl, out);
  typedef void(__stdcall * PFREE)(void *); PFREE fr = ole ? (PFREE)GetProcAddress(ole, "CoTaskMemFree") : 0; if (fr) fr(pidl);
  return ok;
}
// asks for the game folder until a usable one is chosen; returns 0 when the user cancels
static int gd_ask(char *dir, int cap, const char *apptitle) {
  for (;;) {
    char pick[600];
    if (!gd_browse(0, gf_text("Choose the MegaRace 2 GOG folder (M.EXE and MEGARACE2.gog).","Choisissez le dossier GOG de MegaRace 2 (M.EXE et MEGARACE2.gog).","MegaRace-2-GOG-Ordner mit M.EXE und MEGARACE2.gog ausw\xe4hlen."), pick)) return 0;
    char details[3000];
    if (!gf_startup_check(pick,gd_exists,details,sizeof details)) { s_cpy(dir, pick, cap); gd_cfg_write(pick); return 1; }
    sys_log("[error] Selected game directory failed validation:");sys_log(pick);
    char m[4000];s_cpy(m,gf_text("Game data validation failed:\n\n","La validation des donn\xe9""es a \xe9""chou\xe9 :\n\n","Pr\xfc""fung der Spieldaten fehlgeschlagen:\n\n"),sizeof m);s_cat(m, pick, sizeof m);
    s_cat(m, "\n\n", sizeof m);s_cat(m,details,sizeof m);
    s_cat(m, gf_text("\nChoose another folder?","\nChoisir un autre dossier ?","\nAnderen Ordner ausw\xe4hlen?"), sizeof m);
    if (MessageBoxA(0, m, apptitle, 0x4 | 0x30) != 6) return 0;   // MB_YESNO | MB_ICONWARNING, IDYES = 6
  }
}
// complete check: returns the number of missing files, list in `list`
static int gd_check(const char *dir, char *list, int cap) { return gf_check(dir, gd_exists, list, cap); }
// Resolve without displaying UI: local complete data always wins over the saved path.
static int gd_find(const char *exedir, char *out, int cap) {
  char local[600];if(GetFullPathNameA(exedir,sizeof local,local,0))exedir=local;
  if (gf_startup_candidate(exedir,gd_exists)) { s_cpy(out,exedir,cap); return 1; }
  char saved[600];
  if (gd_cfg_read(saved,sizeof saved)) {char absolute[600];if(GetFullPathNameA(saved,sizeof absolute,absolute,0))s_cpy(saved,absolute,sizeof saved);}
  if (saved[0] && gf_startup_candidate(saved,gd_exists)) {
    s_cpy(out,saved,cap); return 1;
  }
  out[0]=0; return 0;
}

static int gd_cfg_flag(const char *key){
  size_t n=0;unsigned char *text=(unsigned char *)sys_read_file(GD_CFG,&n);int value=0;
  size_t a=n>=3 && text[0]==239 && text[1]==187 && text[2]==191?3:0;
  while(a<n){size_t end=a;while(end<n && text[end]!='\n')end++;size_t p=a;a=end+1;
    while(p<end && (text[p]==' '||text[p]=='\t'))p++;size_t k=0;
    while(p<end && key[k] && cfg_ascii_lower(text[p])==key[k]){p++;k++;}
    if(key[k])continue;while(p<end && (text[p]==' '||text[p]=='\t'))p++;
    if(p==end || text[p++]!='=')continue;while(p<end && (text[p]==' '||text[p]=='\t'))p++;
    value=p<end && text[p]=='1';
  }sys_free(text);return value;
}
