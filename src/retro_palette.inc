// Preserve each indexed asset's own colors. Never remap UI/material RGB to a
// video's changing palette. Six-bit DAC precision precedes output-only filters.
// A shared byte framebuffer/shade-table rasterizer is still not reconstructed.
struct RetroPalette { GLuint fbo,tex,program; int w,h; };
static RetroPalette g_retro_palette;
static int g_retro_palette_allowed=1;
static void retro_palette_colors(u8 colors[1024]){
  const u8 *source=G.bg.pal;int six=0;
  // Developer font preview bypasses go()/background loading; it still needs
  // an original palette rather than the zero-initialized background storage.
  if(!G.bg.px&&G.state==GS_FONTTEST)source=g_font_h14.pic.pal;
  if((G.state==GS_RACE||G.state==GS_REPLAY)&&TK.vid.pals&&TK.vid.decoded_frame>=0){source=TK.vid.pals[TK.vid.pal_id[TK.vid.decoded_frame]];six=1;}
  else if(G.state==GS_CLIP&&G.clip.pals&&G.clip.decoded_frame>=0){source=G.clip.pals[G.clip.pal_id[G.clip.decoded_frame]];six=1;}
  for(int i=0;i<256;i++){
    for(int k=0;k<3;k++){int dac=six?(source[i*3+k]&63):(source[i*3+k]>>2);
      // Original race sprites/materials reserve the top eight DAC entries.
      if((G.state==GS_RACE||G.state==GS_REPLAY)&&i>=248&&g_model_reserved_ready)dac=g_model_reserved[(i-248)*3+k]>>2;
      colors[i*4+k]=(u8)(dac*255/63);
    }
    colors[i*4+3]=255;
  }
}
static GLuint retro_palette_apply(){
  RetroPalette &p=g_retro_palette;
  if(!p.program){
    const char *fragment="#version 330 core\nin vec2 vUV;out vec4 o;uniform sampler2D uTex;void main(){vec3 c=texelFetch(uTex,ivec2(gl_FragCoord.xy),0).rgb;o=vec4(floor(c*255.0/4.0+0.001)/63.0,1.0);}";
    p.program=shader_build(VS_FULL,fragment);glGenTextures(1,&p.tex);glGenFramebuffers(1,&p.fbo);
  }
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,p.tex);
  if(p.w!=g_post.w||p.h!=g_post.h){p.w=g_post.w;p.h=g_post.h;glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,p.w,p.h,0,GL_RGBA,GL_UNSIGNED_BYTE,0);}
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glBindFramebuffer(GL_FRAMEBUFFER,p.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,p.tex,0);
  glViewport(0,0,p.w,p.h);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glBindTexture(GL_TEXTURE_2D,g_post.tex);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glUseProgram(p.program);glUniform1i(glGetUniformLocation(p.program,"uTex"),0);glUniform4f(glGetUniformLocation(p.program,"uRect"),-1,-1,1,1);glBindVertexArray(R.empty_vao);glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  return p.tex;
}
