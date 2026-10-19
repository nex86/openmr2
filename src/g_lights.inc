// Texture-derived emission mask: no assumptions about a car's light count or shape.
struct LampResources{GLuint fbo[3],tex[3],depth,program;int w,h;};static LampResources LM;
struct LampPose{M4 world;double time;int valid,model;};static LampPose g_lamp_history[8][8];
static void lamp_history_reset(){mem_zero(g_lamp_history,sizeof g_lamp_history);}
static void lamp_pose_record(const double *cam,const SceneModel &s){
  if(s.car<0)return;LampPose *history=g_lamp_history[s.car];M4 world=m4_id();
  for(int column=0;column<3;column++)for(int k=0;k<3;k++)world.m[column*4+k]=(float)(cam[k+3]*s.mv.m[column*4]+cam[k+6]*s.mv.m[column*4+1]-cam[k+9]*s.mv.m[column*4+2]);
  for(int k=0;k<3;k++)world.m[12+k]=(float)((cam[k]-g_shift[k])*MODEL_SCALE+cam[k+3]*s.mv.m[12]+cam[k+6]*s.mv.m[13]-cam[k+9]*s.mv.m[14]);
  if(history[0].valid&&(RC.t<history[0].time||RC.t-history[0].time>.25||history[0].model!=RC.car[s.car].car))mem_zero(history,sizeof g_lamp_history[0]);
  if(!history[0].valid||RC.t>history[0].time+.0001)for(int j=7;j>0;j--)history[j]=history[j-1];
  history[0]={world,RC.t,1,RC.car[s.car].car};
}
static void lamp_trails_draw(const double *cam){
  if(A.no_light_trails)return;glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glUniform1i(R.um_rear_only,1);
  for(int i=0;i<g_scene_count;i++){const SceneModel &s=g_scene[i];if(s.car<0)continue;LampPose *history=g_lamp_history[s.car];scene_damage_uniforms(s);glUniform1f(R.um_brake,RC.car[s.car].braking?1.f:0.f);
    for(int sample=1;sample<=9;sample++){double age=sample*.014,time=RC.t-age;int index=-1;for(int j=1;j<8;j++)if(history[j].valid&&history[j].time<=time&&history[j-1].time>=time){index=j;break;}if(index<0)continue;
      LampPose &a=history[index],&b=history[index-1];float f=(float)((time-a.time)/(b.time-a.time));M4 world;
      for(int k=0;k<16;k++)world.m[k]=a.world.m[k]+(b.world.m[k]-a.world.m[k])*f;
      double dx=world.m[12]-history[0].world.m[12],dy=world.m[13]-history[0].world.m[13],dz=world.m[14]-history[0].world.m[14],distance=d_sqrt(dx*dx+dy*dy+dz*dz);
      if(distance<.04||distance>40)continue;if(distance>9)for(int k=0;k<3;k++)world.m[12+k]=history[0].world.m[12+k]+(world.m[12+k]-history[0].world.m[12+k])*(float)(9/distance);
      double position[3],r[3],up[3],fw[3];for(int k=0;k<3;k++){position[k]=world.m[12+k]/MODEL_SCALE+g_shift[k];r[k]=world.m[k];up[k]=world.m[4+k];fw[k]=world.m[8+k];}
      float fade=(float)(1-age/.145);glUniform1f(R.um_trail_weight,.48f*fade*fade);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);draw_model_mv(*s.model,world_mv(cam,position,r,up,fw,1),s.wheel,0);glDepthMask(GL_FALSE);
    }
  }
  glUniform1i(R.um_rear_only,0);glUniform1f(R.um_trail_weight,1);glDisable(GL_BLEND);
}
static void lamp_texture(GLuint *id,int w,int h){glGenTextures(1,id);glBindTexture(GL_TEXTURE_2D,*id);glTexImage2D(GL_TEXTURE_2D,0,0x881A,w,h,0,GL_RGBA,GL_FLOAT,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
static int lamp_resources(int W,int H){
  if(LM.w==W&&LM.h==H)return 1;
  if(LM.w){glDeleteTextures(3,LM.tex);glDeleteTextures(1,&LM.depth);glDeleteFramebuffers(3,LM.fbo);}
  if(!LM.program)LM.program=shader_build("#version 330 core\nout vec2 uv;void main(){uv=vec2(float(gl_VertexID&1),float((gl_VertexID>>1)&1));gl_Position=vec4(uv*2.0-1.0,0,1);}",
    "#version 330 core\nin vec2 uv;out vec4 o;uniform sampler2D uTex;uniform vec2 uStep;uniform float uGain;void main(){vec3 c=texture(uTex,uv).rgb*0.227027;c+=(texture(uTex,uv+uStep*1.384615).rgb+texture(uTex,uv-uStep*1.384615).rgb)*0.316216;c+=(texture(uTex,uv+uStep*3.230769).rgb+texture(uTex,uv-uStep*3.230769).rgb)*0.070270;o=vec4(c*uGain,1.0);}");
  glGenFramebuffers(3,LM.fbo);
  for(int i=0;i<3;i++){glBindFramebuffer(GL_FRAMEBUFFER,LM.fbo[i]);lamp_texture(&LM.tex[i],i?W/2:W,i?H/2:H);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,LM.tex[i],0);}
  glGenTextures(1,&LM.depth);glBindTexture(GL_TEXTURE_2D,LM.depth);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,W,H,0,0x1902,GL_UNSIGNED_INT,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER,LM.fbo[0]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,LM.depth,0);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){sys_log("[error] Vehicle light bloom unavailable.");return 0;}
  LM.w=W;LM.h=H;sys_log("[event] Texture-shaped HDR vehicle-light bloom enabled.");return 1;
}
static void lamp_bloom_finish(GLint previous,int W,int H,const GLint *viewport){
  g_lamp_pass=0;glUniform1i(R.um_lamp_pass,0);glUniform1i(R.um_lamps,0);
  glUseProgram(LM.program);glBindVertexArray(R.empty_vao);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_BLEND);glActiveTexture(GL_TEXTURE0);
  GLint step=glGetUniformLocation(LM.program,"uStep"),gain=glGetUniformLocation(LM.program,"uGain");glUniform1i(glGetUniformLocation(LM.program,"uTex"),0);glUniform1f(gain,1);
  for(int i=0;i<2;i++){glBindFramebuffer(GL_FRAMEBUFFER,LM.fbo[i+1]);glViewport(0,0,W/2,H/2);glBindTexture(GL_TEXTURE_2D,LM.tex[i]);glUniform2f(step,i?0:2.f/W,i?2.f/(H/2):0);glDrawArrays(GL_TRIANGLE_STRIP,0,4);}
  glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);glViewport(0,0,W,H);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glBindTexture(GL_TEXTURE_2D,LM.tex[2]);glUniform2f(step,0,0);glUniform1f(gain,.8f);glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  glDisable(GL_BLEND);glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
}
static void vehicle_lights_draw(const double *cam,int W,int H){
  if(!g_vehicle_lights||(g_tex_nearest)||W<2||H<2)return;
  GLint previous=0,viewport[4];glGetIntegerv(0x8CA6,&previous);glGetIntegerv(0x0BA2,viewport);
  if(!lamp_resources(W,H)){glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);return;}
  glBindFramebuffer(GL_READ_FRAMEBUFFER,(GLuint)previous);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,LM.fbo[0]);glBlitFramebuffer(0,0,W,H,0,0,W,H,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER,LM.fbo[0]);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDepthMask(GL_FALSE);glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);
  model_shader_begin(g_proj,1);glUniform1i(R.um_lamps,1);glUniform1i(R.um_lamp_pass,1);g_lamp_pass=1;
  for(int i=0;i<g_scene_count;i++){SceneModel &s=g_scene[i];if(s.car<0)continue;scene_damage_uniforms(s);glUniform1f(R.um_brake,RC.car[s.car].braking?1.f:0.f);draw_model_mv(*s.model,s.mv,s.wheel,0);glDepthMask(GL_FALSE);}
  for(int i=0;i<g_scene_count;i++)lamp_pose_record(cam,g_scene[i]);lamp_trails_draw(cam);
  lamp_bloom_finish(previous,W,H,viewport);
}

