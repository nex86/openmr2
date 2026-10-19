#include "video_fsr_data.h"
#include "video_filter_data.h"
static const char *FS_VIDEO_LANCZOS=R"video(#version 330 core
uniform sampler2D uSource;uniform vec2 uOutput,uAxis;out vec4 o;
float sinc(float x){x=abs(x);return x<0.0001?1.0:sin(3.14159265*x)/(3.14159265*x);}
void main(){vec2 p=gl_FragCoord.xy*vec2(textureSize(uSource,0))/uOutput-0.5,b=floor(p);vec3 c=vec3(0);float total=0;for(int i=-2;i<=3;i++){vec2 q=mix(p,b+float(i),uAxis);float d=dot(p-q,uAxis),w=sinc(d)*sinc(d/3.0);c+=texelFetch(uSource,clamp(ivec2(floor(q+0.5)),ivec2(0),textureSize(uSource,0)-ivec2(1)),0).rgb*w;total+=w;}o=vec4(clamp(c/max(total,0.0001),0.0,1.0),1);}
)video";
struct VideoUpscaleFrame { GLuint source,tex[3];int w[3],h[3],input_w,input_h,dirty,factor,filter,frame;unsigned generation;const void *owner; };
static VideoUpscaleFrame g_video_frames[4];
static GLuint g_video_programs[5],g_video_filter_fbo,g_video_hq_lut;
enum {VIDEO_FILTER_COUNT=6};
static int g_video_filter_failed[VIDEO_FILTER_COUNT],g_video_next,g_video_maximum;
static const char *VIDEO_FILTER_NAMES[VIDEO_FILTER_COUNT]={"FSR","HQ4x","4xBR","Lanczos3","FSRCNNX AI","FSRCNNX AI HQ"};
// Synchronous timing is restricted to the explicit developer capture mode.
static double g_video_profile_ms,g_video_profile_last;static int g_video_profile_runs,g_video_profile_cached;
static void video_profile_save(){char report[512]="GPU video filter profiling (explicit test mode)\nprocessed_frames=";str_int(report,sizeof report,g_video_profile_runs);str_cat(report,"\ncache_hits=",sizeof report);str_int(report,sizeof report,g_video_profile_cached);str_cat(report,"\nmean_processing_microseconds=",sizeof report);str_int(report,sizeof report,g_video_profile_runs?(long)(g_video_profile_ms*1000/g_video_profile_runs):0);str_cat(report,"\nlast_processing_microseconds=",sizeof report);str_int(report,sizeof report,(long)(g_video_profile_last*1000));str_cat(report,"\nrenderer=",sizeof report);str_cat(report,(const char *)glGetString(GL_RENDERER),sizeof report);str_cat(report,"\n",sizeof report);sys_write_file("video-upscale-profile.txt",report,str_len(report));}
static VideoUpscaleFrame *video_frame_find(GLuint texture){for(int i=0;i<4;i++)if(g_video_frames[i].source==texture)return &g_video_frames[i];return 0;}
static void video_fsr_uploaded(GLuint texture,const Video &video){
  VideoUpscaleFrame *slot=video_frame_find(texture);
  if(!slot){slot=&g_video_frames[g_video_next++%4];for(int k=0;k<3;k++)if(slot->tex[k])glDeleteTextures(1,&slot->tex[k]);mem_zero(slot,sizeof *slot);slot->source=texture;slot->dirty=1;}
  if(slot->owner!=video.data||slot->generation!=video.generation||slot->frame!=video.decoded_frame)slot->dirty=1;
  slot->generation=video.generation;
  slot->owner=video.data;slot->frame=video.decoded_frame;slot->input_w=video.w;slot->input_h=video.h;
}
static GLuint video_program_build(const char *vs,const char *fs){
  GLuint program=glCreateProgram();const char *stages[2]={vs,fs};const GLenum types[2]={GL_VERTEX_SHADER,GL_FRAGMENT_SHADER};
  for(int stage=0;stage<2;stage++){GLuint shader=glCreateShader(types[stage]);glShaderSource(shader,1,&stages[stage],0);glCompileShader(shader);GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];GLsizei length=0;glGetShaderInfoLog(shader,sizeof log-1,&length,log);log[length]=0;sys_log(log);if(A.graphics_test==16)sys_write_file("video-filter-error.txt",log,length);glDeleteShader(shader);glDeleteProgram(program);return 0;}glAttachShader(program,shader);glDeleteShader(shader);}
  glLinkProgram(program);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
  if(!linked){char log[4096];GLsizei length=0;glGetProgramInfoLog(program,sizeof log-1,&length,log);log[length]=0;sys_log(log);if(A.graphics_test==16)sys_write_file("video-filter-error.txt",log,length);glDeleteProgram(program);return 0;}return program;
}
static GLuint video_filter_program(int index){
  if(g_video_programs[index])return g_video_programs[index];
  const char *vs=index==1||index==2?VS_VIDEO_HQ:index==3?VS_VIDEO_XBR:VS_FULL;
  const char *fs=index==0?FS_VIDEO_FSR:index==1?FS_VIDEO_HQ_PATTERN:index==2?FS_VIDEO_HQ_BLEND:index==3?FS_VIDEO_XBR:FS_VIDEO_LANCZOS;
  return g_video_programs[index]=video_program_build(vs,fs);
}
#include "video_cnn.inc"
static GLuint video_fsr_texture(GLuint source,int output_w,int output_h){
  int filter=g_video_filter;if(filter<0||filter>=VIDEO_FILTER_COUNT)filter=0;
  if(!g_video_upscale||g_tex_nearest)return source;
  VideoUpscaleFrame *slot=video_frame_find(source);if(!slot)return source;VideoUpscaleFrame &v=*slot;
  if(v.input_w<=0||v.input_h<=0||(output_w<=v.input_w&&output_h<=v.input_h))return source;
  if(filter>=4){
    if(!g_video_filter_failed[filter]){
      GLuint result=video_cnn_texture(v,source,filter,output_w,output_h);if(result)return result;
      g_video_filter_failed[filter]=1;sys_log("[warning] FSRCNNX GPU video processing failed; using Lanczos3 for this selection.");
    }
    filter=3;
  }
  if(g_video_filter_failed[filter])return source;
  int factor=filter==1||filter==2?4:output_w>v.input_w*2||output_h>v.input_h*2?4:2;
  if(!g_video_maximum)glGetIntegerv(0x0D33,&g_video_maximum);
  if(filter==0||filter==3)if(v.input_w*factor>2048||v.input_h*factor>2048)factor=2;
  if(v.input_w*factor>g_video_maximum||v.input_h*factor>g_video_maximum)return source;
  if(!v.dirty&&v.factor==factor&&v.filter==filter){if(A.graphics_test==16){g_video_profile_cached++;video_profile_save();}if(!(A.graphics_test==16&&g_video_profile_cached==8&&g_video_profile_runs==1))return v.tex[2];}
  int programs[3]={0,0,0},width[3]={0,0,0},height[3]={0,0,0};
  if(filter==0){programs[0]=programs[2]=0;programs[1]=factor==4?0:-1;width[0]=v.input_w*2;height[0]=v.input_h*2;width[1]=width[2]=v.input_w*factor;height[1]=height[2]=v.input_h*factor;}
  else if(filter==1){programs[0]=1;programs[1]=-1;programs[2]=2;width[0]=v.input_w;height[0]=v.input_h;width[2]=v.input_w*4;height[2]=v.input_h*4;}
  else if(filter==2){programs[0]=programs[1]=-1;programs[2]=3;width[2]=v.input_w*4;height[2]=v.input_h*4;}
  else{programs[0]=programs[2]=4;programs[1]=-1;width[0]=width[2]=v.input_w*factor;height[0]=v.input_h;height[2]=v.input_h*factor;}
  for(int pass=0;pass<3;pass++)if(programs[pass]>=0&&!video_filter_program(programs[pass])){g_video_filter_failed[filter]=1;sys_log("[error] GPU video filter compilation failed; using original video filtering.");return source;}
  if(filter==1&&!g_video_hq_lut){int w,h,ch;u8 *pixels=stbi_load_from_memory(VIDEO_HQ4X_LUT,sizeof VIDEO_HQ4X_LUT,&w,&h,&ch,4);if(!pixels)return source;glGenTextures(1,&g_video_hq_lut);glActiveTexture(GL_TEXTURE0+2);glBindTexture(GL_TEXTURE_2D,g_video_hq_lut);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);sys_free(pixels);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
  GLint previous=0;glGetIntegerv(0x8CA6,&previous);GLint viewport[4];glGetIntegerv(0x0BA2,viewport);
  double profile_start=0;if(A.graphics_test==16){glFinish();profile_start=sys_time();}
  if(!g_video_filter_fbo)glGenFramebuffers(1,&g_video_filter_fbo);
  glBindVertexArray(R.empty_vao);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
  GLuint input=source;
  for(int pass=0;pass<3;pass++){
    if(programs[pass]<0)continue;GLuint program=g_video_programs[programs[pass]];glUseProgram(program);
    int w=width[pass],h=height[pass];glActiveTexture(GL_TEXTURE0);
    if(!v.tex[pass])glGenTextures(1,&v.tex[pass]);glBindTexture(GL_TEXTURE_2D,v.tex[pass]);
    if(v.w[pass]!=w||v.h[pass]!=h){v.w[pass]=w;v.h[pass]=h;glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,0);}
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER,g_video_filter_fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,v.tex[pass],0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);g_video_filter_failed[filter]=1;return source;}
    glViewport(0,0,w,h);glBindTexture(GL_TEXTURE_2D,input);
    int input_w=filter==3&&pass==2?width[0]:filter==0&&pass>0?v.input_w*(pass==1?2:factor):v.input_w;
    int input_h=filter==0&&pass>0?v.input_h*(pass==1?2:factor):v.input_h;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glUniform1i(glGetUniformLocation(program,"uSource"),0);glUniform1i(glGetUniformLocation(program,"Texture"),0);glUniform1i(glGetUniformLocation(program,"decal"),0);
    glUniform4f(glGetUniformLocation(program,"uRect"),-1,-1,1,1);glUniform2f(glGetUniformLocation(program,"TextureSize"),(float)input_w,(float)input_h);glUniform2f(glGetUniformLocation(program,"InputSize"),(float)input_w,(float)input_h);glUniform2f(glGetUniformLocation(program,"OutputSize"),(float)w,(float)h);glUniform2f(glGetUniformLocation(program,"uOutput"),(float)w,(float)h);glUniform1i(glGetUniformLocation(program,"uStage"),pass==2?1:0);glUniform2f(glGetUniformLocation(program,"uAxis"),pass==0?1.f:0.f,pass==0?0.f:1.f);
    if(filter==1&&pass==2){glActiveTexture(GL_TEXTURE0+1);glBindTexture(GL_TEXTURE_2D,source);glUniform1i(glGetUniformLocation(program,"OrigTexture"),1);glActiveTexture(GL_TEXTURE0+2);glBindTexture(GL_TEXTURE_2D,g_video_hq_lut);glUniform1i(glGetUniformLocation(program,"LUT"),2);glActiveTexture(GL_TEXTURE0);}
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);input=v.tex[pass];
  }
  if(A.graphics_test==16){glFinish();g_video_profile_last=(sys_time()-profile_start)*1000;g_video_profile_ms+=g_video_profile_last;g_video_profile_runs++;video_profile_save();}
  glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);v.dirty=0;v.factor=factor;v.filter=filter;return v.tex[2];
}
