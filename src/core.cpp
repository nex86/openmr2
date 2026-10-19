// OpenMR2 (MegaRace 2 Revived) - portable core
// Loads the original MegaRace 2 (Cryo, 1996) 3D assets (.M3D models, .HLZ textures)
// and renders them on the GPU with OpenGL 3.3 core.
#include "sys.h"
#include "gl.h"
#include "media.h"
#include "i18n.h"

// ------------------------------------------------------------------ small runtime helpers
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;

static size_t str_len(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static void str_cpy(char *d, const char *s, size_t cap) { size_t i = 0; for (; s[i] && i + 1 < cap; i++) d[i] = s[i]; d[i] = 0; }
static void str_cat(char *d, const char *s, size_t cap) { size_t n = str_len(d); str_cpy(d + n, s, cap > n ? cap - n : 0); }
static char up(char c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
static int str_icmp(const char *a, const char *b) {
  for (;; a++, b++) { char x = up(*a), y = up(*b); if (x != y) return x < y ? -1 : 1; if (!x) return 0; }
}
static int str_icontains(const char *h, const char *n) {
  size_t ln = str_len(n); if (!ln) return 1;
  for (; *h; h++) { size_t i = 0; while (i < ln && h[i] && up(h[i]) == up(n[i])) i++; if (i == ln) return 1; }
  return 0;
}
static void str_int(char *d, size_t cap, long v) {
  char t[24]; int n = 0; int neg = v < 0; unsigned long u = neg ? -v : v;
  do { t[n++] = '0' + (u % 10); u /= 10; } while (u);
  char o[26]; int k = 0; if (neg) o[k++] = '-'; while (n) o[k++] = t[--n]; o[k] = 0;
  str_cat(d, o, cap);
}
static long str_toi(const char *s) { long v = 0; int neg = 0; if (*s == '-') { neg = 1; s++; } while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); return neg ? -v : v; }
static void mem_zero(void *p, size_t n) { u8 *b = (u8 *)p; while (n--) *b++ = 0; }
static void mem_cpy(void *d, const void *s, size_t n) { u8 *a = (u8 *)d; const u8 *b = (const u8 *)s; while (n--) *a++ = *b++; }
static u16 rd16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

template <class T> struct Arr {
  T *p; int n, cap;
  void push(const T &v) { if (n == cap) { cap = cap ? cap * 2 : 16; p = (T *)sys_realloc(p, sizeof(T) * cap); } p[n++] = v; }
  void clear() { n = 0; }
  void release() { if (p) sys_free(p); p = 0; n = cap = 0; }
};

// ------------------------------------------------------------------ math
static const float PI = 3.14159265358979f;
static float m_floor(float x) { float t = (float)(long long)x; return t > x ? t - 1 : t; }
static float m_sin(float x) {
  x = x - 2 * PI * m_floor(x / (2 * PI) + 0.5f);   // [-pi, pi]
  if (x > PI / 2) x = PI - x; else if (x < -PI / 2) x = -PI - x;
  float x2 = x * x;
  return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110)))));
}
static float m_cos(float x) { return m_sin(x + PI / 2); }
static float m_sqrt(float x) { return __builtin_sqrtf(x); }
static float m_abs(float x) { return x < 0 ? -x : x; }
static float m_min(float a, float b) { return a < b ? a : b; }
static float m_max(float a, float b) { return a > b ? a : b; }
static float m_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

struct V3 { float x, y, z; };
static V3 v3(float x, float y, float z) { V3 r = {x, y, z}; return r; }
static V3 operator+(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 operator-(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static V3 operator*(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static V3 norm(V3 a) { float l = m_sqrt(dot(a, a)); return l > 1e-20f ? a * (1.0f / l) : v3(0, 1, 0); }

struct M4 { float m[16]; };  // column-major
static M4 m4_id() { M4 r; mem_zero(&r, sizeof r); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
static M4 m4_mul(const M4 &a, const M4 &b) {
  M4 r;
  for (int c = 0; c < 4; c++) for (int rr = 0; rr < 4; rr++) {
    float s = 0; for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
    r.m[c * 4 + rr] = s;
  }
  return r;
}
static M4 m4_persp(float fovy, float aspect, float zn, float zf) {
  float f = m_cos(fovy / 2) / m_sin(fovy / 2);
  M4 r; mem_zero(&r, sizeof r);
  r.m[0] = f / aspect; r.m[5] = f; r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1; r.m[14] = 2 * zf * zn / (zn - zf);
  return r;
}
static M4 m4_look(V3 eye, V3 at, V3 upv) {
  V3 f = norm(at - eye), s = norm(cross(f, upv)), u = cross(s, f);
  M4 r = m4_id();
  r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
  r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
  r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
  r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
  return r;
}
static M4 m4_trans(V3 t) { M4 r = m4_id(); r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
static M4 m4_rotx(float a) { M4 r = m4_id(); float c = m_cos(a), s = m_sin(a); r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r; }

// ------------------------------------------------------------------ HLZ (Cryo LZ image) decoder
// Layout: 768 byte palette (8-bit RGB), u16 width, u16 height, LZ stream.
// LZ: 32-bit little-endian control words read MSB first.
//   1            -> literal byte
//   0 1 <u16 w>  -> copy (w&7 ? w&7 : next byte, 0 = end) + 2 bytes from offset ((w|0xffff0000) >> 3)
//   0 0 b b <u8> -> copy (bb + 2) bytes from offset (byte - 256)
static char g_gamedir[400];
static unsigned char g_model_reserved[24];static int g_model_reserved_ready;
static int model_palette_ensure(){
  if(g_model_reserved_ready)return 1;char path[512];str_cpy(path,g_gamedir,sizeof path);str_cat(path,"/DATA640/PAL_RACE.HLZ",sizeof path);
  size_t n=0;unsigned char *d=sys_read_file(path,&n);if(!d || n<768){sys_free(d);return 0;}
  mem_cpy(g_model_reserved,d+248*3,sizeof g_model_reserved);sys_free(d);g_model_reserved_ready=1;return 1;
}
// Only the two explicit accent markers are substituted. Later slots can be real white/gray.
static void model_palette_resolve(u8 *palette,const u8 *reserved){
  if(palette[744]==128 && !palette[745] && palette[746]==128)mem_cpy(palette+744,reserved,3);
  if(!palette[747] && palette[748]==252 && !palette[749])mem_cpy(palette+747,reserved+3,3);
}
struct Image { int w, h; u8 *rgba; u8 pal[768]; };

static int hlz_unpack(const u8 *d, size_t n, size_t pos, u8 *out, size_t cap) {
  size_t o = 0; u32 bits = 0; int nb = 0;
#define HLZ_BIT(dst) do { if (!nb) { if (pos + 4 > n) return -1; bits = rd32(d + pos); pos += 4; nb = 32; } nb--; dst = (bits >> nb) & 1; } while (0)
  for (;;) {
    int b; HLZ_BIT(b);
    if (b) { if (pos >= n || o >= cap) return -1; out[o++] = d[pos++]; continue; }
    int cnt; long off;
    HLZ_BIT(b);
    if (b) {
      if (pos + 2 > n) return -1;
      u32 w = rd16(d + pos); pos += 2;
      off = (long)((s32)(w | 0xffff0000u) >> 3);
      cnt = w & 7;
      if (!cnt) { if (pos >= n) return -1; cnt = d[pos++]; if (!cnt) break; }
    } else {
      int b1, b2; HLZ_BIT(b1); HLZ_BIT(b2);
      cnt = (b1 << 1) | b2;
      if (pos >= n) return -1;
      off = (long)d[pos++] - 256;
    }
    cnt += 2;
    if ((long)o + off < 0 || o + cnt > cap) return -1;
    for (int i = 0; i < cnt; i++, o++) out[o] = out[o + off];
  }
#undef HLZ_BIT
  return (int)o;
}

#include "textures.inc"

// Reject isolated marker speckles; only connected 4-neighbour groups of >=4 pixels emit red light.
static void lamp_cluster_filter(u8 *mask,int w,int h){
  size_t count=(size_t)w*h;if(!mask||!count)return;u8 *visited=(u8 *)sys_alloc(count);mem_zero(visited,count);u32 *queue=(u32 *)sys_alloc(count*sizeof(u32));
  for(size_t start=0;start<count;start++)if(mask[start*4]&&!visited[start]){
    size_t end=1;queue[0]=(u32)start;visited[start]=1;
    for(size_t at=0;at<end;at++){u32 p=queue[at];int x=p%w,y=p/w;u32 adjacent[4]={p-1,p+1,p-(u32)w,p+(u32)w};int valid[4]={x>0,x+1<w,y>0,y+1<h};for(int k=0;k<4;k++)if(valid[k]&&mask[adjacent[k]*4]&&!visited[adjacent[k]]){visited[adjacent[k]]=1;queue[end++]=adjacent[k];}}
    if(end<4)for(size_t k=0;k<end;k++)mask[queue[k]*4]=0;
  }
  sys_free(queue);sys_free(visited);
}

static int image_load_hlz(const char *path, Image *img,const unsigned char *reserved=0,u8 **lamp_mask=0) {
  if(lamp_mask)*lamp_mask=0;
  size_t n; u8 *d = sys_read_file(path, &n);
  if (!d) return 0;
  if (n < 776) { sys_free(d); return 0; }
  mem_cpy(img->pal, d, 768);
  int red_marker=d[744]==128&&!d[745]&&d[746]==128;int yellow_marker=!d[747]&&d[748]==252&&!d[749];
  if(reserved)model_palette_resolve(img->pal,reserved);
  img->w = rd16(d + 768); img->h = rd16(d + 770);
  size_t np = (size_t)img->w * img->h;
  u8 *idx = (u8 *)sys_alloc(np + 16);
  int got = hlz_unpack(d, n, 772, idx, np);
  sys_free(d);
  if (got < (int)np) {
    char m[300] = "HLZ decode short: "; str_cat(m, path, 300); sys_log(m);
    if (got < 0) { sys_free(idx); return 0; }
  }
  img->rgba = (u8 *)sys_alloc(np * 4);
  for (size_t i = 0; i < np; i++) {
    const u8 *c = img->pal + idx[i] * 3;
    img->rgba[i * 4 + 0] = c[0]; img->rgba[i * 4 + 1] = c[1]; img->rgba[i * 4 + 2] = c[2]; img->rgba[i * 4 + 3] = 255;
  }
  if(lamp_mask){*lamp_mask=(u8 *)sys_alloc(np*4);for(size_t i=0;i<np;i++){u8 *p=*lamp_mask+i*4;p[0]=red_marker&&idx[i]==248?255:0;p[1]=yellow_marker&&idx[i]==249?255:0;p[2]=0;p[3]=255;}lamp_cluster_filter(*lamp_mask,img->w,img->h);}
  sys_free(idx);
  return 1;
}

// ------------------------------------------------------------------ GL helpers
static float g_max_aniso = 1;
static int g_smooth_filter = 1;

static void tex_params(int smooth) {
  if (smooth) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (g_max_aniso > 1) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, m_min(16.0f, g_max_aniso));
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (g_max_aniso > 1) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);
  }
}

static GLuint tex_upload(const u8 *rgba, int w, int h, int mip, int clamp) {
  GLuint t; glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
  if (mip) { glGenerateMipmap(GL_TEXTURE_2D); tex_params(g_smooth_filter); }
  else { glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); }
  return t;
}

static GLuint shader_build(const char *vs, const char *fs) {
  const char *src[2] = {vs, fs}; GLenum ty[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
  GLuint p = glCreateProgram();
  for (int i = 0; i < 2; i++) {
    GLuint s = glCreateShader(ty[i]);
    glShaderSource(s, 1, &src[i], 0); glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; GLsizei l = 0; glGetShaderInfoLog(s, 2047, &l, log); log[l] = 0; sys_log("shader compile error:"); sys_log(log); }
    glAttachShader(p, s);
  }
  glLinkProgram(p);
  GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) { char log[2048]; GLsizei l = 0; glGetProgramInfoLog(p, 2047, &l, log); log[l] = 0; sys_log("program link error:"); sys_log(log); }
  return p;
}