// Same HDR lamp mask/blur as the race, restricted to this car preview's viewport.
static void preview_lights_draw(const Model &model,const M4 &vp,const M4 &pose,V3 eye,float wheel,int W,int H){
  if(!g_vehicle_lights||(g_tex_nearest)||W<2||H<2)return;
  GLint previous=0,viewport[4];glGetIntegerv(0x8CA6,&previous);glGetIntegerv(0x0BA2,viewport);
  if(!lamp_resources(W,H)){glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);return;}
  glBindFramebuffer(GL_READ_FRAMEBUFFER,(GLuint)previous);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,LM.fbo[0]);glBlitFramebuffer(0,0,W,H,0,0,W,H,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER,LM.fbo[0]);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDepthMask(GL_FALSE);glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);
  model_shader_begin(vp,1);glUniform3f(R.um_cam,eye.x,eye.y,eye.z);glUniform1i(R.um_lamps,1);glUniform1i(R.um_lamp_pass,1);glUniform1i(R.um_rear_only,A.graphics_test==15);glUniform1f(R.um_brake,A.graphics_test==15&&A.test_damage?1.f:0.f);g_lamp_pass=1;
  draw_model_mv(model,pose,wheel,0);glDepthMask(GL_FALSE);lamp_bloom_finish(previous,W,H,viewport);
}
