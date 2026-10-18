// Ordered layers retain high-resolution replacement pixels, UI/text ordering and straight alpha.
#include "remastered_ui_data.h"
static TextureImage g_remastered_ui_cache[REMASTERED_UI_ASSETS];
static TextureImage *texture_remastered_ui(const char *key){
  if(!g_revived_ui||!key[0])return 0;
  for(unsigned i=0;i<sizeof REMASTERED_UI_ENTRIES/sizeof *REMASTERED_UI_ENTRIES;i++){
    const RemasteredUIEntry &e=REMASTERED_UI_ENTRIES[i];if(str_icmp(e.key,key))continue;
    TextureImage &t=g_remastered_ui_cache[e.asset];
    if(!t.pixels){int channels=0;t.pixels=stbi_load_from_memory(e.png,e.bytes,&t.w,&t.h,&channels,4);if(!t.pixels)return 0;str_cpy(t.key,key,sizeof t.key);texture_log("Loaded built-in Revived UI",key,t.w,t.h,t.pixels);}
    return &t;
  }
  return 0;
}
struct TextureLayer { GLuint texture; float x,y,w,h; float u,v,uw,vh; uint32_t tint; int slice; int sdf; };
struct CanvasSlice { uint32_t *pixels; GLuint texture; };
static Arr<TextureLayer> g_texture_layers={};
static Arr<CanvasSlice> g_canvas_slices={};
static int g_canvas_slice_used;
static void texture_canvas_reset(){g_texture_layers.n=0;g_canvas_slice_used=0;}
static void texture_canvas_snapshot(){
  if(!g_cv_dirty)return;
  int any=0;for(int i=0;i<CW*CH;i++)if(g_cv[i]>>24){any=1;break;}
  if(any){
    if(g_canvas_slice_used==g_canvas_slices.n){CanvasSlice s={};s.pixels=(uint32_t *)sys_alloc(CW*CH*4);g_canvas_slices.push(s);}
    CanvasSlice &s=g_canvas_slices.p[g_canvas_slice_used++];mem_cpy(s.pixels,g_cv,CW*CH*4);
    // Uploaded at canvas_draw, after collecting all layers. Keep a stable pool index.
    TextureLayer l={0,0,0,CW,CH,0,0,1,1,0xffffffffu,g_canvas_slice_used-1};g_texture_layers.push(l);
  }
  mem_zero(g_cv,CW*CH*4);g_cv_dirty=0;
}
static TextureImage *texture_for_canvas(const char *key){TextureImage *t=texture_custom(key);return t?t:texture_remastered_ui(key);}
static int texture_canvas_image(const char *key,int x,int y,int w,int h,float u,float v,float uw,float vh,uint32_t tint){
  TextureImage *t=texture_for_canvas(key);if(!t)return 0;
  GLuint texture=texture_gpu(t,0,1);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,g_smooth_filter?GL_LINEAR:GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,g_smooth_filter?GL_LINEAR:GL_NEAREST);texture_canvas_snapshot();
  TextureLayer l={texture,(float)x,(float)y,(float)w,(float)h,u,v,uw,vh,tint,-1,0};g_texture_layers.push(l);return 1;
}
static Arr<TextureImage> g_original_pictures={};
static int texture_canvas_original_picture(const Pic &p,int x,int y,int w,int h){
  if(!g_smooth_filter||!p.px||!p.texture_key[0])return 0;
  TextureImage *image=0;for(int i=0;i<g_original_pictures.n;i++)if(!str_icmp(g_original_pictures.p[i].key,p.texture_key)){image=&g_original_pictures.p[i];break;}
  if(!image){TextureImage t={};str_cpy(t.key,p.texture_key,sizeof t.key);t.w=p.w;t.h=p.h;t.pixels=(u8 *)sys_alloc((size_t)t.w*t.h*4);for(int i=0;i<t.w*t.h;i++){int index=p.px[i];for(int k=0;k<3;k++)t.pixels[i*4+k]=p.pal[index*3+k];t.pixels[i*4+3]=255;}g_original_pictures.push(t);image=&g_original_pictures.p[g_original_pictures.n-1];}
  GLuint texture=texture_gpu(image,0,1);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);texture_canvas_snapshot();
  TextureLayer l={texture,(float)x,(float)y,(float)w,(float)h,0,0,1,1,0xffffffffu,-1,0};g_texture_layers.push(l);return 1;
}
static void texture_canvas_draw_layer(GLuint texture,int W,int H,float x,float y,float w,float h,float u,float v,float uw,float vh,uint32_t tint){
  glBindTexture(GL_TEXTURE_2D,texture);
  float x0=g_scr.x+x*g_scr.w/(float)CW,y1=g_scr.y+g_scr.h-y*g_scr.h/(float)CH;
  glUniform4f(uc_rect,-1+2*x0/W,-1+2*(y1-h*g_scr.h/(float)CH)/H,-1+2*(x0+w*g_scr.w/(float)CW)/W,-1+2*y1/H);
  glUniform4f(uc_uv,u,v,uw,vh);glUniform4f(uc_tint,(tint&255)/255.f,((tint>>8)&255)/255.f,((tint>>16)&255)/255.f,(tint>>24)/255.f);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}

// The same scalable font path serves every game label, including the race HUD.
static GLuint g_revived_font_tex;
static void revived_font_draw(const Font &f,float x,float y,unsigned char c,uint32_t col,int scale){
  if(c<33 || c==127)return;
  if(!g_revived_font_tex){
    int w,h,ch;u8 *pixels=stbi_load_from_memory(REVIVED_FONT_PNG,sizeof REVIVED_FONT_PNG,&w,&h,&ch,4);if(!pixels)return;
    glGenTextures(1,&g_revived_font_tex);glBindTexture(GL_TEXTURE_2D,g_revived_font_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);sys_free(pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  }
  const RevivedGlyph &g=REVIVED_GLYPHS[c-32];float k=revived_font_height(f)*scale/(float)REVIVED_CAP;
  texture_canvas_snapshot();
  for(int pass=0;pass<2;pass++){
    float offset=pass==0?0.75f*scale:0;
    TextureLayer l={g_revived_font_tex,x+g.left*k+offset,y+(g.top+REVIVED_CAP)*k+offset,g.w*k,g.h*k,g.x/2048.f,g.y/2048.f,g.w/2048.f,g.h/2048.f,pass==0?(col&0xff000000u):col,-1,1};g_texture_layers.push(l);
  }
}