// ------------------------------------------------------------------ M3D model (Didier Bouchon 3D engine, "70BD")
// Header: "70BD", u32 0x40, u32 nodeCount, u32 nodeTableOfs, u32 faceTotal, u32 memBase, u32 dataOfs, u32 fileSize
//         char copyright[32]
// 0x40:   material table, 30-byte entries, terminated by 0xffffffff:
//           char name[14] ("XXX.GIF" -> XXX.HLZ), u32 ptr, u8 info[8], u16 type, u16 pad
//           type 0 = untextured, 1 = textured, 3 = additive (fx), 4 = translucent
// nodeTableOfs: nodeCount nodes of 248 bytes:
//           +4 u32 parent node (file offset), +12 s32 pos[3] (relative to parent),
//           +0x78 4 LODs x 8 u32: (0, nVA, ptrVA, nVB, ptrVB, nFaces, ptrFaces, 0)
// vertices: 16 bytes (s16 x,y,z + runtime scratch); faces: 28 bytes:
//           u32 vptr[3] (memory pointers: fileofs = ptr - memBase + dataOfs),
//           3 x (u16 shadeIdx, u8 u, u8 v), u16 material (byte offset into material table), u16 flags
enum { MT_FLAT = 0, MT_TEX = 1, MT_ADD = 3, MT_TRANS = 4, MT_SHADOW = 99 };
struct Vtx { float p[3], n[3], uv[2], lamp_facing; };
struct Batch { int first, count, mat; };
struct Mesh { GLuint vao, vbo; int nverts; Batch b[24]; int nb; V3 bmin, bmax; int ok; };
struct Node { int parent; V3 pos; Mesh lod[4]; int spin; };
struct Mat { int type; int tex; char name[16]; };
enum { KIND_STATIC, KIND_FRAMES };
struct Model {
  char name[64]; Node nodes[32]; int nn;
  Mat mats[24]; int nmat;
  GLuint tex[8],lamp_mask[8]; char texture_keys[8][512]; char texname[8][16]; int texw[8], texh[8]; int texmissing[8]; int ntex;
  int kind; V3 bmin, bmax; int tris[4]; int loaded; int shadow_mat;
};

static const float MODEL_SCALE = 1.0f / 1000.0f;

static int model_find_tex(Model *m, const char *dir, const char *gif) {
  for (int i = 0; i < m->ntex; i++) if (!str_icmp(m->texname[i], gif)) return i;
  if (m->ntex >= 8) return -1;
  char base[16]; str_cpy(base, gif, 16);
  for (char *p = base; *p; p++) if (*p == '.') { *p = 0; break; }
  char path[512]; str_cpy(path, dir, 512); str_cat(path, "/", 512); str_cat(path, base, 512); str_cat(path, ".HLZ", 512);
  Image img; mem_zero(&img, sizeof img);
  int i = m->ntex;
  str_cpy(m->texname[i], gif, 16);
  texture_key(m->texture_keys[i],path,0);str_cat(m->texture_keys[i],".png",512);
  if (!model_palette_ensure())return -1;
  u8 *lamp_pixels=0;
  if (image_load_hlz(path, &img,g_model_reserved,&lamp_pixels)) {
    m->lamp_mask[i]=tex_upload(lamp_pixels,img.w,img.h,0,0);sys_free(lamp_pixels);
    m->tex[i] = tex_upload(img.rgba, img.w, img.h, 1, 0);
    m->texw[i] = img.w; m->texh[i] = img.h;
    sys_free(img.rgba);
  } else {
    char msg[600] = "texture missing: "; str_cat(msg, path, 600); sys_log(msg);
    u8 white[4 * 4]; for (int k = 0; k < 16; k++) white[k] = 200;
    m->tex[i] = tex_upload(white, 2, 2, 1, 0); m->texw[i] = m->texh[i] = 256; m->texmissing[i] = 1;
  }
  m->ntex++;
  return i;
}

struct Face { int v[3]; int uv[3][2]; int mat; };

static int mat_order(int type) { return type == MT_ADD ? 2 : (type == MT_TRANS || type == MT_SHADOW) ? 1 : 0; }

static void mesh_build(Model *m, Mesh *mesh, const u8 *d, size_t n, u32 base, u32 dofs, u32 nf, u32 fofs, int *tri_count) {
  mesh->ok = 0;
  if (!nf || fofs + nf * 28 > n) return;
  Arr<Face> faces = {};
  for (u32 i = 0; i < nf; i++) {
    const u8 *f = d + fofs + i * 28;
    Face fc;
    int bad = 0;
    for (int k = 0; k < 3; k++) {
      long vo = (long)rd32(f + k * 4) - (long)base + (long)dofs;
      if (vo < (long)dofs || vo + 6 > (long)n) bad = 1;
      fc.v[k] = (int)vo;
      fc.uv[k][0] = f[12 + k * 4 + 2];
      fc.uv[k][1] = f[12 + k * 4 + 3];
    }
    fc.mat = rd16(f + 24) / 30;
    if (fc.mat < 0 || fc.mat >= m->nmat) fc.mat = 0;
    if (!bad) faces.push(fc);
  }
  *tri_count += faces.n;
  // positions + face normals
  int nc = faces.n * 3;
  V3 *pos = (V3 *)sys_alloc(sizeof(V3) * nc);
  V3 *fn = (V3 *)sys_alloc(sizeof(V3) * faces.n);
  for (int i = 0; i < faces.n; i++) {
    for (int k = 0; k < 3; k++) {
      const u8 *v = d + faces.p[i].v[k];
      // original data: x right, y up, z forward (int16 units)
      pos[i * 3 + k] = v3((s16)rd16(v) * MODEL_SCALE, (s16)rd16(v + 2) * MODEL_SCALE, (s16)rd16(v + 4) * MODEL_SCALE);
    }
    fn[i] = norm(cross(pos[i * 3 + 1] - pos[i * 3], pos[i * 3 + 2] - pos[i * 3]));
  }
  // untextured horizontal polygons at the very bottom of a mesh are the original engine's fake drop shadow
  {
    float miny = 1e9f;
    for (int c = 0; c < nc; c++) miny = m_min(miny, pos[c].y);
    for (int i = 0; i < faces.n; i++) {
      if (m->mats[faces.p[i].mat].type != MT_FLAT || m_abs(fn[i].y) < 0.9f) continue;
      int low = 1; for (int k = 0; k < 3; k++) if (pos[i * 3 + k].y > miny + 0.05f) low = 0;
      if (low && m->shadow_mat >= 0) faces.p[i].mat = m->shadow_mat;
    }
  }
  // smoothed normals: average faces that share a corner position and have similar orientation
  // (orientation compared with abs(dot) so inconsistent winding in the source data does not break it)
  V3 *cn = (V3 *)sys_alloc(sizeof(V3) * nc);
  const float crease = 0.6f;
  for (int c = 0; c < nc; c++) {
    int fi = c / 3; V3 acc = fn[fi];
    for (int c2 = 0; c2 < nc; c2++) {
      int f2 = c2 / 3; if (f2 == fi) continue;
      V3 dp = pos[c2] - pos[c];
      if (m_abs(dp.x) + m_abs(dp.y) + m_abs(dp.z) > 1e-6f) continue;
      float dd = dot(fn[fi], fn[f2]);
      if (dd > crease) acc = acc + fn[f2];
      else if (dd < -crease) acc = acc - fn[f2];
    }
    cn[c] = norm(acc);
  }
  // emit vertices grouped by material (opaque first, translucent, additive last)
  Vtx *vb = (Vtx *)sys_alloc(sizeof(Vtx) * nc);
  int w = 0; mesh->nb = 0;
  V3 bmin = v3(1e9f, 1e9f, 1e9f), bmax = v3(-1e9f, -1e9f, -1e9f);
  for (int pass = 0; pass < 3; pass++)
    for (int mt = 0; mt < m->nmat; mt++) {
      if (mat_order(m->mats[mt].type) != pass) continue;
      int start = w;
      int tw = 256, th = 256;
      if (m->mats[mt].tex >= 0) { tw = m->texw[m->mats[mt].tex]; th = m->texh[m->mats[mt].tex]; }
      for (int i = 0; i < faces.n; i++) {
        if (faces.p[i].mat != mt) continue;
        for (int k = 0; k < 3; k++) {
          Vtx &o = vb[w++];
          V3 p = pos[i * 3 + k], nn = cn[i * 3 + k];
          o.p[0] = p.x; o.p[1] = p.y; o.p[2] = p.z;
          o.n[0] = nn.x; o.n[1] = nn.y; o.n[2] = nn.z;o.lamp_facing=m_abs(fn[i].z);
          o.uv[0] = (faces.p[i].uv[k][0] + 0.5f) / tw;
          o.uv[1] = (faces.p[i].uv[k][1] + 0.5f) / th;
          bmin = v3(m_min(bmin.x, p.x), m_min(bmin.y, p.y), m_min(bmin.z, p.z));
          bmax = v3(m_max(bmax.x, p.x), m_max(bmax.y, p.y), m_max(bmax.z, p.z));
        }
      }
      if (w > start && mesh->nb < 24) { Batch &b = mesh->b[mesh->nb++]; b.first = start; b.count = w - start; b.mat = mt; }
    }
  mesh->nverts = w; mesh->bmin = bmin; mesh->bmax = bmax;
  glGenVertexArrays(1, &mesh->vao); glBindVertexArray(mesh->vao);
  glGenBuffers(1, &mesh->vbo); glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vtx) * (w ? w : 1), vb, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void *)0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void *)12);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void *)24);
  glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
  glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void *)32);glEnableVertexAttribArray(3);
  glBindVertexArray(0);
  mesh->ok = w > 0;
  sys_free(vb); sys_free(cn); sys_free(fn); sys_free(pos); faces.release();
}

