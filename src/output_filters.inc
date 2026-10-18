// Final image only. Source FBO, original rasterizer, textures, UI and hit boxes stay native.
#include "output_filter_data.h"
static const char *VS_OUTPUT_FILTER=R"filter(#version 330 core
uniform vec4 uRect;uniform vec2 TextureSize;
out vec4 TEX0,TEX1,TEX2,TEX3;out vec2 VARps;
void main(){
  vec2 c=vec2(float(gl_VertexID&1),float((gl_VertexID>>1)&1));
  gl_Position=vec4(mix(uRect.xy,uRect.zw,c),0,1);
  vec2 p=1.0/TextureSize;VARps=p;TEX0=vec4(c,0,1);
  TEX1=c.xxxy+vec4(-p.x,0,p.x,-p.y);
  TEX2=c.xxxy+vec4(-p.x,0,p.x,0);
  TEX3=c.xxxy+vec4(-p.x,0,p.x,p.y);
}
)filter";
static const char *FS_INTERLACING=R"filter(#version 330 core
in vec4 TEX0;out vec4 o;uniform sampler2D Texture;uniform vec2 TextureSize;uniform int FrameCount;
void main(){vec3 color=texture(Texture,TEX0.xy).rgb;int row=int(floor(TEX0.y*TextureSize.y));float beam=((row+FrameCount)&1)==0?1.0:0.5;o=vec4(color*beam,1);}
)filter";
static const char *FS_CRT_OUTPUT=R"filter(#version 330 core
in vec4 TEX0;out vec4 o;uniform sampler2D Texture;uniform vec2 TextureSize,OutputOrigin;
void main(){
  vec2 uv=TEX0.xy,p=1.0/TextureSize;vec3 c=texture(Texture,uv).rgb;
  vec3 glow=(texture(Texture,uv+vec2(p.x,0)).rgb+texture(Texture,uv-vec2(p.x,0)).rgb)*0.5;
  c=mix(c,glow,0.08)*1.10;
  float beam=0.88+0.12*sin(3.14159265*(gl_FragCoord.y-OutputOrigin.y));
  int phosphor=int(floor(gl_FragCoord.x-OutputOrigin.x))%3;
  vec3 mask=phosphor==0?vec3(1,0.82,0.82):phosphor==1?vec3(0.82,1,0.82):vec3(0.82,0.82,1);
  float vignette=1.0-0.18*dot(uv-0.5,uv-0.5);o=vec4(clamp(c*mask*beam*vignette,0,1),1);
}
)filter";
struct OutputFilterProgram{GLuint program;GLint rect,source,size,output,origin,frame,lut;int attempted;};
static OutputFilterProgram g_output_filters[6];static GLuint g_hq2x_lut;
static OutputFilterProgram *output_filter_program(int filter){
  if(filter<2||filter>5)return 0;OutputFilterProgram &p=g_output_filters[filter];
  if(!p.attempted){
    p.attempted=1;const char *source=filter==2?FS_HQ2X:filter==3?FS_XBRZ:filter==4?FS_INTERLACING:FS_CRT_OUTPUT;
    p.program=shader_build(VS_OUTPUT_FILTER,source);if(!p.program){sys_log("[error] Output filter shader failed; falling back to Point.");return 0;}
    p.rect=glGetUniformLocation(p.program,"uRect");p.source=glGetUniformLocation(p.program,"Texture");p.size=glGetUniformLocation(p.program,"TextureSize");
    p.output=glGetUniformLocation(p.program,"OutputSize");p.origin=glGetUniformLocation(p.program,"OutputOrigin");p.frame=glGetUniformLocation(p.program,"FrameCount");p.lut=glGetUniformLocation(p.program,"LUT");
  }
  return p.program?&p:0;
}
static int output_filter_draw(int filter,int source_w,int source_h,int dx,int dy,int dw,int dh,float x0,float y0,float x1,float y1){
  OutputFilterProgram *p=output_filter_program(filter);if(!p)return 0;
  if(filter==2&&!g_hq2x_lut){
    int w,h,ch;u8 *data=stbi_load_from_memory(HQ2X_LUT_PNG,sizeof HQ2X_LUT_PNG,&w,&h,&ch,4);if(!data)return 0;
    glGenTextures(1,&g_hq2x_lut);glActiveTexture(GL_TEXTURE0+1);glBindTexture(GL_TEXTURE_2D,g_hq2x_lut);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);sys_free(data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glActiveTexture(GL_TEXTURE0);
  }
  glUseProgram(p->program);glUniform1i(p->source,0);glUniform2f(p->size,(float)source_w,(float)source_h);glUniform2f(p->output,(float)dw,(float)dh);
  glUniform2f(p->origin,(float)dx,(float)dy);glUniform1i(p->frame,((int)(A.time*60))&1);glUniform4f(p->rect,x0,y0,x1,y1);
  if(filter==2){glActiveTexture(GL_TEXTURE0+1);glBindTexture(GL_TEXTURE_2D,g_hq2x_lut);glUniform1i(p->lut,1);glActiveTexture(GL_TEXTURE0);}
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);return 1;
}
