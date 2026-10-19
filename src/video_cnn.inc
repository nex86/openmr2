// FSRCNNX inference stays at decoded source resolution. Only the final learned
// luma and its reconstructed RGB image are enlarged; feature tensors are signed.
#include "video_cnn_data.h"
static GLuint g_video_cnn_program[2][26],g_video_cnn_scratch[19],g_video_cnn_compose;
static int g_video_cnn_w[19],g_video_cnn_h[19];
static const char *FS_VIDEO_CNN_COMPOSE=R"cnn(#version 330 core
uniform sampler2D uSource,uLuma;uniform vec2 uOutput;out vec4 o;
const vec3 Y=vec3(.299,.587,.114);
float lum(ivec2 p){return texelFetch(uLuma,clamp(p,ivec2(0),textureSize(uLuma,0)-1),0).r;}
void main(){
  vec2 uv=gl_FragCoord.xy/uOutput;ivec2 p=ivec2(gl_FragCoord.xy);
  float center=lum(p),n=lum(p+ivec2(0,-1)),s=lum(p+ivec2(0,1)),w=lum(p+ivec2(-1,0)),e=lum(p+ivec2(1,0));
  // Restrained detail enhancement. Bound halos and keep compression speckles
  // from acquiring the large contrast of an unrestricted unsharp mask.
  float detail=clamp((center-(n+s+w+e)*.25)*.22,-.015,.015);
  ivec2 base=ivec2(floor(uv*vec2(textureSize(uSource,0))-.5));
  float lo=1.,hi=0.;
  for(int y=0;y<2;y++)for(int x=0;x<2;x++){
    float value=dot(texelFetch(uSource,clamp(base+ivec2(x,y),ivec2(0),textureSize(uSource,0)-1),0).rgb,Y);
    lo=min(lo,value);hi=max(hi,value);
  }
  float learned=clamp(center+detail,max(0.,lo-.02),min(1.,hi+.02));
  vec3 original=texture(uSource,uv).rgb;
  // Preserve the source chroma; the model reconstructs gamma-coded BT.601 luma.
  o=vec4(clamp(original+vec3(learned-dot(original,Y)),0.,1.),1.);
}
)cnn";
static bool video_cnn_target(GLuint &texture,int &old_w,int &old_h,int w,int h,GLenum format){
  glActiveTexture(GL_TEXTURE0);
  if(!texture)glGenTextures(1,&texture);if(!texture)return false;
  glBindTexture(GL_TEXTURE_2D,texture);
  if(old_w!=w||old_h!=h){
    glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,GL_RGBA,GL_FLOAT,0);
    if(glGetError()!=0)return false;old_w=w;old_h=h;
  }
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return false;
  glViewport(0,0,w,h);return true;
}
static void video_cnn_extent(int input_w,int input_h,int requested_w,int requested_h,int maximum,int &w,int &h){
  double scale=(double)requested_w/input_w,vertical=(double)requested_h/input_h;if(vertical>scale)scale=vertical;
  if(scale<2)scale=2;
  int limit=maximum<2048?maximum:2048;
  if(input_w*scale>limit)scale=(double)limit/input_w;
  if(input_h*scale>limit)scale=(double)limit/input_h;
  w=(int)(input_w*scale+.5);h=(int)(input_h*scale+.5);
}
static GLuint video_cnn_texture(VideoUpscaleFrame &v,GLuint source,int filter,int requested_w,int requested_h){
  int model_index=filter-4;if(model_index<0||model_index>1)return 0;
  if(!g_video_maximum)glGetIntegerv(0x0D33,&g_video_maximum);
  if(v.input_w*2>g_video_maximum||v.input_h*2>g_video_maximum)return 0;
  int w,h;video_cnn_extent(v.input_w,v.input_h,requested_w,requested_h,g_video_maximum,w,h);
  bool cached=!v.dirty&&v.filter==filter&&v.w[2]==w&&v.h[2]==h;
  if(cached){
    if(A.graphics_test==16){g_video_profile_cached++;video_profile_save();}
    if(!(A.graphics_test==16&&g_video_profile_cached==8&&g_video_profile_runs==1))return v.tex[2];
  }
  const VideoCnnModel &model=VIDEO_CNN_MODELS[model_index];
  for(int pass=0;pass<model.count;pass++)if(!g_video_cnn_program[model_index][pass]){
    g_video_cnn_program[model_index][pass]=video_program_build(VS_FULL,model.passes[pass].fragment);
    if(!g_video_cnn_program[model_index][pass])return 0;
  }
  if(!g_video_cnn_compose)g_video_cnn_compose=video_program_build(VS_FULL,FS_VIDEO_CNN_COMPOSE);
  if(!g_video_cnn_compose||!video_filter_program(4))return 0;
  GLint previous=0,viewport[4];glGetIntegerv(0x8CA6,&previous);glGetIntegerv(0x0BA2,viewport);
  double profile_start=0;if(A.graphics_test==16){glFinish();profile_start=sys_time();}
  if(!g_video_filter_fbo)glGenFramebuffers(1,&g_video_filter_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER,g_video_filter_fbo);glBindVertexArray(R.empty_vao);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
  bool ok=true;GLuint textures[19]={};textures[0]=source;
  for(int pass=0;pass<model.count&&ok;pass++){
    const VideoCnnPass &p=model.passes[pass];int pw=v.input_w*p.scale,ph=v.input_h*p.scale;
    if(!video_cnn_target(g_video_cnn_scratch[p.output],g_video_cnn_w[p.output],g_video_cnn_h[p.output],pw,ph,GL_RGBA16F)){ok=false;break;}
    textures[p.output]=g_video_cnn_scratch[p.output];GLuint program=g_video_cnn_program[model_index][pass];glUseProgram(program);
    glUniform4f(glGetUniformLocation(program,"uRect"),-1,-1,1,1);glUniform2f(glGetUniformLocation(program,"uOutput"),(float)pw,(float)ph);
    for(int binding=0;binding<p.binding_count;binding++){
      glActiveTexture(GL_TEXTURE0+binding);glBindTexture(GL_TEXTURE_2D,textures[p.bindings[binding]]);
      char name[]="uBind0";name[5]=(char)('0'+binding);glUniform1i(glGetUniformLocation(program,name),binding);
    }
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  }
  GLuint luma=textures[model.final_slot];
  // For displays beyond the learned 2x image, enlarge its luma with separable
  // Lanczos, retaining FP16 until final RGB composition. Run the CNN only once.
  if(ok&&(w!=v.input_w*2||h!=v.input_h*2))for(int axis=0;axis<2&&ok;axis++){
    int ph=axis?h:v.input_h*2;
    // These two cache slots may have held RGBA8 images from another filter.
    if(v.filter!=filter&&v.tex[axis]){glDeleteTextures(1,&v.tex[axis]);v.tex[axis]=0;v.w[axis]=v.h[axis]=0;}
    ok=video_cnn_target(v.tex[axis],v.w[axis],v.h[axis],w,ph,GL_RGBA16F);if(!ok)break;
    GLuint program=g_video_programs[4];glUseProgram(program);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,luma);
    glUniform1i(glGetUniformLocation(program,"uSource"),0);glUniform4f(glGetUniformLocation(program,"uRect"),-1,-1,1,1);
    glUniform2f(glGetUniformLocation(program,"uOutput"),(float)w,(float)ph);glUniform2f(glGetUniformLocation(program,"uAxis"),axis?0.f:1.f,axis?1.f:0.f);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);luma=v.tex[axis];
  }
  if(ok){
    ok=video_cnn_target(v.tex[2],v.w[2],v.h[2],w,h,GL_RGBA8);
    if(ok){
      glUseProgram(g_video_cnn_compose);glUniform4f(glGetUniformLocation(g_video_cnn_compose,"uRect"),-1,-1,1,1);
      glUniform2f(glGetUniformLocation(g_video_cnn_compose,"uOutput"),(float)w,(float)h);
      glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glUniform1i(glGetUniformLocation(g_video_cnn_compose,"uSource"),0);
      glActiveTexture(GL_TEXTURE0+1);glBindTexture(GL_TEXTURE_2D,luma);glUniform1i(glGetUniformLocation(g_video_cnn_compose,"uLuma"),1);
      glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    }
  }
  if(ok&&glGetError()!=0)ok=false;
  if(A.graphics_test==16&&ok){glFinish();g_video_profile_last=(sys_time()-profile_start)*1000;g_video_profile_ms+=g_video_profile_last;g_video_profile_runs++;video_profile_save();}
  glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glActiveTexture(GL_TEXTURE0);
  if(!ok)return 0;
  v.dirty=0;v.factor=0;v.filter=filter;return v.tex[2];
}