static void model_free(Model *m) {
  for (int i = 0; i < m->nn; i++)
    for (int l = 0; l < 4; l++) {
      Mesh &me = m->nodes[i].lod[l];
      if (me.vbo) { glDeleteBuffers(1, &me.vbo); glDeleteVertexArrays(1, &me.vao); }
    }
  if (m->ntex) {glDeleteTextures(m->ntex, m->tex);glDeleteTextures(m->ntex,m->lamp_mask);}
  mem_zero(m, sizeof *m);
}

static int model_load(Model *m, const char *dir, const char *file) {
  mem_zero(m, sizeof *m);
  str_cpy(m->name, file, 64);
  char path[512]; str_cpy(path, dir, 512); str_cat(path, "/", 512); str_cat(path, file, 512);
  size_t n; u8 *d = sys_read_file(path, &n);
  if (!d) return 0;
  if (n < 0x60 || d[0] != '7' || d[1] != '0' || d[2] != 'B' || d[3] != 'D') { sys_free(d); return 0; }
  u32 nn = rd32(d + 8), ntab = rd32(d + 12), base = rd32(d + 20), dofs = rd32(d + 24);
  // materials
  for (u32 o = 0x40; o + 30 <= n && m->nmat < 24; o += 30) {
    if (rd32(d + o) == 0xffffffffu) break;
    Mat &mt = m->mats[m->nmat++];
    str_cpy(mt.name, (const char *)d + o, 15);
    mt.type = rd16(d + o + 26);
    mt.tex = -1;
    if (mt.name[0] && mt.type != MT_FLAT) mt.tex = model_find_tex(m, dir, mt.name);
  }
  m->shadow_mat = -1;
  if (m->nmat < 24) { Mat &sm = m->mats[m->nmat]; sm.type = MT_SHADOW; sm.tex = -1; str_cpy(sm.name, "shadow", 16); m->shadow_mat = m->nmat++; }
  if (nn > 32) nn = 32;
  m->nn = (int)nn;
  int all_fx = 1, any_mat = 0;
  for (int i = 1; i < m->nmat; i++) { if (m->mats[i].type == MT_SHADOW) continue; any_mat = 1; if (m->mats[i].type != MT_ADD) all_fx = 0; }
  m->kind = (any_mat && all_fx && nn >= 6) ? KIND_FRAMES : KIND_STATIC;
  for (u32 i = 0; i < nn; i++) {
    u32 no = ntab + i * 248;
    if (no + 248 > n) { m->nn = (int)i; break; }
    Node &nd = m->nodes[i];
    u32 par = rd32(d + no + 4);
    nd.parent = -1;
    if (i > 0 && par >= ntab && par < no && (par - ntab) % 248 == 0) nd.parent = (int)((par - ntab) / 248);
    nd.pos = v3((s32)rd32(d + no + 12) * MODEL_SCALE, (s32)rd32(d + no + 16) * MODEL_SCALE, (s32)rd32(d + no + 20) * MODEL_SCALE);
    if (i == 0 || m->kind == KIND_FRAMES) nd.pos = v3(0, 0, 0);
    for (int l = 0; l < 4; l++) {
      const u8 *e = d + no + 0x78 + l * 32;
      u32 nf = rd32(e + 20), fp = rd32(e + 24);
      if (!nf) continue;
      // identical LOD entries (static props) -> build once
      if (l > 0 && rd32(e + 24) == rd32(e - 32 + 24)) { nd.lod[l] = nd.lod[l - 1]; nd.lod[l].vbo = 0; m->tris[l] += (int)nf; continue; }
      mesh_build(m, &nd.lod[l], d, n, base, dofs, nf, fp, &m->tris[l]);
    }
  }
  // wheel spin: child nodes of the root on 3-node vehicles that use regular textures
  if (m->kind == KIND_STATIC && m->nn == 3) {
    for (int i = 1; i < 3; i++) {
      Mesh &me = m->nodes[i].lod[0]; int fx = 0;
      for (int b = 0; b < me.nb; b++) if (m->mats[me.b[b].mat].type == MT_ADD) fx = 1;
      if (!fx && m->nodes[i].parent == 0) m->nodes[i].spin = 1;
    }
  }
  // bounds (LOD0)
  V3 bmin = v3(1e9f, 1e9f, 1e9f), bmax = v3(-1e9f, -1e9f, -1e9f);
  for (int i = 0; i < m->nn; i++) {
    if (m->kind == KIND_FRAMES && i > 0) break;
    V3 off = v3(0, 0, 0);
    for (int j = i; j >= 0 && j < m->nn; j = m->nodes[j].parent) { off = off + m->nodes[j].pos; if (m->nodes[j].parent == j) break; }
    Mesh &me = m->nodes[i].lod[0];
    if (!me.ok) continue;
    bmin = v3(m_min(bmin.x, me.bmin.x + off.x), m_min(bmin.y, me.bmin.y + off.y), m_min(bmin.z, me.bmin.z + off.z));
    bmax = v3(m_max(bmax.x, me.bmax.x + off.x), m_max(bmax.y, me.bmax.y + off.y), m_max(bmax.z, me.bmax.z + off.z));
  }
  if (m->kind == KIND_FRAMES) {
    float ex = 0, ey = 0;
    for (int i = 0; i < m->nn; i++) { Mesh &me = m->nodes[i].lod[0]; if (!me.ok) continue; ex = m_max(ex, me.bmax.x - me.bmin.x); ey = m_max(ey, me.bmax.y - me.bmin.y); }
    bmin = v3(-ex * 0.5f, 0, -ex * 0.5f); bmax = v3(ex * 0.5f, ey, ex * 0.5f);
  }
  if (bmin.x > bmax.x) { bmin = v3(-1, -1, -1); bmax = v3(1, 1, 1); }
  m->bmin = bmin; m->bmax = bmax;
  m->loaded = 1;
  sys_free(d);
  return 1;
}

// ------------------------------------------------------------------ asset catalog
struct Entry { char set[32]; char dir[400]; char file[32]; };
struct Catalog { Arr<Entry> e; char sets[16][32]; int nsets; };
static Catalog g_cat;


struct ListCtx { const char *dir; const char *set; Arr<char[64]> *names; };
static void cb_collect(void *u, const char *name, int is_dir) {
  Arr<char[64]> *a = (Arr<char[64]> *)u; char t[64]; str_cpy(t, name, 64);
  (void)is_dir;
  if (a->n == a->cap) { a->cap = a->cap ? a->cap * 2 : 32; a->p = (char(*)[64])sys_realloc(a->p, 64 * a->cap); }
  str_cpy(a->p[a->n++], t, 64);
}
static void sort_names(Arr<char[64]> *a) {
  for (int i = 1; i < a->n; i++) {
    char t[64]; str_cpy(t, a->p[i], 64); int j = i - 1;
    while (j >= 0 && str_icmp(a->p[j], t) > 0) { str_cpy(a->p[j + 1], a->p[j], 64); j--; }
    str_cpy(a->p[j + 1], t, 64);
  }
}
static void catalog_add_dir(const char *dir, const char *set) {
  Arr<char[64]> files = {};
  sys_list_dir(dir, ".M3D", cb_collect, &files);
  if (!files.n) { files.release(); return; }
  sort_names(&files);
  if (g_cat.nsets < 16) str_cpy(g_cat.sets[g_cat.nsets++], set, 32);
  for (int i = 0; i < files.n; i++) {
    Entry e; str_cpy(e.set, set, 32); str_cpy(e.dir, dir, 400); str_cpy(e.file, files.p[i], 32);
    g_cat.e.push(e);
  }
  files.release();
}
static void catalog_build(const char *gamedir) {
  char cars[512]; str_cpy(cars, gamedir, 512); str_cat(cars, "/CARS", 512);
  Arr<char[64]> sets = {};
  sys_list_dir(cars, "/", cb_collect, &sets);  // "/" = directories
  sort_names(&sets);
  for (int i = 0; i < sets.n; i++) {
    char d[512]; str_cpy(d, cars, 512); str_cat(d, "/", 512); str_cat(d, sets.p[i], 512);
    char s[32] = "CARS/"; str_cat(s, sets.p[i], 32);
    catalog_add_dir(d, s);
  }
  sets.release();
  char objs[512]; str_cpy(objs, gamedir, 512); str_cat(objs, "/OBJS", 512);
  Arr<char[64]> osets = {};
  sys_list_dir(objs, "/", cb_collect, &osets);
  sort_names(&osets);
  for (int i = 0; i < osets.n; i++) {
    char d[512]; str_cpy(d, objs, 512); str_cat(d, "/", 512); str_cat(d, osets.p[i], 512);
    char s[32] = "OBJS/"; str_cat(s, osets.p[i], 32);
    catalog_add_dir(d, s);
  }
  osets.release();
}

