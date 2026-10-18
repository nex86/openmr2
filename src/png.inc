// PNG only, memory I/O and the game's allocator. Vendored stb licenses remain in headers.
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STBI_MAX_DIMENSIONS 8192
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(n) sys_alloc(n)
#define STBI_REALLOC(p,n) sys_realloc(p,n)
#define STBI_FREE(p) sys_free(p)
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#define STBIW_MALLOC(n) sys_alloc(n)
#define STBIW_REALLOC(p,n) sys_realloc(p,n)
#define STBIW_FREE(p) sys_free(p)
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

static u8 *png_read(const char *path,int *w,int *h){
  size_t n=0;u8 *d=sys_read_file(path,&n);if(!d)return 0;
  int channels=0,x=0,y=0;u8 *out=0;
  if(n<67108864 && stbi_info_from_memory(d,(int)n,&x,&y,&channels) && x>0 && y>0 && (size_t)x*y<=16777216)
    out=stbi_load_from_memory(d,(int)n,w,h,&channels,4);
  sys_free(d);return out;
}
static int png_write(const char *path,const u8 *pixels,int w,int h){
  if(w<=0 || h<=0 || (size_t)w*h>16777216)return 0;
  int n=0;u8 *png=stbi_write_png_to_mem(pixels,w*4,w,h,4,&n);if(!png)return 0;
  int ok=sys_write_file(path,png,n);sys_free(png);return ok;
}
struct JpegOutput {u8 *data;size_t used,cap;};
static void jpeg_output(void *context,void *data,int count){
  JpegOutput &o=*(JpegOutput *)context;size_t n=o.used+count;if(n>o.cap){o.cap=n*2+4096;o.data=(u8 *)sys_realloc(o.data,o.cap);}mem_cpy(o.data+o.used,data,count);o.used=n;
}
static int jpeg_write(const char *path,const u8 *rgba,int w,int h){
  JpegOutput output={};int ok=stbi_write_jpg_to_func(jpeg_output,&output,w,h,4,rgba,98);if(ok)ok=sys_write_file(path,output.data,output.used);sys_free(output.data);return ok;
}
