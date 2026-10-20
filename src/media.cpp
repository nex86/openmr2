// MegaRace 2 CD image, original video decoder host, audio decoding
#include "media.h"
#include "sys.h"
#include "sha256.h"
#include "gog_manifest.h"

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int16_t s16; typedef int32_t s32;
static u16 rd16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static char upc(char c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
static void mcpy(void *d, const void *s, size_t n) { u8 *a = (u8 *)d; const u8 *b = (const u8 *)s; while (n--) *a++ = *b++; }
static void mset(void *d, int v, size_t n) { u8 *a = (u8 *)d; while (n--) *a++ = (u8)v; }

// ======================================================================== ISO9660
static void *g_iso;
struct IsoEnt { u32 lba, size; int dir; };

int iso_open(const char *path) {
  if (g_iso) { sys_fclose(g_iso); g_iso = 0; }
  void *f = sys_fopen(path);
  if (!f) return 0;
  u8 pvd[2048];
  if (!sys_fread_at(f, 16 * 2048, pvd, 2048) || pvd[0] != 1 || pvd[1] != 'C' || pvd[2] != 'D' || pvd[3] != '0') { sys_fclose(f); return 0; }
  g_iso = f;
  return 1;
}
int iso_is_open(void) { return g_iso != 0; }

static int iso_root(IsoEnt *e) {
  u8 pvd[2048];
  if (!g_iso || !sys_fread_at(g_iso, 16 * 2048, pvd, 2048)) return 0;
  e->lba = rd32(pvd + 156 + 2); e->size = rd32(pvd + 156 + 10); e->dir = 1;
  return 1;
}

// iterate a directory extent; cb returns 1 to stop
static int iso_iter(const IsoEnt &d, int (*cb)(void *u, const char *name, const IsoEnt &e), void *u) {
  if (!d.dir || d.size > (16u << 20)) return 0;
  u8 *buf = (u8 *)sys_alloc(d.size + 2048);
  if (!sys_fread_at(g_iso, (uint64_t)d.lba * 2048, buf, d.size)) { sys_free(buf); return 0; }
  u32 o = 0; int stop = 0;
  while (o < d.size && !stop) {
    u8 l = buf[o];
    if (!l) { o = (o / 2048 + 1) * 2048; continue; }
    u8 nl = buf[o + 32]; char name[64]; int k = 0;
    for (int i = 0; i < nl && k < 63; i++) { char c = (char)buf[o + 33 + i]; if (c == ';') break; name[k++] = c; }
    name[k] = 0;
    if (!(nl == 1 && (buf[o + 33] == 0 || buf[o + 33] == 1))) {
      IsoEnt e; e.lba = rd32(buf + o + 2); e.size = rd32(buf + o + 10); e.dir = (buf[o + 25] & 2) != 0;
      if (k && name[k - 1] == '.') name[k - 1] = 0;
      stop = cb(u, name, e);
    }
    o += l;
  }
  sys_free(buf);
  return stop;
}

struct FindCtx { const char *want; IsoEnt found; int ok; };
static int find_cb(void *u, const char *name, const IsoEnt &e) {
  FindCtx *c = (FindCtx *)u;
  const char *a = name, *b = c->want;
  while (*a && *b && upc(*a) == upc(*b)) { a++; b++; }
  if (!*a && !*b) { c->found = e; c->ok = 1; return 1; }
  return 0;
}
static int iso_find(const char *path, IsoEnt *out) {
  IsoEnt cur; if (!iso_root(&cur)) return 0;
  char comp[64];
  while (*path) {
    while (*path == '/' || *path == '\\') path++;
    int k = 0; while (*path && *path != '/' && *path != '\\' && k < 63) comp[k++] = *path++;
    comp[k] = 0; if (!k) break;
    FindCtx c; c.want = comp; c.ok = 0;
    iso_iter(cur, find_cb, &c);
    if (!c.ok) return 0;
    cur = c.found;
  }
  *out = cur; return 1;
}
uint8_t *iso_load(const char *path, size_t *size) {
  IsoEnt e; if (!g_iso || !iso_find(path, &e) || e.dir) return 0;
  u8 *b = (u8 *)sys_alloc(e.size + 16);
  if (!b) return 0;
  if (!sys_fread_at(g_iso, (uint64_t)e.lba * 2048, b, e.size)) { sys_free(b); return 0; }
  *size = e.size; sys_log_asset(path); return b;
}
int iso_stream_begin(const char *path, IsoStream *s) {
  IsoEnt e; s->buf = 0; s->size = s->done = 0;
  if (!g_iso || !iso_find(path, &e) || e.dir) return 0;
  s->buf = (u8 *)sys_alloc(e.size + 16); if (!s->buf) return 0;
  s->off = (uint64_t)e.lba * 2048; s->size = e.size; sys_log_asset(path); return 1;
}
int iso_stream_step(IsoStream *s, uint32_t bytes) {
  if (!s->buf) return -1;
  uint32_t n = s->size - s->done; if (n > bytes) n = bytes;
  if (n && !sys_fread_at(g_iso, s->off + s->done, s->buf + s->done, n)) { sys_free(s->buf); s->buf = 0; return -1; }
  s->done += n; return s->done >= s->size;
}
struct ListCtx { void (*cb)(void *, const char *, uint32_t); void *user; };
static int list_cb(void *u, const char *name, const IsoEnt &e) { ListCtx *c = (ListCtx *)u; if (!e.dir) c->cb(c->user, name, e.size); return 0; }
void iso_list(const char *dir, void (*cb)(void *, const char *, uint32_t), void *user) {
  IsoEnt d; if (!g_iso || !iso_find(dir, &d) || !d.dir) return;
  ListCtx c; c.cb = cb; c.user = user;
  iso_iter(d, list_cb, &c);
}

struct IsoAllCtx {void (*cb)(void *,const char *,int);void *user;};
static int iso_all_cb(void *u,const char *name,const IsoEnt &e){IsoAllCtx *c=(IsoAllCtx *)u;c->cb(c->user,name,e.dir);return 0;}
void iso_list_all(const char *dir,void (*cb)(void *,const char *,int),void *user){IsoEnt d;if(!g_iso||!iso_find(dir,&d)||!d.dir)return;IsoAllCtx c={cb,user};iso_iter(d,iso_all_cb,&c);}

// ======================================================================== M.EXE (LE / DOS4GW) loader
// The original video codec is a hand-written, self-modifying x86 routine with jump tables.
// It has been statically translated to C (ubbdec.inc); it runs against a flat 64 MB image of the
// game's own executable (tables and variables live at their original addresses).
#define MEMSZ (64u << 20)
static u8 *g_M;
static int g_mexe_edition=-1;
#define RD32(a) (*(uint32_t *)(M + ((a) & (MEMSZ - 1))))
#define RD16(a) (*(uint16_t *)(M + ((a) & (MEMSZ - 1))))
#define RD8(a) (M[(a) & (MEMSZ - 1)])
#define WR32(a, v) (*(uint32_t *)(M + ((a) & (MEMSZ - 1))) = (uint32_t)(v))
#define WR16(a, v) (*(uint16_t *)(M + ((a) & (MEMSZ - 1))) = (uint16_t)(v))
#define WR8(a, v) (M[(a) & (MEMSZ - 1)] = (uint8_t)(v))
static uint32_t g_ebp, g_edi;
static void ubb_bad_jump(uint32_t a) { (void)a; sys_log("video decoder: invalid jump"); }
#include "ubbdec.inc"

int mexe_load(const char *path) {
  g_mexe_edition=-1;
  size_t n; u8 *d = sys_read_file(path, &n);
  if (!d) return 0;
  int edition=sys_game_language();
  // The one-time startup verifier established the edition. No hashing on cached starts.
  if(edition<0 || edition>2){sys_free(d);sys_log("M.EXE: game edition has not been verified");return 0;}
  // find the LE header behind the bound DOS4GW stub: an MZ header whose e_lfanew points to "LE\0\0"
  size_t le = 0, mz = 0;
  for (size_t i = 0; i + 0x40 < n && !le; i++) {
    if (d[i] != 'M' || d[i + 1] != 'Z') continue;
    u32 o = rd32(d + i + 0x3c);
    if (i + o + 0xc4 < n && d[i + o] == 'L' && d[i + o + 1] == 'E' && d[i + o + 2] == 0 && d[i + o + 3] == 0) { le = i + o; mz = i; }
  }
  if (!le) { sys_free(d); sys_log("M.EXE: LE header not found"); return 0; }
  const u8 *H = d + le;
  u32 npages = rd32(H + 0x14), pagesize = rd32(H + 0x28), lastpage = rd32(H + 0x2c);
  u32 objtab = rd32(H + 0x40), nobj = rd32(H + 0x44), pagemap = rd32(H + 0x48);
  u32 fixpage = rd32(H + 0x68), fixrec = rd32(H + 0x6c), datapages = rd32(H + 0x80);
  if (nobj > 16 || pagesize != 4096) { sys_free(d); return 0; }
  if (!g_M) g_M = (u8 *)sys_alloc(MEMSZ + 64);
  if (!g_M) { sys_free(d); return 0; }
  mset(g_M,0,MEMSZ);
  u32 obase[16], opmi[16], opmn[16];
  int *pobj = (int *)sys_alloc(sizeof(int) * (npages + 2)), *pidx = (int *)sys_alloc(sizeof(int) * (npages + 2));
  for (u32 i = 0; i < nobj; i++) {
    const u8 *o = H + objtab + i * 24;
    obase[i] = rd32(o + 4); opmi[i] = rd32(o + 12); opmn[i] = rd32(o + 16);
    for (u32 p = 0; p < opmn[i]; p++) {
      u32 pg = opmi[i] + p;
      const u8 *e = H + pagemap + (pg - 1) * 4;
      u32 num = ((u32)e[0] << 16) | ((u32)e[1] << 8) | e[2];
      u32 sz = pg < npages ? pagesize : lastpage;
      size_t src = mz + datapages + (size_t)(num - 1) * pagesize;
      u32 dst = obase[i] + p * pagesize;
      if (src + sz <= n && dst + sz < MEMSZ) mcpy(g_M + dst, d + src, sz);
      if (pg <= npages) { pobj[pg] = (int)i; pidx[pg] = (int)p; }
    }
  }
  for (u32 pg = 1; pg <= npages; pg++) {
    u32 a = rd32(H + fixpage + (pg - 1) * 4), b = rd32(H + fixpage + pg * 4);
    const u8 *p = H + fixrec + a, *end = H + fixrec + b;
    u32 pagevirt = obase[pobj[pg]] + (u32)pidx[pg] * pagesize;
    while (p < end) {
      u8 src = p[0], flg = p[1]; p += 2;
      int cnt = 1; s16 single = 0; int list = (src & 0x20) != 0;
      if (list) { cnt = *p++; } else { single = (s16)rd16(p); p += 2; }
      if (flg & 3) { sys_log("M.EXE: unsupported fixup"); break; }
      u32 tobj;
      if (flg & 0x40) { tobj = rd16(p); p += 2; } else tobj = *p++;
      u32 toff = 0; int st = src & 0xf;
      if (st != 2) { if (flg & 0x10) { toff = rd32(p); p += 4; } else { toff = rd16(p); p += 2; } }
      u32 target = (tobj >= 1 && tobj <= nobj ? obase[tobj - 1] : 0) + toff;
      for (int k = 0; k < cnt; k++) {
        s16 so = list ? (s16)rd16(p + k * 2) : single;
        u32 va = pagevirt + (u32)(s32)so;
        if (va + 4 >= MEMSZ) continue;
        if (st == 7) { u32 v = target; mcpy(g_M + va, &v, 4); }
        else if (st == 8) { u32 v = target - (va + 4); mcpy(g_M + va, &v, 4); }
      }
      if (list) p += cnt * 2;
    }
  }
  sys_free(pobj); sys_free(pidx); sys_free(d);
  // GOG decoder tables are identical after fixups, but translated strings shift
  // their data addresses: EN -1696, FR -16, DE 0. Normalize only this table block.
  // Copy backwards: source and destination overlap. No original files are changed.
  static const int table_delta[3]={-1696,-16,0};
  int delta=table_delta[edition];
  for(int i=23124-1;i>=0;i--)g_M[0x98220+i]=g_M[0x98220+delta+i];
  // The translated routine reads seven self-modifying instruction operands as data.
  // Their initial relocated pointers must use the same canonical table addresses.
  static const u32 operands[]={0x25546,0x26751,0x285aa,0x285ba,0x29972,0x29982,0x2a9d6};
  for(unsigned i=0;i<sizeof operands/sizeof operands[0];i++){u32 value=rd32(g_M+operands[i])-(s32)delta;mcpy(g_M+operands[i],&value,4);}
  // sanity check: the decoder's opcode jump table must point into the decoder
  u32 t0 = *(u32 *)(g_M + 0x98224 + 4 * 2);
  if (t0 < 0x254d0 || t0 > 0x2ba24) { sys_log("M.EXE: unexpected version (decoder table mismatch)"); return 0; }
  g_mexe_edition=edition;
  return 1;
}

int mexe_engine_profile(int car,int *level){
  if(level)*level=0;
  if(!g_M||g_mexe_edition<0||car<0||car>=14)return -1;
  static const u32 tables[3]={0xdcfac,0xdd63c,0xdd64c};
  u32 base=tables[g_mexe_edition],profile=rd32(g_M+base+car*4);
  if(profile>3)return -1;
  if(level)*level=(int)rd32(g_M+base+56+profile*36+32);
  return (int)profile;
}
int mexe_engine_frequency(int speed){
  if(!g_M||g_mexe_edition<0)return 14000;
  // Reverse uses half the magnitude, as in the original sound update at EN 0x4566c.
  if(speed<0)speed=-(speed/2);if(speed>1399)speed=1399;
  static const u32 tables[3]={0x858b6,0x85f42,0x85f5a};
  return (int)rd32(g_M+tables[g_mexe_edition]+speed*4);
}

// ======================================================================== UBB video
static const u32 BUF_A = 0x1000000, BUF_B = 0x1400000, BUF_IN = 0x1800000;
static Video *g_dec_owner;

// A streamed source reads headers/palettes for its index, and only the requested
// compressed superchunk during playback. It never allocates the whole UBB file.
static const u8 *video_bytes(Video *v,size_t offset,size_t count){
  if(offset>v->size||count>v->size-offset)return 0;
  if(!v->streaming)return v->data+offset;
  if(count>v->stream_capacity){v->stream_capacity=count+64;v->stream_buffer=(u8 *)sys_realloc(v->stream_buffer,v->stream_capacity);}
  if(!v->stream_buffer||!sys_fread_at(g_iso,v->stream_offset+offset,v->stream_buffer,count))return 0;
  v->stream_bytes_read+=count;return v->stream_buffer;
}
static int video_open_source(Video *v,uint8_t *data,size_t size,int height,int decode_audio,int streaming,uint64_t base){
  mset(v,0,sizeof *v);static unsigned generation;v->generation=++generation;
  v->data=data;v->size=size;v->streaming=streaming;v->stream_offset=base;v->decoded_frame=-1;v->fps=25;
  size_t o=0;const u8 *header=video_bytes(v,0,size>=4?4:size);
  if(!header)return 0;
  if(size>64&&header[0]=='U'&&header[1]=='B'&&header[2]=='B'&&header[3]=='2'){header=video_bytes(v,0,12);if(!header)return 0;v->w=rd16(header+8);v->h=rd16(header+10);o=64;}
  else{v->w=320;v->h=height;}
  if(v->w<=0||v->w>320||v->h<=0||v->h>480)return 0;
  int cap=1024,n=0,pcap=16;
  v->frame_ofs=(u32 *)sys_alloc(4*cap);v->keyframe=(u8 *)sys_alloc(cap);v->pal_id=(u16 *)sys_alloc(2*cap);
  v->pals=(u8(*)[768])sys_alloc(768*pcap);v->npals=1;mset(v->pals[0],0,768);
  u8 curpal[768];mset(curpal,0,768);s16 tab[256];int have_tab=0,scap=0;s32 al=0,ar=0;
  for(size_t q=o;q+8<size;){
    const u8 *h=video_bytes(v,q,4);if(!h)return 0;u32 ssz=rd32(h)&0xffffff;
    if(ssz<4||ssz>size-q)break;
    for(size_t pos=q+4;pos+8<=q+ssz;){
      h=video_bytes(v,pos,8);if(!h)return 0;u32 csz=rd32(h);if(csz<8||csz>q+ssz-pos)break;
      if(h[4]=='D'&&h[5]=='D'&&csz>=520){const u8 *d=video_bytes(v,pos+8,512);if(!d)return 0;for(int i=0;i<256;i++)tab[i]=(s16)rd16(d+i*2);have_tab=1;}
      pos+=(csz+3)&~3u;
    }
    if(have_tab)break;q+=ssz;if(q-o>200000)break;
  }
  int audio_frames=0;
  while(o+8<=size){
    const u8 *h=video_bytes(v,o,4);if(!h)return 0;u32 sh=rd32(h),ssz=sh&0xffffff;
    if(ssz<4||ssz>size-o)break;
    if(n==cap){cap*=2;v->frame_ofs=(u32 *)sys_realloc(v->frame_ofs,4*cap);v->keyframe=(u8 *)sys_realloc(v->keyframe,cap);v->pal_id=(u16 *)sys_realloc(v->pal_id,2*cap);}
    v->frame_ofs[n]=(u32)o;v->keyframe[n]=(sh>>31)&1;
    for(size_t pos=o+4;pos+8<=o+ssz;){
      h=video_bytes(v,pos,8);if(!h)return 0;u32 csz=rd32(h);if(csz<8||csz>o+ssz-pos)break;
      u32 cl=csz-8;int tag0=h[4],tag1=h[5],flags=h[6];
      if(tag0=='P'&&tag1=='L'&&cl>=2){
        const u8 *c=video_bytes(v,pos+8,cl);if(!c)return 0;int first=c[0],count=c[1]?c[1]:256;
        for(int i=0;i<count*3&&first*3+i<768&&2+i<(int)cl;i++)curpal[first*3+i]=c[2+i];
        if(v->npals==pcap){pcap*=2;v->pals=(u8(*)[768])sys_realloc(v->pals,768*pcap);}
        mcpy(v->pals[v->npals++],curpal,768);
      }
      if(tag0=='S'&&tag1=='D'&&have_tab){
        int stereo=(flags&0x80)!=0,nfr=stereo?(int)cl/2:(int)cl;audio_frames+=nfr;
        if(decode_audio){
          const u8 *c=video_bytes(v,pos+8,cl);if(!c)return 0;
          if(v->pcm_frames+nfr>scap){scap=(v->pcm_frames+nfr)*2+4096;v->pcm=(s16 *)sys_realloc(v->pcm,4*scap);}
          for(int i=0;i<nfr;i++){if(stereo){al=(s16)(al+tab[c[i*2]]);ar=(s16)(ar+tab[c[i*2+1]]);}else{al=(s16)(al+tab[c[i]]);ar=al;}
            v->pcm[(v->pcm_frames+i)*2]=(s16)al;v->pcm[(v->pcm_frames+i)*2+1]=(s16)ar;}
          v->pcm_frames+=nfr;
        }
      }
      pos+=(csz+3)&~3u;
    }
    v->pal_id[n]=(u16)(v->npals-1);n++;o+=ssz;
  }
  v->nframes=n;
  if(audio_frames>22050&&n>0){double fps=(double)n*22050.0/audio_frames;v->fps=fps<18?12:25;v->fps_exact=fps;}else v->fps_exact=v->fps;
  if(n)v->keyframe[0]=1;v->rgba=(u8 *)sys_alloc((size_t)v->w*v->h*4);return n>0;
}
int video_open(Video *v,uint8_t *data,size_t size,int height,int decode_audio){
  int ok=video_open_source(v,data,size,height,decode_audio,0,0);if(!ok)video_close(v);return ok;
}
int video_open_iso(Video *v,const char *path,int height,int decode_audio){
  IsoEnt e;if(!g_iso||!iso_find(path,&e)||e.dir){mset(v,0,sizeof *v);return 0;}
  // A unique ownership token also preserves the renderer's video cache identity.
  u8 *token=(u8 *)sys_alloc(1);int ok=video_open_source(v,token,e.size,height,decode_audio,1,(uint64_t)e.lba*2048);
  if(!ok)video_close(v);else sys_log_asset(path);return ok;
}
static int decode_one(Video *v,int i){
  u8 *M=g_M;size_t offset=v->frame_ofs[i];const u8 *head=video_bytes(v,offset,4);if(!head)return 0;
  u32 ssz=rd32(head)&0xffffff;const u8 *d=video_bytes(v,offset,ssz);if(!d)return 0;
  for(size_t pos=4;pos+8<=ssz;){u32 csz=rd32(d+pos);if(csz<8||csz>ssz-pos)break;
    if(d[pos+4]=='I'&&d[pos+5]=='V'&&csz-8<(4u<<20)){
      u32 t=RD32(0xa4ff4);WR32(0xa4ff4,RD32(0xa4ff8));WR32(0xa4ff8,t);
      mcpy(M+BUF_IN,d+pos+8,csz-8);mset(M+BUF_IN+csz-8,0,64);ubb_iv320(M,BUF_IN,0,0,0,0,0,0);
    }pos+=(csz+3)&~3u;
  }return 1;
}

int video_decode(Video *v, int frame) {
  if (!g_M || !v->nframes) return 0;
  if (frame < 0) frame = 0;
  if (frame >= v->nframes) frame = v->nframes - 1;
  u8 *M = g_M;
  if (g_dec_owner != v) { g_dec_owner = v; v->decoded_frame = -1; }
  if (frame != v->decoded_frame) {
    int k = frame; while (k > 0 && !v->keyframe[k]) k--;
    int start;
    if (v->decoded_frame >= k && v->decoded_frame < frame) start = v->decoded_frame + 1;
    else {
      WR32(0xa4ff4, BUF_A); WR32(0xa4ff8, BUF_B);
      mset(M + BUF_A, 0, 0x400000); mset(M + BUF_B, 0, 0x400000);
      start = k;
    }
    for (int i = start; i <= frame; i++) if(!decode_one(v, i)){v->decoded_frame=-1;return 0;}
    v->decoded_frame = frame;
  }
  const u8 *src = M + RD32(0xa4ff8);
  const u8 *pal = v->pals[v->pal_id[frame]];
  int np = v->w * v->h;
  for (int i = 0; i < np; i++) {
    const u8 *c = pal + src[i] * 3;
    u8 *o = v->rgba + i * 4;
    o[0] = (u8)((c[0] & 63) * 255 / 63); o[1] = (u8)((c[1] & 63) * 255 / 63); o[2] = (u8)((c[2] & 63) * 255 / 63); o[3] = 255;
  }
  return 1;
}

void video_close(Video *v) {
  if (g_dec_owner == v) g_dec_owner = 0;
  sys_free(v->data); sys_free(v->frame_ofs); sys_free(v->keyframe); sys_free(v->pal_id); sys_free(v->pals);
  sys_free(v->pcm); sys_free(v->rgba);sys_free(v->stream_buffer);
  mset(v, 0, sizeof *v);
}

// ======================================================================== WAV
int16_t *wav_decode(const uint8_t *d, size_t n, int *frames, int *channels, int *rate) {
  if (n < 44 || d[0] != 'R' || d[1] != 'I' || d[2] != 'F' || d[3] != 'F') return 0;
  int ch = 1, bits = 16; *rate = 22050;
  for (size_t o = 12; o + 8 <= n;) {
    u32 cs = rd32(d + o + 4);
    if (d[o] == 'f' && d[o + 1] == 'm' && d[o + 2] == 't') { ch = rd16(d + o + 10); *rate = (int)rd32(d + o + 12); bits = rd16(d + o + 22); }
    if (d[o] == 'd' && d[o + 1] == 'a' && d[o + 2] == 't' && d[o + 3] == 'a') {
      if (o + 8 + cs > n) cs = (u32)(n - o - 8);
      int bps = bits / 8 * ch; if (bps <= 0) return 0;
      int f = (int)(cs / bps);
      s16 *out = (s16 *)sys_alloc((size_t)f * ch * 2 + 4);
      for (int i = 0; i < f * ch; i++) out[i] = bits == 16 ? (s16)rd16(d + o + 8 + i * 2) : (s16)(((int)d[o + 8 + i] - 128) << 8);
      *frames = f; *channels = ch; return out;
    }
    o += 8 + cs + (cs & 1);
  }
  return 0;
}