// ------------------------------------------------------------------ renderer
#define SHADOW_SAMPLE_GLSL \
 "uniform sampler2D uShadowMap; uniform int uShadowEnabled;\n" \
 "float shadowCoverage(vec4 p,float bias){if(uShadowEnabled==0)return 0.0;vec3 q=p.xyz/p.w*0.5+0.5;if(q.z<=0.0||q.z>=1.0||any(lessThan(q.xy,vec2(0.002)))||any(greaterThan(q.xy,vec2(0.998))))return 0.0;float s=0.0;vec2 step=1.0/vec2(textureSize(uShadowMap,0));for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)s+=q.z-bias>texture(uShadowMap,q.xy+vec2(x,y)*step).r?1.0:0.0;return s/9.0;}\n"
static const char *VS_MODEL =
  "#version 330 core\n"
  "layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec2 aUV;layout(location=3) in float aLampFacing;\n"
  "uniform float uDamage,uDamageStrength;uniform vec3 uDamageCenter,uDamageExtent,uDamageOffset;uniform mat4 uVP; uniform mat4 uM; uniform mat4 uShadowVP; uniform int uRetro; uniform vec2 uViewport;\n"
  "out vec3 vW; out vec3 vN; out vec3 vLocal;out vec3 vObject;flat out float vLampFacing;flat out vec3 vForward; out vec4 vShadow; out vec2 vUV; noperspective out vec2 vRetroUV;\n"
  "void main(){ vec3 pos=aPos;if(uDamage>=0.7){float d=uDamage>=1.0?1.0:uDamage<0.8?0.015+0.115*clamp((uDamage-0.7)/0.1,0.0,1.0):0.13+0.16*clamp((uDamage-0.8)/0.19,0.0,1.0);d=min(d*uDamageStrength,uDamage>=1.0?1.1:0.50);vec3 q=(aPos+uDamageOffset-uDamageCenter)/uDamageExtent;float dent=exp(-dot(q.xz-vec2(0.55,-0.45),q.xz-vec2(0.55,-0.45))*5.0)+exp(-dot(q.xz-vec2(-0.5,0.5),q.xz-vec2(-0.5,0.5))*7.0);pos.x+=(-q.x*0.12+sin(q.z*7.0+q.y*4.0)*0.10)*uDamageExtent.x*d;pos.y-=((q.y+1.0)*0.32+dent*0.20)*uDamageExtent.y*d;pos.z+=(-q.z*0.14+sin(q.x*6.0)*0.07)*uDamageExtent.z*d;}vec4 w = uM*vec4(pos,1.0); vW=w.xyz;vLocal=aPos;vObject=aPos+uDamageOffset;vLampFacing=aLampFacing;vForward=normalize(uM[2].xyz);vShadow=uShadowVP*w; vN=mat3(uM)*aNrm; vUV=aUV; vRetroUV=aUV; gl_Position=uVP*w; if(uRetro!=0 && gl_Position.w>0.0){ vec2 p=(gl_Position.xy/gl_Position.w*0.5+0.5)*uViewport; gl_Position.xy=(floor(p)/uViewport*2.0-1.0)*gl_Position.w; } }\n";
static const char *FS_MODEL =
  "#version 330 core\n"
  "in vec3 vW; in vec3 vN; in vec3 vLocal;in vec3 vObject;flat in float vLampFacing;flat in vec3 vForward; in vec4 vShadow; in vec2 vUV; noperspective in vec2 vRetroUV; uniform int uRetro;\n"
  "uniform sampler2D uTex; uniform int uMode; uniform vec3 uCam; uniform vec4 uColor; uniform float uLit; uniform float uGain; uniform float uGray;\n"
  "uniform vec3 uVisualStrength;uniform float uDamageStrength;uniform sampler2D uLampMask;uniform vec2 uLampEnds;uniform int uLampNode;uniform float uDamage;uniform int uLamps,uLampPass,uRearOnly;uniform float uBrake,uTrailWeight; uniform int uShadowPass; uniform int uOil; uniform vec4 uOilBounds; uniform vec3 uLightDir=vec3(0.45,0.85,0.35);\n"
  SHADOW_SAMPLE_GLSL
  "vec3 vehicleEmission(vec4 t,vec3 v){vec3 emission=vec3(0.0);if(uLamps!=0&&uMode==1&&uLampNode!=0){ivec2 size=textureSize(uLampMask,0);vec2 mask=texelFetch(uLampMask,ivec2(fract(vUV)*vec2(size)),0).rg;float red=mask.r*step(0.35,vLampFacing)*step(dot(vForward,v),0.0)*step(vObject.z,uLampEnds.x)*step(t.g*1.5,t.r)*step(t.b*1.4,t.r);float yellow=mask.g*step(0.0,dot(vForward,v))*step(uLampEnds.y,vObject.z)*step(t.b*1.5,min(t.r,t.g));emission=vec3(1.0,0.012,0.035)*red*mix(1.35,7.0,uBrake);if(uRearOnly==0)emission+=mix(vec3(1.0,0.72,0.14),vec3(1.0,0.94,0.78),clamp(mask.g,0.0,1.0))*yellow*4.5;}emission*=uVisualStrength.y;if(uDamage>=1.0)emission*=0.08;return emission;}\n"
  "out vec4 o;\n"
  "void main(){\n"
  "  if(uShadowPass!=0){vec4 t=(uMode==0||uMode==3)?uColor:texture(uTex,vUV);if(t.a<0.5)discard;o=vec4(0.0);return;}\n"
  "  if(uRetro!=0&&uLampPass==0&&uOil==0){ vec2 size=vec2(textureSize(uTex,0)); vec2 uv=floor(vRetroUV*size*65536.0)/65536.0/size; vec4 t=(uMode==0||uMode==3)?uColor:textureLod(uTex,uv,0.0); vec3 c=t.rgb*uGain;c+=vehicleEmission(t,normalize(uCam-vW));c*=1.0-min(0.95,0.78*uVisualStrength.x)*shadowCoverage(vShadow,0.00015);if(uDamage>=0.7)c*=mix(1.0,0.20,clamp((uDamage-0.7)/0.3,0.0,1.0)); c=mix(c,vec3(dot(c,vec3(0.3,0.59,0.11))),uGray); o=vec4(c,uMode==3?uColor.a:t.a); return; }\n"
  "  vec3 n = normalize(vN); vec3 v = normalize(uCam - vW);\n"
  "  if(dot(n,v) < 0.0) n = -n;\n"   // double sided: the original data has no consistent winding
  "  vec4 t = (uMode==0||uMode==3) ? uColor : texture(uTex, vUV);\n"
  "  if(t.a<=0.0)discard;vec3 emission=vehicleEmission(t,v);if(uLampPass!=0){if(max(emission.r,max(emission.g,emission.b))<0.01)discard;o=vec4(emission*uTrailWeight,1.0);return;}\n"
  "  if(t.a<=0.0)discard; if(uMode==2){ o = vec4(mix(t.rgb, vec3(dot(t.rgb, vec3(0.3,0.59,0.11))), uGray)*uColor.rgb*uGain*t.a, t.a); return; }\n"
  "  if(uOil!=0){vec2 q=(vLocal.xz-uOilBounds.xy)/uOilBounds.zw;float r=length(q);float edge=1.0-smoothstep(0.70,1.12,r);float swirl=sin(q.x*13.0+sin(q.y*9.0))*sin(q.y*17.0+q.x*5.0);float film=0.5+0.5*sin(swirl*4.0+q.x*5.0+q.y*7.0);float f=pow(1.0-abs(dot(n,v)),3.0);vec3 rainbow=0.5+0.5*sin(vec3(0.0,2.1,4.2)+film*8.0+f*4.0);vec3 oil=vec3(0.012,0.017,0.020)+rainbow*(0.016+0.045*f);float glint=pow(max(dot(reflect(-normalize(uLightDir),n),v),0.0),48.0);o=vec4(oil+glint*0.18,min(0.92,edge*(0.38+film*0.17+f*0.10)*uVisualStrength.z));return;}\n"
  "  vec3 L1 = normalize(uLightDir);float vis=1.0-min(0.95,0.78*uVisualStrength.x)*shadowCoverage(vShadow,0.00015);\n"
  "  vec3 L2 = normalize(vec3(-0.6, 0.25, -0.75));\n"
  "  float d1 = max(dot(n,L1),0.0), d2 = max(dot(n,L2),0.0);\n"
  "  float hemi = 0.5+0.5*n.y;\n"
  "  vec3 amb = mix(vec3(0.20,0.18,0.17), vec3(0.42,0.44,0.50), hemi);\n"
  "  vec3 h = normalize(L1+v); float s = pow(max(dot(n,h),0.0), 48.0)*0.45;\n"
  "  float rim = pow(1.0-max(dot(n,v),0.0), 3.0)*0.30;\n"
  "  vec3 light = amb + d1*vis*vec3(1.0,0.95,0.88)*0.85 + d2*vec3(0.35,0.45,0.65)*0.45;\n"
  "  vec3 c = t.rgb*mix(vec3(1.0), light, uLit) + (s*vis + rim*vec3(0.45,0.65,0.9))*uLit;\n"
  "  if(uDamage>=0.7){float d=uDamage>=1.0?1.0:0.04+0.20*clamp((uDamage-0.7)/0.29,0.0,1.0);float scars=0.5+0.5*sin(vLocal.x*7.0+sin(vLocal.z*9.0)*2.0);c*=mix(vec3(1.0),vec3(0.16,0.13,0.115),min(1.0,d*uDamageStrength*(0.65+scars*0.35)));}c+=emission; c = mix(c, vec3(dot(c, vec3(0.3,0.59,0.11))), uGray);\n  o = vec4(c*uGain, uMode==3 ? uColor.a : t.a);\n"
  "}\n";
