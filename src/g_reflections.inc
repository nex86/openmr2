// View-dependent environment radiance from the actual composed backplate.
// Capture precedes cars, particles and HUD: reflections never feed back into
// their own source. This is a finite camera view, not an invented panorama or
// depth-traced SSR; reflected rays outside that view use its average lighting.
struct ReflectionResources {
  GLuint resolve_fbo,resolve_texture,environment_fbo,environment_texture;
  GLuint picture_texture,picture_fbo,picture_source;const void *picture_owner,*picture_pixels;int picture_w,picture_h,picture_custom;char picture_key[512];u8 picture_palette[768];
  int resolve_w,resolve_h,size,valid,failed,test_captures,test_draws;float max_lod;
  M4 projection;V3 camera;
};
static ReflectionResources RF;
static int g_reflection_vehicle;
static void reflections_vehicle(int enabled){g_reflection_vehicle=enabled;}
static int reflection_map_size(){
  const int sizes[4]={256,512,768,1024};int q=g_effect_quality;if(q<0)q=0;if(q>3)q=3;
  GLint maximum=0;glGetIntegerv(0x0D33,&maximum);return maximum<sizes[q]?maximum:sizes[q];
}
static void reflections_restore_framebuffers(GLint read,GLint draw){
  glBindFramebuffer(GL_READ_FRAMEBUFFER,(GLuint)read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,(GLuint)draw);
}
static int reflections_resources(int width,int height){
  if(RF.failed||width<=0||height<=0)return 0;
  int size=reflection_map_size();GLint maximum=0;glGetIntegerv(0x0D33,&maximum);
  if(size<=0||width>maximum||height>maximum)return 0;
  if(RF.resolve_fbo&&RF.environment_fbo&&RF.resolve_w>=width&&RF.resolve_h>=height&&RF.size==size)return 1;
  GLint previous_read=0,previous_draw=0,active=0,binding=0;
  glGetIntegerv(0x8CAA,&previous_read);glGetIntegerv(0x8CA6,&previous_draw);glGetIntegerv(0x84E0,&active);
  glActiveTexture(GL_TEXTURE0);glGetIntegerv(0x8069,&binding);
  if(!RF.resolve_fbo)glGenFramebuffers(1,&RF.resolve_fbo);
  if(!RF.environment_fbo)glGenFramebuffers(1,&RF.environment_fbo);
  if(!RF.resolve_texture)glGenTextures(1,&RF.resolve_texture);
  if(!RF.environment_texture)glGenTextures(1,&RF.environment_texture);
  int ok=RF.resolve_fbo&&RF.environment_fbo&&RF.resolve_texture&&RF.environment_texture;
  if(ok){
    if(RF.resolve_w<width||RF.resolve_h<height){
      int w=RF.resolve_w>width?RF.resolve_w:width,h=RF.resolve_h>height?RF.resolve_h:height;
      glBindTexture(GL_TEXTURE_2D,RF.resolve_texture);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      RF.resolve_w=w;RF.resolve_h=h;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,RF.resolve_fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,RF.resolve_texture,0);
    ok=glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(RF.size!=size){
      glBindTexture(GL_TEXTURE_2D,RF.environment_texture);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      RF.size=size;RF.max_lod=0;for(int n=size;n>1;n/=2)RF.max_lod+=1;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,RF.environment_fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,RF.environment_texture,0);
    ok=ok&&glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE&&glGetError()==0;
  }
  reflections_restore_framebuffers(previous_read,previous_draw);
  glBindTexture(GL_TEXTURE_2D,(GLuint)binding);glActiveTexture((GLenum)active);
  if(!ok){RF.failed=1;RF.valid=0;sys_log("[error] Vehicle environment reflections unavailable; keeping the ordinary vehicle material.");}
  return ok;
}
static void reflections_preload(int width,int height){
  if(!g_tex_nearest&&g_vehicle_reflections&&g_effect_strength[6]>0)reflections_resources(width,height);
}
static void reflections_capture(const M4 &projection,V3 camera){
  RF.valid=0;g_reflection_vehicle=0;
  if(g_tex_nearest||!g_vehicle_reflections||g_effect_strength[6]<=0)return;
  GLint viewport[4],previous_read=0,previous_draw=0,active=0,binding=0;
  glGetIntegerv(0x0BA2,viewport);
  if(!reflections_resources(viewport[2],viewport[3]))return;
  glGetIntegerv(0x8CAA,&previous_read);glGetIntegerv(0x8CA6,&previous_draw);glGetIntegerv(0x84E0,&active);
  glActiveTexture(GL_TEXTURE0);glGetIntegerv(0x8069,&binding);
  // Resolving a multisampled framebuffer cannot simultaneously change size.
  // First resolve the exact viewport, then downsample the single-sample image.
  glBindFramebuffer(GL_READ_FRAMEBUFFER,(GLuint)previous_draw);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,RF.resolve_fbo);
  glBlitFramebuffer(viewport[0],viewport[1],viewport[0]+viewport[2],viewport[1]+viewport[3],0,0,viewport[2],viewport[3],GL_COLOR_BUFFER_BIT,GL_NEAREST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,RF.resolve_fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,RF.environment_fbo);
  glBlitFramebuffer(0,0,viewport[2],viewport[3],0,0,RF.size,RF.size,GL_COLOR_BUFFER_BIT,GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D,RF.environment_texture);glGenerateMipmap(GL_TEXTURE_2D);
  int ok=glGetError()==0;
  reflections_restore_framebuffers(previous_read,previous_draw);
  glBindTexture(GL_TEXTURE_2D,(GLuint)binding);glActiveTexture((GLenum)active);
  if(!ok){RF.failed=1;sys_log("[error] Vehicle reflection backplate capture failed; the reflection pass has been disabled.");return;}
  RF.projection=projection;RF.camera=camera;RF.valid=1;if(g_graphics_test_active)RF.test_captures++;
}
// Preview menus have already composited labels and the cursor by the time the
// car is drawn. Read the clean background image instead of that UI framebuffer.
// The optional replacement is the same custom picture used by the canvas.
static void reflections_preview_picture(const u8 *indices,const u8 *palette,int image_w,int image_h,const char *key,TextureImage *replacement,
    float x,float y,float width,float height,const M4 &projection,V3 camera){
  RF.valid=0;g_reflection_vehicle=0;
  if(g_tex_nearest||!g_vehicle_reflections||g_effect_strength[6]<=0||image_w<=0||image_h<=0)return;
  if((!indices||!palette)&&!replacement)return;
  if(!reflections_resources(1,1))return;
  GLint previous_read=0,previous_draw=0,active=0,binding=0;
  glGetIntegerv(0x8CAA,&previous_read);glGetIntegerv(0x8CA6,&previous_draw);glGetIntegerv(0x84E0,&active);
  glActiveTexture(GL_TEXTURE0);glGetIntegerv(0x8069,&binding);
  int source_w=replacement?replacement->w:image_w,source_h=replacement?replacement->h:image_h;
  const void *owner=replacement?(const void *)replacement:(const void *)indices;
  const void *pixels=replacement?(const void *)replacement->pixels:(const void *)indices;
  GLuint identity=replacement?replacement->tex:0;
  int changed=RF.picture_custom!=(replacement?1:0)||RF.picture_owner!=owner||RF.picture_pixels!=pixels||RF.picture_source!=identity||RF.picture_w!=source_w||RF.picture_h!=source_h||str_icmp(RF.picture_key,key?key:"");
  if(!replacement)for(int i=0;i<768&&!changed;i++)if(RF.picture_palette[i]!=palette[i])changed=1;
  if(!RF.picture_texture)glGenTextures(1,&RF.picture_texture);
  glBindTexture(GL_TEXTURE_2D,RF.picture_texture);
  if(changed){
    u8 *rgba=(u8 *)sys_alloc((size_t)source_w*source_h*4);
    for(int i=0;i<source_w*source_h;i++){
      if(replacement){
        // Menus clear their canvas to opaque black before drawing the picture.
        // Composite straight PNG alpha against that actual backdrop BEFORE any
        // linear filtering or mip generation: hidden RGB must not leak into a
        // highlight or roughness level. TextureImage source pixels stay intact.
        const u8 *pixel=replacement->pixels+i*4;unsigned alpha=pixel[3];
        for(int k=0;k<3;k++)rgba[i*4+k]=(u8)((pixel[k]*alpha+127)/255);
      }else{int index=indices[i]*3;for(int k=0;k<3;k++)rgba[i*4+k]=palette[index+k];}
      rgba[i*4+3]=255;
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,source_w,source_h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);sys_free(rgba);
    RF.picture_owner=owner;RF.picture_pixels=pixels;RF.picture_source=identity;RF.picture_custom=replacement?1:0;
    RF.picture_w=source_w;RF.picture_h=source_h;str_cpy(RF.picture_key,key?key:"",sizeof RF.picture_key);if(!replacement)mem_cpy(RF.picture_palette,palette,768);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  }
  GLuint source=RF.picture_texture;
  if(!RF.picture_fbo)glGenFramebuffers(1,&RF.picture_fbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,RF.picture_fbo);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,source,0);
  int ok=source&&glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
  if(ok){
    int x0=(int)(x*source_w/640.f+.5f),x1=(int)((x+width)*source_w/640.f+.5f);
    int y0=(int)((y+height)*source_h/480.f+.5f),y1=(int)(y*source_h/480.f+.5f);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,RF.environment_fbo);
    // Indexed pictures are uploaded top row first; the captured environment is
    // bottom row first, matching clip-space projection and the race capture.
    glBlitFramebuffer(x0,y0,x1,y1,0,0,RF.size,RF.size,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D,RF.environment_texture);glGenerateMipmap(GL_TEXTURE_2D);
    ok=glGetError()==0;
  }
  reflections_restore_framebuffers(previous_read,previous_draw);
  glBindTexture(GL_TEXTURE_2D,(GLuint)binding);glActiveTexture((GLenum)active);
  if(!ok){RF.failed=1;sys_log("[error] Vehicle reflection preview background unavailable; keeping the ordinary vehicle material.");return;}
  RF.projection=projection;RF.camera=camera;RF.valid=1;if(g_graphics_test_active)RF.test_captures++;
}
static void reflections_model_bind(){
  int enabled=!g_tex_nearest&&g_vehicle_reflections&&RF.valid&&g_reflection_vehicle&&g_effect_strength[6]>0&&!g_shadow_pass&&!g_lamp_pass&&!g_oil_render;
  glUniform1i(R.um_reflection_enabled,enabled);if(!enabled)return;
  if(g_graphics_test_active)RF.test_draws++;
  glUniform1i(R.um_reflection_map,5);glUniformMatrix4fv(R.um_reflection_projection,1,GL_FALSE,RF.projection.m);
  glUniform3f(R.um_reflection_camera,RF.camera.x,RF.camera.y,RF.camera.z);
  glUniform1f(R.um_reflection_lod,RF.max_lod);glUniform1f(R.um_reflection_strength,g_effect_strength[6]);
  glActiveTexture(GL_TEXTURE0+5);glBindTexture(GL_TEXTURE_2D,RF.environment_texture);glActiveTexture(GL_TEXTURE0);
}