static const char *VS_FULL =
  "#version 330 core\n"
  "out vec2 vUV; uniform vec4 uRect;\n"   // rect in NDC: x0,y0,x1,y1
  "void main(){ vec2 c = vec2(float(gl_VertexID&1), float((gl_VertexID>>1)&1));\n"
  "  vUV = vec2(c.x, 1.0-c.y); gl_Position = vec4(mix(uRect.xy, uRect.zw, c), 0.0, 1.0); }\n";
static const char *FS_BG =
  "#version 330 core\n"
  "in vec2 vUV; out vec4 o; uniform sampler2D uTex; uniform int uHasTex; uniform vec2 uScale; uniform float uDim;\n"
  "void main(){\n"
  "  vec2 uv = (vUV-0.5)*uScale+0.5;\n"
  "  vec3 gradient=mix(vec3(0.02,0.03,0.05),vec3(0.10,0.14,0.20),vUV.y); vec4 t=uHasTex==1?texture(uTex,uv):vec4(gradient,1.0); vec3 c=mix(gradient,t.rgb,t.a);\n"
  "  float vig = smoothstep(1.15, 0.25, length(vUV-0.5)*1.6);\n"
  "  o = vec4(c*uDim*vig, 1.0);\n"
  "}\n";
static const char *FS_HUD =
  "#version 330 core\n"
  "in vec2 vUV; out vec4 o; uniform sampler2D uTex; uniform vec4 uColor;\n"
  "void main(){ float a = texture(uTex, vUV).r; o = vec4(uColor.rgb, a*uColor.a); }\n";
static const char *VS_GROUND =
  "#version 330 core\n"
  "uniform mat4 uVP; uniform vec3 uCenter; uniform float uSize; out vec3 vW;\n"
  "void main(){ vec2 c = vec2(float(gl_VertexID&1), float((gl_VertexID>>1)&1))*2.0-1.0;\n"
  "  vW = uCenter + vec3(c.x*uSize, 0.0, c.y*uSize); gl_Position = uVP*vec4(vW,1.0); }\n";
static const char *FS_GROUND =
  "#version 330 core\n"
  "in vec3 vW; out vec4 o; uniform vec3 uCenter; uniform vec2 uExt; uniform float uSize;\n"
  "void main(){\n"
  "  vec2 d = vW.xz - uCenter.xz;\n"
  "  float fade = 1.0 - smoothstep(uSize*0.25, uSize*0.95, length(d));\n"
  "  vec2 g = vW.xz*0.5; vec2 fw = fwidth(g); vec2 gr = abs(fract(g-0.5)-0.5)/fw;\n"
  "  float line = 1.0 - min(min(gr.x, gr.y), 1.0);\n"
  "  float sh = length(d/uExt); float shadow = 1.0 - smoothstep(0.55, 1.25, sh);\n"
  "  vec3 base = vec3(0.05,0.06,0.075) + line*vec3(0.05,0.13,0.17);\n"
  "  float a = fade*(0.80) ;\n"
  "  o = vec4(base*(1.0-0.85*shadow), a);\n"
  "}\n";

static int g_realtime_shadows=1,g_modern_explosions=1,g_vehicle_lights=1,g_damage_effects=1,g_enhanced_oil=1,g_missile_effects=1;
static int g_video_upscale,g_video_filter;
static float g_effect_strength[6]={1,1,1,1,1,1};
static int g_shadow_pass,g_shadow_active,g_oil_render,g_lamp_pass;
static int g_tex_nearest;   // nearest-neighbour model textures (Retro Mode)
static int g_post_done;   // the game mode has already presented the picture (post-processing pipeline)
struct Renderer {
  GLuint prog_model, prog_bg, prog_hud, prog_ground, empty_vao;
  GLint um_visual_strength,um_damage_strength;
  GLint um_damage,um_damage_center,um_damage_extent,um_damage_offset;
  GLint um_lamps,um_lamp_pass,um_brake,um_rear_only,um_trail_weight,um_lamp_mask,um_lamp_ends,um_lamp_node;
  GLint um_shadow_vp,um_shadow_enabled,um_shadow_map,um_shadow_pass,um_light,um_oil,um_oil_bounds;
  GLint um_retro, um_viewport, um_vp, um_m, um_tex, um_mode, um_cam, um_color, um_lit, um_gain, um_gray;
  GLint ub_tex, ub_has, ub_scale, ub_dim, ub_rect;
  GLint uh_tex, uh_color, uh_rect;
  GLint ug_vp, ug_center, ug_size, ug_ext;
  GLuint bg_tex; char bg_texture_key[512]; int bg_w, bg_h;
  GLuint fbo, rb_color, rb_depth; int fbo_w, fbo_h, samples, max_samples, fbo_samples;
  GLuint hud_tex; int hud_w, hud_h; char hud_text[1024];
};
static Renderer R;

static void renderer_init(void) {
  R.prog_model = shader_build(VS_MODEL, FS_MODEL);
  R.um_damage=glGetUniformLocation(R.prog_model,"uDamage");R.um_damage_center=glGetUniformLocation(R.prog_model,"uDamageCenter");R.um_damage_extent=glGetUniformLocation(R.prog_model,"uDamageExtent");R.um_damage_offset=glGetUniformLocation(R.prog_model,"uDamageOffset");
  R.um_rear_only=glGetUniformLocation(R.prog_model,"uRearOnly");R.um_trail_weight=glGetUniformLocation(R.prog_model,"uTrailWeight");
  R.um_visual_strength=glGetUniformLocation(R.prog_model,"uVisualStrength");R.um_damage_strength=glGetUniformLocation(R.prog_model,"uDamageStrength");R.um_lamp_mask=glGetUniformLocation(R.prog_model,"uLampMask");R.um_lamp_ends=glGetUniformLocation(R.prog_model,"uLampEnds");R.um_lamp_node=glGetUniformLocation(R.prog_model,"uLampNode");R.um_lamps=glGetUniformLocation(R.prog_model,"uLamps");R.um_lamp_pass=glGetUniformLocation(R.prog_model,"uLampPass");R.um_brake=glGetUniformLocation(R.prog_model,"uBrake");
  R.um_shadow_vp=glGetUniformLocation(R.prog_model,"uShadowVP");R.um_shadow_enabled=glGetUniformLocation(R.prog_model,"uShadowEnabled");R.um_shadow_map=glGetUniformLocation(R.prog_model,"uShadowMap");R.um_shadow_pass=glGetUniformLocation(R.prog_model,"uShadowPass");R.um_light=glGetUniformLocation(R.prog_model,"uLightDir");R.um_oil=glGetUniformLocation(R.prog_model,"uOil");R.um_oil_bounds=glGetUniformLocation(R.prog_model,"uOilBounds");
  R.um_retro=glGetUniformLocation(R.prog_model,"uRetro");R.um_viewport=glGetUniformLocation(R.prog_model,"uViewport");
  R.um_vp = glGetUniformLocation(R.prog_model, "uVP"); R.um_m = glGetUniformLocation(R.prog_model, "uM");
  R.um_tex = glGetUniformLocation(R.prog_model, "uTex"); R.um_mode = glGetUniformLocation(R.prog_model, "uMode");
  R.um_cam = glGetUniformLocation(R.prog_model, "uCam"); R.um_color = glGetUniformLocation(R.prog_model, "uColor");
  R.um_lit = glGetUniformLocation(R.prog_model, "uLit"); R.um_gain = glGetUniformLocation(R.prog_model, "uGain"); R.um_gray = glGetUniformLocation(R.prog_model, "uGray");
  R.prog_bg = shader_build(VS_FULL, FS_BG);
  R.ub_tex = glGetUniformLocation(R.prog_bg, "uTex"); R.ub_has = glGetUniformLocation(R.prog_bg, "uHasTex");
  R.ub_scale = glGetUniformLocation(R.prog_bg, "uScale"); R.ub_dim = glGetUniformLocation(R.prog_bg, "uDim");
  R.ub_rect = glGetUniformLocation(R.prog_bg, "uRect");
  R.prog_hud = shader_build(VS_FULL, FS_HUD);
  R.uh_tex = glGetUniformLocation(R.prog_hud, "uTex"); R.uh_color = glGetUniformLocation(R.prog_hud, "uColor");
  R.uh_rect = glGetUniformLocation(R.prog_hud, "uRect");
  R.prog_ground = shader_build(VS_GROUND, FS_GROUND);
  R.ug_vp = glGetUniformLocation(R.prog_ground, "uVP"); R.ug_center = glGetUniformLocation(R.prog_ground, "uCenter");
  R.ug_size = glGetUniformLocation(R.prog_ground, "uSize"); R.ug_ext = glGetUniformLocation(R.prog_ground, "uExt");
  glGenVertexArrays(1, &R.empty_vao);
  GLint ms = 0; glGetIntegerv(GL_MAX_SAMPLES, &ms);
  R.max_samples = ms; R.samples = ms >= 4 ? 4 : 0;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &g_max_aniso);
  glGetError();
  if (g_max_aniso < 1) g_max_aniso = 1;
  // menu background of the original game
  char p[512]; str_cpy(p, g_gamedir, 512); str_cat(p, "/DATA640/FOND.HLZ", 512);
  Image img; mem_zero(&img, sizeof img);
  texture_key(R.bg_texture_key,p,0);str_cat(R.bg_texture_key,".png",512);
  if (image_load_hlz(p, &img)) { R.bg_tex = tex_upload(img.rgba, img.w, img.h, 0, 1); R.bg_w = img.w; R.bg_h = img.h; sys_free(img.rgba); }
}

static void fbo_ensure(int w, int h) {
  if (!R.samples) return;
  if (R.fbo && R.fbo_w == w && R.fbo_h == h && R.fbo_samples == R.samples) return;
  if (R.fbo) { glDeleteFramebuffers(1, &R.fbo); glDeleteRenderbuffers(1, &R.rb_color); glDeleteRenderbuffers(1, &R.rb_depth); }
  glGenFramebuffers(1, &R.fbo); glBindFramebuffer(GL_FRAMEBUFFER, R.fbo);
  glGenRenderbuffers(1, &R.rb_color); glBindRenderbuffer(GL_RENDERBUFFER, R.rb_color);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, R.samples, GL_RGBA8, w, h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, R.rb_color);
  glGenRenderbuffers(1, &R.rb_depth); glBindRenderbuffer(GL_RENDERBUFFER, R.rb_depth);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, R.samples, GL_DEPTH_COMPONENT24, w, h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, R.rb_depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    sys_log("MSAA framebuffer incomplete - disabling MSAA");
    glBindFramebuffer(GL_FRAMEBUFFER, 0); R.samples = 0; return;
  }
  R.fbo_w = w; R.fbo_h = h; R.fbo_samples = R.samples;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void hud_set(const char *text) {
  if (!str_icmp(text, R.hud_text) && (R.hud_tex || !text[0])) return;
  str_cpy(R.hud_text, text, sizeof R.hud_text);
  u8 *rgba; int w, h;
  if (!text[0] || !sys_render_text(text, 15, &rgba, &w, &h)) { R.hud_w = 0; return; }
  if (!R.hud_tex) glGenTextures(1, &R.hud_tex);
  glBindTexture(GL_TEXTURE_2D, R.hud_tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  sys_free(rgba);
  R.hud_w = w; R.hud_h = h;
}

// ------------------------------------------------------------------ app state
enum { MODE_VIEWER, MODE_TRACK, MODE_FMV, MODE_VIEWER_FORCED };
struct App {
  int graphics_test,effect_age_ms,test_damage,no_light_trails;
  int mode; char status[256]; int have_mexe; int start_mode, start_track, start_frame; int start_opp_ahead; float demo_time, demo_t; char start_screen[64]; int start_car, start_laps; int roaddbg, audiolog, camoff, dumpvideo; int yoff, yoff_set; float quit_after; int warptest; int debug_console, debug_start_console,export_textures; int retro_scale, fxaa_test; int focal_pct, halfw_pct; int shot_fps, objtest, sim_revived, trace;
  Model model; int cur; int lod; int wire; int spin_on; int autorot; int show_bg; int show_help; int show_ground;
  float yaw, pitch, dist, wheel_angle, time, frame_time;
  int shot_pending; char shot_path[400],native_shot_path[400]; int shot_exit; int frames_rendered; int shot_after;
  char title[256];
  float fps_acc; int fps_n; float fps;
};
static App A;

static void apply_filter_to_model(void) {
  for (int i = 0; i < A.model.ntex; i++) { glBindTexture(GL_TEXTURE_2D, A.model.tex[i]); tex_params(g_smooth_filter); }
}

static void select_model(int idx) {
  if (!g_cat.e.n) return;
  idx = ((idx % g_cat.e.n) + g_cat.e.n) % g_cat.e.n;
  if (A.model.loaded) model_free(&A.model);
  A.cur = idx;
  Entry &e = g_cat.e.p[idx];
  if (!model_load(&A.model, e.dir, e.file)) { char m[600] = "failed to load "; str_cat(m, e.file, 600); sys_log(m); }
  V3 ext = A.model.bmax - A.model.bmin;
  float r = m_sqrt(dot(ext, ext)) * 0.5f;
  A.dist = r * 2.6f + 0.5f;
  while (A.lod > 0 && !A.model.nodes[0].lod[A.lod].ok) A.lod--;
}

static void set_step(int dir) {
  if (!g_cat.e.n) return;
  const char *cs = g_cat.e.p[A.cur].set; const char *cf = g_cat.e.p[A.cur].file;
  int si = 0; for (int i = 0; i < g_cat.nsets; i++) if (!str_icmp(g_cat.sets[i], cs)) si = i;
  si = ((si + dir) % g_cat.nsets + g_cat.nsets) % g_cat.nsets;
  int first = -1, same = -1;
  for (int i = 0; i < g_cat.e.n; i++)
    if (!str_icmp(g_cat.e.p[i].set, g_cat.sets[si])) { if (first < 0) first = i; if (same < 0 && !str_icmp(g_cat.e.p[i].file, cf)) same = i; }
  select_model(same >= 0 ? same : first);
}

static void app_start_media(void);
int app_init(const char *gamedir, int argc, char **argv) {
  mem_zero(&A, sizeof A); mem_zero(&R, sizeof R); mem_zero(&g_cat, sizeof g_cat);
  str_cpy(g_gamedir, gamedir, 400);g_model_reserved_ready=0;
  A.spin_on = 1; A.autorot = 1; A.show_bg = 1; A.show_help = 1; A.show_ground = 1;
  A.yaw = 0.75f; A.pitch = 0.32f; A.start_car = -1;
  catalog_build(gamedir);
  if (!g_cat.e.n) { sys_log("No MegaRace 2 models found (expected CARS/<SET>/*.M3D)."); return 0; }
  const GLubyte *ver = glGetString(GL_VERSION), *ren = glGetString(GL_RENDERER);
  char m[512] = "OpenGL "; str_cat(m, ver ? (const char *)ver : "?", 512); str_cat(m, " / ", 512); str_cat(m, ren ? (const char *)ren : "?", 512);
  sys_log(m);
  renderer_init();
  int start = 0;
  const char *want_set = 0;
  for (int i = 1; i < argc; i++) {
    if (!str_icmp(argv[i], "--viewer")) { A.start_mode = MODE_VIEWER_FORCED; }
    else if (!str_icmp(argv[i], "--screen") && i + 1 < argc) { str_cpy(A.start_screen, argv[++i], 64); }
    else if (!str_icmp(argv[i], "--roaddbg")) { A.roaddbg = 1; }
    else if (!str_icmp(argv[i], "--roaddbg2")) { A.roaddbg = 2; }
    else if (!str_icmp(argv[i], "--camoff") && i + 1 < argc) { A.camoff = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--warptest") && i + 1 < argc) { A.warptest = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--retro-scale") && i + 1 < argc) { A.retro_scale = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--fxaa")) { A.fxaa_test = 1; }
    else if (!str_icmp(argv[i], "--focal") && i + 1 < argc) { A.focal_pct = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--halfw") && i + 1 < argc) { A.halfw_pct = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "-debug") || !str_icmp(argv[i], "--debug")) { A.debug_console = A.debug_start_console = 1; }
    else if (!str_icmp(argv[i], "--dumpvideo") && i + 1 < argc) { A.dumpvideo = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--yoff") && i + 1 < argc) { A.yoff = (int)str_toi(argv[++i]); A.yoff_set = 1; }
    else if (!str_icmp(argv[i], "--audiolog")) { A.audiolog = 1; }
    else if (!str_icmp(argv[i], "--trace")) { A.trace = 1; }   // per-update state trace: revived_config/openmr2_trace.csv
    else if (!str_icmp(argv[i], "--simrevived")) { A.sim_revived = 1; }   // test: subdivided Revived physics
    else if (!str_icmp(argv[i], "--no-light-trails"))A.no_light_trails=1;
    else if (!str_icmp(argv[i], "--graphics-test")&&i+1<argc)A.graphics_test=(int)str_toi(argv[++i]);
    else if (!str_icmp(argv[i], "--test-damage")&&i+1<argc)A.test_damage=(int)str_toi(argv[++i]);
    else if (!str_icmp(argv[i], "--effect-age-ms")&&i+1<argc)A.effect_age_ms=(int)str_toi(argv[++i]);
    else if (!str_icmp(argv[i], "--objtest")) { A.objtest = 1; }            // player alone, standing, steerable
    else if (!str_icmp(argv[i], "--objtest-drive")) { A.objtest = 2; }      // player alone, full throttle straight ahead into the objects
    else if (!str_icmp(argv[i], "--shotfps") && i + 1 < argc) { A.shot_fps = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--dump-textures"))A.export_textures=1;
    else if (!str_icmp(argv[i], "--quit") && i + 1 < argc) { A.quit_after = (float)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--laps") && i + 1 < argc) { A.start_laps = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--car") && i + 1 < argc) { A.start_car = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--model") && i + 1 < argc) { A.start_mode = MODE_VIEWER_FORCED; const char *w = argv[++i]; for (int k = 0; k < g_cat.e.n; k++) if (str_icontains(g_cat.e.p[k].file, w) && (!want_set || str_icontains(g_cat.e.p[k].set, want_set))) { start = k; break; } }
    else if (!str_icmp(argv[i], "--set") && i + 1 < argc) { want_set = argv[++i]; for (int k = 0; k < g_cat.e.n; k++) if (str_icontains(g_cat.e.p[k].set, want_set)) { start = k; break; } }
    else if (!str_icmp(argv[i], "--shot") && i + 1 < argc) { str_cpy(A.shot_path, argv[++i], 400); A.shot_pending = 1; A.shot_exit = 1; A.autorot = 0; }
    else if (!str_icmp(argv[i], "--shot-native") && i + 1 < argc) str_cpy(A.native_shot_path,argv[++i],sizeof A.native_shot_path);
    else if (!str_icmp(argv[i], "--shot-after") && i+1<argc) { A.shot_after=(int)str_toi(argv[++i]);if(A.shot_after<2)A.shot_after=2;if(A.shot_after>60000)A.shot_after=60000; }
    else if (!str_icmp(argv[i], "--yaw") && i + 1 < argc) { A.yaw = str_toi(argv[++i]) * PI / 180; }
    else if (!str_icmp(argv[i], "--pitch") && i + 1 < argc) { A.pitch = str_toi(argv[++i]) * PI / 180; }
    else if (!str_icmp(argv[i], "--lod") && i + 1 < argc) { A.lod = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--wire")) { A.wire = 1; }
    else if (!str_icmp(argv[i], "--no-remastered-ui")) { g_ui_assets_allowed=g_revived_ui=0; }
    else if (!str_icmp(argv[i], "--retro")) { g_smooth_filter = 0; }
    else if (!str_icmp(argv[i], "--nohelp")) { A.show_help = 0; }
    else if (!str_icmp(argv[i], "--time") && i + 1 < argc) { A.time = str_toi(argv[++i]) / 1000.0f; }
    else if (!str_icmp(argv[i], "--track") && i + 1 < argc) { A.start_mode = MODE_TRACK; A.start_track = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--demo") && i + 1 < argc) { A.demo_time = (float)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--frame") && i + 1 < argc) { A.start_frame = (int)str_toi(argv[++i]); }
    else if (!str_icmp(argv[i], "--fmv") && i + 1 < argc) { A.start_mode = MODE_FMV; A.start_frame = (int)str_toi(argv[++i]); }
  }
  select_model(start);
  app_start_media();
  return 1;
}

static void write_bmp(const char *path, int w, int h) {
  int stride = (w * 3 + 3) & ~3;
  size_t sz = 54 + (size_t)stride * h;
  u8 *buf = (u8 *)sys_alloc(sz);
  u8 *px = (u8 *)sys_alloc((size_t)w * h * 4);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
  buf[0] = 'B'; buf[1] = 'M';
  u32 v[13] = {(u32)sz, 0, 54, 40, (u32)w, (u32)h, 1 | (24 << 16), 0, (u32)(stride * h), 2835, 2835, 0, 0};
  mem_cpy(buf + 2, v, sizeof v);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const u8 *s = px + ((size_t)y * w + x) * 4; u8 *d = buf + 54 + (size_t)y * stride + x * 3;
      d[0] = s[2]; d[1] = s[1]; d[2] = s[0];
    }
  sys_write_file(path, buf, sz);
  sys_free(px); sys_free(buf);
  char m[500] = "screenshot saved: "; str_cat(m, path, 500); sys_log(m);
}
static void shortcut_notice(const char *text);
static void capture_notice(const char *text);
static void capture_filename(char *out,size_t cap,const char *directory,const char *extension){
  char base[400]="";str_cpy(base,directory,sizeof base);str_cat(base,"/openmr2_",sizeof base);str_int(base,sizeof base,(long)((long long)(sys_time()*1000)%2000000000));
  for(int suffix=0;;suffix++){str_cpy(out,base,cap);if(suffix){str_cat(out,"_",cap);str_int(out,cap,suffix);}str_cat(out,extension,cap);void *existing=sys_fopen(out);if(!existing)return;sys_fclose(existing);}
}
static void write_screenshot_jpg(const char *path,int w,int h){
  u8 *pixels=(u8 *)sys_alloc((size_t)w*h*4),*row=(u8 *)sys_alloc((size_t)w*4);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<h/2;y++){u8 *a=pixels+(size_t)y*w*4,*b=pixels+(size_t)(h-1-y)*w*4;mem_cpy(row,a,w*4);mem_cpy(a,b,w*4);mem_cpy(b,row,w*4);}
  int ok=jpeg_write(path,pixels,w,h);sys_free(pixels);sys_free(row);char message[600]="";str_cpy(message,ok?"Screenshot saved (JPEG quality 98): ":"Screenshot could not be saved: ",sizeof message);str_cat(message,path,sizeof message);sys_log(message);capture_notice(ok?"Screenshot saved":"Screenshot failed");
}

static void draw_mesh(const Model &m, const Mesh &me, const M4 &model_mtx, int pass) {
  if (!me.ok) return;
  float center=(m.bmin.z+m.bmax.z)*.5f,extent=(m.bmax.z-m.bmin.z)*.5f;glUniform2f(R.um_lamp_ends,center-extent*.20f,center+extent*.20f);
  glUniform1i(R.um_retro,g_tex_nearest);
  if(g_tex_nearest){GLint viewport[4]={0,0,1,1};glGetIntegerv(0x0BA2,viewport);glUniform2f(R.um_viewport,(float)(viewport[2]>0?viewport[2]:1),(float)(viewport[3]>0?viewport[3]:1));}
  glUniformMatrix4fv(R.um_m, 1, GL_FALSE, model_mtx.m);
  glBindVertexArray(me.vao);
  for (int b = 0; b < me.nb; b++) {
    const Mat &mt = m.mats[me.b[b].mat];
    if(g_lamp_pass&&mt.type!=MT_TEX)continue;
    if(g_shadow_pass&&(mt.type==MT_SHADOW||mt.type==MT_TRANS||mt.type==MT_ADD))continue;
    if(g_shadow_active&&mt.type==MT_SHADOW&&!g_oil_render)continue;
    int order=g_oil_render?1:mat_order(mt.type);
    if (order != pass) continue;
    if(g_oil_render){glUniform1i(R.um_mode,3);glUniform4f(R.um_color,0,0,0,1);glDrawArrays(GL_TRIANGLES,me.b[b].first,me.b[b].count);continue;}
    if (mt.type == MT_SHADOW) { glUniform1i(R.um_mode, 3); glUniform4f(R.um_color, 0, 0, 0, 0.62f); }
    else if (mt.type == MT_FLAT || mt.tex < 0) {
      if (mt.type == MT_TRANS) { glUniform1i(R.um_mode, 3); glUniform4f(R.um_color, 0.02f, 0.02f, 0.03f, 0.75f); }
      else { glUniform1i(R.um_mode, 0); glUniform4f(R.um_color, 0.16f, 0.17f, 0.19f, 1); }
    } else {
      if (m.texmissing[mt.tex] && mt.type == MT_ADD) continue;  // effect sprite sheet not shipped with this folder
      glActiveTexture(GL_TEXTURE0+2);glBindTexture(GL_TEXTURE_2D,m.lamp_mask[mt.tex]);if(m.lamp_mask[mt.tex]){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}glActiveTexture(GL_TEXTURE0);
      TextureImage *replacement=texture_custom(m.texture_keys[mt.tex]);
      glBindTexture(GL_TEXTURE_2D,replacement?texture_gpu(replacement,1,0):m.tex[mt.tex]);
      {   // Retro Mode: unfiltered textures as in the DOS software renderer
        tex_params(!g_tex_nearest && g_smooth_filter);
      }
      if (mt.type == MT_ADD) { glUniform1i(R.um_mode, 2); glUniform4f(R.um_color, 1, 1, 1, 1); }
      else { glUniform1i(R.um_mode, 1); glUniform4f(R.um_color, 1, 1, 1, 1); }
    }
    glDrawArrays(GL_TRIANGLES, me.b[b].first, me.b[b].count);
  }
}

static void draw_model(const M4 &vp, V3 eye) {
  Model &m = A.model;
  glUseProgram(R.prog_model);
  glUniformMatrix4fv(R.um_vp, 1, GL_FALSE, vp.m);
  glUniform3f(R.um_cam, eye.x, eye.y, eye.z);
  glUniform1i(R.um_tex, 0);
  glUniform1f(R.um_lit, 1.0f); glUniform1f(R.um_gain, 1.0f);
  glActiveTexture(GL_TEXTURE0);
  V3 c = (m.bmin + m.bmax) * 0.5f;
  M4 center = m4_trans(v3(-c.x, -m.bmin.y, -c.z));  // model sits on the ground plane y=0
  int lod = A.lod;
  for (int pass = 0; pass < 3; pass++) {
    if(pass==0){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
    if (pass == 1) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); }
    if (pass == 2) { glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE); glDepthMask(GL_FALSE); }
    if (m.kind == KIND_FRAMES) {
      int f = (int)(A.time * 15.0f) % (m.nn ? m.nn : 1);
      int l = lod; while (l > 0 && !m.nodes[f].lod[l].ok) l--;
      const Mesh &fm = m.nodes[f].lod[l];
      V3 fc = (fm.bmin + fm.bmax) * 0.5f;
      V3 ext = m.bmax - m.bmin;
      draw_mesh(m, fm, m4_trans(v3(-fc.x, ext.y * 0.5f - fc.y, -fc.z)), pass);
    } else {
      for (int i = 0; i < m.nn; i++) {
        const Node &nd = m.nodes[i];
        int l = lod; while (l > 0 && !nd.lod[l].ok) l--;
        if (!nd.lod[l].ok) continue;
        if (i > 0 && !m.nodes[i].lod[lod].ok && lod > 0) continue;  // node absent on this LOD (e.g. wheels on far LOD)
        V3 off = v3(0, 0, 0);
        for (int j = i, guard = 0; j >= 0 && guard < 32; j = m.nodes[j].parent, guard++) off = off + m.nodes[j].pos;
        M4 mm = m4_mul(center, m4_trans(off));
        if (nd.spin && A.spin_on) mm = m4_mul(mm, m4_rotx(A.wheel_angle));
        draw_mesh(m, nd.lod[l], mm, pass);
      }
    }
  }
  glDisable(GL_BLEND); glDepthMask(GL_TRUE);
  glBindVertexArray(0);
}

static void hud_draw(int W, int H, const char *t,int right=0) {
  glViewport(0, 0, W, H);
  {
    hud_set(t);
    if (R.hud_w) {
      glUseProgram(R.prog_hud); glBindVertexArray(R.empty_vao);
      glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDisable(GL_DEPTH_TEST);
      glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, R.hud_tex); glUniform1i(R.uh_tex, 0);
      float x0 = -1 + 2.0f * (right?W-14-R.hud_w:14) / W, y1 = 1 - 2.0f * 12 / H;
      float x1 = x0 + 2.0f * R.hud_w / W, y0 = y1 - 2.0f * R.hud_h / H;
      glUniform4f(R.uh_color, 0, 0, 0, 0.8f); glUniform4f(R.uh_rect, x0 + 2.0f / W, y0 - 2.0f / H, x1 + 2.0f / W, y1 - 2.0f / H);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glUniform4f(R.uh_color, 0.85f, 0.95f, 1.0f, 1.0f); glUniform4f(R.uh_rect, x0, y0, x1, y1);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glDisable(GL_BLEND);
    }
  }

}

static void texture_dump_overlay_draw(int W,int H);
static int texture_dump_frame_tick();
static char g_shortcut_notice[600];static double g_shortcut_until;static int g_shortcut_right;
static void capture_frame(int W,int H);
static int frame_finish(int W, int H) {
  if(!g_post_done)texture_dump_overlay_draw(W,H);
  if (R.samples && !g_post_done) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, R.fbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  g_post_done = 0;
  capture_frame(W,H);
  A.frames_rendered++;
  if (A.shot_pending && A.frames_rendered >= (A.shot_after>0?A.shot_after:2)) {
    char p[400];
    if (A.shot_path[0]) str_cpy(p, A.shot_path, 400);
    else capture_filename(p,sizeof p,"revived_config/screenshots",".jpg");
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    if(!A.shot_path[0])write_screenshot_jpg(p,W,H);else write_bmp(p, W, H);
    A.shot_pending = 0;
    if (A.shot_exit) return 0;
  }
  if(!A.shot_exit&&sys_time()<g_shortcut_until)hud_draw(W,H,g_shortcut_notice,g_shortcut_right);
  return 1;
}

static int viewer_frame(int W, int H, float dt, const Input *in) {
  // ---- input
  if (in->pressed[K_RIGHT]) select_model(A.cur + 1);
  if (in->pressed[K_LEFT]) select_model(A.cur - 1);
  if (in->pressed[K_DOWN] || in->pressed[K_PGDN]) set_step(1);
  if (in->pressed[K_UP] || in->pressed[K_PGUP]) set_step(-1);
  if (in->pressed[K_L]) { A.lod = (A.lod + 1) % 4; if (!A.model.nodes[0].lod[A.lod].ok) A.lod = 0; }
  if (in->pressed[K_W]) A.wire ^= 1;
  if (in->pressed[K_R]) A.spin_on ^= 1;
  if (in->pressed[K_SPACE]) A.autorot ^= 1;
  if (in->pressed[K_B]) A.show_bg ^= 1;
  if (in->pressed[K_G]) A.show_ground ^= 1;
  if (in->pressed[K_H]) A.show_help ^= 1;
  // Filtering is controlled globally by F2.
  if (in->pressed[K_HOME]) { A.yaw = 0.75f; A.pitch = 0.32f; A.start_car = -1; select_model(A.cur); }
  if (in->lmb) { A.yaw -= in->mouse_dx * 0.008f; A.pitch += in->mouse_dy * 0.008f; A.autorot = 0; }
  if (in->wheel != 0) A.dist *= (in->wheel > 0) ? 0.88f : 1.0f / 0.88f;
  if (in->down[K_PLUS]) A.dist *= 1.0f - dt * 1.5f;
  if (in->down[K_MINUS]) A.dist *= 1.0f + dt * 1.5f;
  A.pitch = m_clamp(A.pitch, -0.2f, 1.45f);
  if (A.dist < 0.3f) A.dist = 0.3f;
  if (A.dist > 200) A.dist = 200;
  if (A.autorot) A.yaw += dt * 0.35f;
  if (!A.shot_exit) A.wheel_angle -= dt * 9.0f; else A.wheel_angle = 0;

  // ---- camera
  Model &m = A.model;
  V3 ext = m.bmax - m.bmin;
  V3 target = v3(0, ext.y * 0.45f, 0);
  V3 eye = target + v3(m_sin(A.yaw) * m_cos(A.pitch), m_sin(A.pitch), m_cos(A.yaw) * m_cos(A.pitch)) * A.dist;
  float aspect = (float)W / (float)H;
  M4 proj = m4_persp(40.0f * PI / 180.0f, aspect, A.dist * 0.02f, A.dist * 20.0f + 100);
  M4 view = m4_look(eye, target, v3(0, 1, 0));
  M4 vp = m4_mul(proj, view);

  // ---- render
  fbo_ensure(W, H);
  glBindFramebuffer(GL_FRAMEBUFFER, R.samples ? R.fbo : 0);
  glViewport(0, 0, W, H);
  glClearColor(0.02f, 0.025f, 0.035f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_CULL_FACE);

  // background: the game's own menu backdrop, darkened
  glDisable(GL_DEPTH_TEST);
  glUseProgram(R.prog_bg); glBindVertexArray(R.empty_vao);
  glUniform4f(R.ub_rect, -1, -1, 1, 1);
  TextureImage *background_replacement=texture_custom(R.bg_texture_key);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,background_replacement?texture_gpu(background_replacement,0,1):R.bg_tex);
  glUniform1i(R.ub_tex, 0); glUniform1i(R.ub_has, (A.show_bg && R.bg_tex) ? 1 : 0);
  {
    float ia = (float)R.bg_w / (R.bg_h ? R.bg_h : 1);
    float sx = 1, sy = 1; if (aspect > ia) sy = ia / aspect; else sx = aspect / ia;
    glUniform2f(R.ub_scale, sx, sy);
  }
  glUniform1f(R.ub_dim, 0.38f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


  // ground with grid and contact shadow
  if (A.show_ground) {
    glUseProgram(R.prog_ground); glBindVertexArray(R.empty_vao);
    glUniformMatrix4fv(R.ug_vp, 1, GL_FALSE, vp.m);
    float size = m_max(ext.x, ext.z) * 3.0f + 4.0f;
    glUniform3f(R.ug_center, 0, -0.002f, 0); glUniform1f(R.ug_size, size);
    glUniform2f(R.ug_ext, ext.x * 0.62f + 0.05f, ext.z * 0.62f + 0.05f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); glEnable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
  }

  glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
  if (A.wire) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  draw_model(vp, eye);
  if (A.wire) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  // HUD
  {
    Entry &e = g_cat.e.p[A.cur];
    char t[1024]; t[0] = 0;
    str_cat(t, e.set, 1024); str_cat(t, "/", 1024); str_cat(t, e.file, 1024);
    str_cat(t, "   [", 1024); str_int(t, 1024, A.cur + 1); str_cat(t, "/", 1024); str_int(t, 1024, g_cat.e.n); str_cat(t, "]", 1024);
    str_cat(t, "\nLOD ", 1024); str_int(t, 1024, A.lod); str_cat(t, "  |  ", 1024); str_int(t, 1024, m.tris[A.lod] ? m.tris[A.lod] : m.tris[0]);
    str_cat(t, ui_translate(" Dreiecke  |  "), 1024); str_cat(t, ui_translate(g_smooth_filter ? "Filter: anisotrop+Mipmaps" : "Filter: Retro (nearest)"), 1024);
    str_cat(t, "  |  MSAA ", 1024); str_int(t, 1024, R.samples); str_cat(t, "x", 1024);
    if (A.show_help)
      str_cat(t, "\n\nDrag: rotate   Wheel/+/-: zoom   Left/Right: model   Up/Down: set\n"
                 "L: LOD   W: wireframe   F2: texture filter   R: wheels   Space: rotation\n"
                 "B: background   G: ground   F12: screenshot   H: help   Esc: Extras", 1024);
    hud_draw(W, H, t);
  }
  if (!frame_finish(W, H)) return 0;
  // title
  {
    Entry &e = g_cat.e.p[A.cur];
    A.title[0] = 0; str_cat(A.title, OPENMR2_FULLTITLE "  -  ", 256); str_cat(A.title, e.set, 256); str_cat(A.title, "/", 256); str_cat(A.title, e.file, 256);
    str_cat(A.title, "  -  ", 256); str_int(A.title, 256, (long)(A.fps + 0.5f)); str_cat(A.title, " fps", 256);
  }
  return 1;
}


static int game_mode_frame(int W, int H, float dt, const Input *in);
static int fmv_frame(int W, int H, float dt, const Input *in);
static void mode_set(int mode);
static void pacing_frame_start(void);
static void global_shortcuts(int W,int H,const Input *in);

int app_frame(int W, int H, float dt, const Input *in) {
  if(texture_dump_frame_tick())return 0;
  if (W < 8 || H < 8) return 1;
  if (dt > 0.1f) dt = 0.1f;
  if (!A.shot_exit) A.time += dt;
  A.fps_acc += dt; A.fps_n++;
  if (A.fps_acc > 0.5f) { A.fps = A.fps_n / A.fps_acc; A.fps_acc = 0; A.fps_n = 0; }
  if (in->pressed[K_F12]) { A.shot_pending = 1; A.shot_path[0] = 0; }
  global_shortcuts(W,H,in);
  if (A.mode == MODE_TRACK) { pacing_frame_start(); return game_mode_frame(W, H, dt, in); }
  if (in->pressed[K_ESC]) { if (iso_is_open() && A.have_mexe) { mode_set(MODE_TRACK); return 1; } return 0; }
  if (A.mode == MODE_FMV) return fmv_frame(W, H, dt, in);
  return viewer_frame(W, H, dt, in);
}

const char *app_title(void) { return A.title; }
int app_wants_screenshot_exit(void) { return A.shot_exit; }

#include "modes.inc"

// ------------------------------------------------------------------ GL loader
#define GL_DEFINE(ret, name, args) PFN_##name name;
GL_FUNCS(GL_DEFINE)
#undef GL_DEFINE
const char *gl_load(void *(*getproc)(const char *name)) {
#define GL_LOAD(ret, name, args) name = (PFN_##name)getproc(#name); if (!name) return #name;
  GL_FUNCS(GL_LOAD)
#undef GL_LOAD
  return 0;
}
