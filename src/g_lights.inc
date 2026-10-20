// Texture-derived emission mask: no assumptions about a car's light count or shape.
struct LampResources{GLuint fbo[3],tex[3],depth,program,trail_fbo,trail_tex,trail_program;int w,h,trail_w,trail_h;};static LampResources LM;
struct LampPose{M4 world;double time;int valid,model;};static LampPose g_lamp_history[8][32];
static double lamp_clock(){return g_effect_quality>=2?g_effect_render_time:RC.t;}
static void lamp_history_reset(){mem_zero(g_lamp_history,sizeof g_lamp_history);}
static void lamp_pose_record(const double *cam,const SceneModel &s){
  if(s.car<0)return;LampPose *history=g_lamp_history[s.car];M4 world=m4_id();
  for(int column=0;column<3;column++)for(int k=0;k<3;k++)world.m[column*4+k]=(float)(cam[k+3]*s.mv.m[column*4]+cam[k+6]*s.mv.m[column*4+1]-cam[k+9]*s.mv.m[column*4+2]);
  for(int k=0;k<3;k++)world.m[12+k]=(float)((cam[k]-g_shift[k])*MODEL_SCALE+cam[k+3]*s.mv.m[12]+cam[k+6]*s.mv.m[13]-cam[k+9]*s.mv.m[14]);
  double now=lamp_clock();if(history[0].valid&&(now<history[0].time||now-history[0].time>.25||history[0].model!=RC.car[s.car].car))mem_zero(history,sizeof g_lamp_history[0]);
  if(!history[0].valid||now>history[0].time+.0001)for(int j=(g_effect_quality>=2?31:7);j>0;j--)history[j]=history[j-1];
  history[0]={world,now,1,RC.car[s.car].car};
}
static int lamp_motion_resources(int W,int H);
static void lamp_motion_trails_draw(const double *cam);
static void lamp_trails_draw(const double *cam){
  if(A.no_light_trails)return;if(g_effect_quality>=2&&lamp_motion_resources(LM.w,LM.h)){lamp_motion_trails_draw(cam);return;}glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glUniform1i(R.um_rear_only,1);
  for(int i=0;i<g_scene_count;i++){const SceneModel &s=g_scene[i];if(s.car<0)continue;LampPose *history=g_lamp_history[s.car];scene_damage_uniforms(s);glUniform1f(R.um_brake,RC.car[s.car].braking?1.f:0.f);
    for(int sample=1;sample<=(g_effect_quality==0?4:9);sample++){double age=sample*(g_effect_quality==0?.028:.014),time=RC.t-age;int index=-1;for(int j=1;j<8;j++)if(history[j].valid&&history[j].time<=time&&history[j-1].time>=time){index=j;break;}if(index<0)continue;
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
static int lamp_motion_resources(int W,int H){
  if(g_effect_quality<2||g_tex_nearest||W<2||H<2)return 0;
  if(!LM.trail_program)LM.trail_program=shader_build(
    "#version 330 core\nout vec2 uv;uniform vec4 uRect;void main(){vec2 c=vec2(float(gl_VertexID&1),float((gl_VertexID>>1)&1));vec2 p=mix(uRect.xy,uRect.zw,c);uv=p*0.5+0.5;gl_Position=vec4(p,0.0,1.0);}",
    "#version 330 core\nin vec2 uv;out vec4 o;uniform sampler2D uMask;uniform mat4 uPath[8];uniform int uCount,uSamples;uniform vec2 uHead;vec4 path(int i){return uPath[i/4][i%4];}void main(){vec3 sum=vec3(0.0);float weight=0.0;for(int j=0;j<192;j++){if(j>=uSamples)break;float t=(float(j)+0.5)/float(uSamples)*float(uCount-1);int k=min(int(t),uCount-2);vec4 p=mix(path(k),path(k+1),fract(t));vec2 source=(uv-p.xy)/p.z+uHead;float fade=(1.0-p.w)*(1.0-p.w);if(all(greaterThanEqual(source,vec2(0.0)))&&all(lessThanEqual(source,vec2(1.0))))sum+=texture(uMask,source).rgb*fade;weight+=fade;}vec3 c=sum/max(weight,0.001)*2.2;if(max(c.r,max(c.g,c.b))<0.0002)discard;o=vec4(c,0.0);}");
  if(!LM.trail_program)return 0;
  if(LM.trail_w==W&&LM.trail_h==H&&LM.trail_fbo)return 1;
  if(LM.trail_tex)glDeleteTextures(1,&LM.trail_tex);if(LM.trail_fbo)glDeleteFramebuffers(1,&LM.trail_fbo);
  glGenFramebuffers(1,&LM.trail_fbo);glBindFramebuffer(GL_FRAMEBUFFER,LM.trail_fbo);lamp_texture(&LM.trail_tex,W,H);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,LM.trail_tex,0);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,LM.depth,0);
  int ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;glBindFramebuffer(GL_FRAMEBUFFER,LM.fbo[0]);if(ok){LM.trail_w=W;LM.trail_h=H;}return ok;
}
static V3 lamp_project(V3 eye,const GLint *viewport){
  float w=g_proj.m[3]*eye.x+g_proj.m[7]*eye.y+g_proj.m[11]*eye.z+g_proj.m[15];if(w<=.001f)return v3(-100,-100,0);
  float x=(g_proj.m[0]*eye.x+g_proj.m[4]*eye.y+g_proj.m[8]*eye.z+g_proj.m[12])/w,y=(g_proj.m[1]*eye.x+g_proj.m[5]*eye.y+g_proj.m[9]*eye.z+g_proj.m[13])/w;
  return v3((viewport[0]+(x*.5f+.5f)*viewport[2])/LM.w,(viewport[1]+(y*.5f+.5f)*viewport[3])/LM.h,-eye.z);
}
static V3 lamp_world_project(const double *cam,const M4 &world,V3 local,const GLint *viewport){V3 point=transform_point(world,local);double W[3]={point.x/MODEL_SCALE,point.y/MODEL_SCALE,point.z/MODEL_SCALE};return lamp_project(shadow_eye(cam,W),viewport);}
static int lamp_motion_path(const double *cam,const SceneModel &s,const GLint *viewport,float *path,float *rect,int &samples){
  LampPose *history=g_lamp_history[s.car];if(!history[0].valid||!history[1].valid)return 0;
  const Model &m=*s.model;V3 center=(m.bmin+m.bmax)*.5f,head=lamp_world_project(cam,history[0].world,center,viewport);if(head.z<=0)return 0;
  float lo_x=1,lo_y=1,hi_x=0,hi_y=0;
  for(int corner=0;corner<8;corner++){V3 local=v3(corner&1?m.bmax.x:m.bmin.x,corner&2?m.bmax.y:m.bmin.y,corner&4?m.bmax.z:m.bmin.z),p=lamp_world_project(cam,history[0].world,local,viewport);if(p.z<=0)continue;lo_x=m_min(lo_x,p.x);lo_y=m_min(lo_y,p.y);hi_x=m_max(hi_x,p.x);hi_y=m_max(hi_y,p.y);}
  double now=lamp_clock(),duration=g_effect_quality>=3?.14:.105;int count=g_effect_quality>=3?32:16;float maximum_pixels=0;
  rect[0]=lo_x;rect[1]=lo_y;rect[2]=hi_x;rect[3]=hi_y;
  for(int i=0;i<count;i++){double age=duration*i/(count-1),time=now-age;M4 pose=history[0].world;int index=-1;
    for(int j=1;j<32;j++)if(history[j].valid&&history[j].time<=time&&history[j-1].time>=time){index=j;break;}
    if(index<0){int oldest=0;for(int j=1;j<32&&history[j].valid;j++)oldest=j;pose=history[oldest].world;}
    else{const LampPose &a=history[index],&b=history[index-1];double f=(time-a.time)/(b.time-a.time);for(int k=0;k<16;k++)pose.m[k]=a.world.m[k]+(b.world.m[k]-a.world.m[k])*(float)f;}
    double d=0;for(int k=0;k<3;k++)d+=(pose.m[12+k]-history[0].world.m[12+k])*(pose.m[12+k]-history[0].world.m[12+k]);d=d_sqrt(d);double limit=g_effect_quality>=3?12:9;
    if(d>limit)for(int k=0;k<3;k++)pose.m[12+k]=history[0].world.m[12+k]+(pose.m[12+k]-history[0].world.m[12+k])*(float)(limit/d);
    V3 p=lamp_world_project(cam,pose,center,viewport);if(p.z<=0)p=head;float scale=m_clamp(head.z/p.z,.5f,2.f);
    path[i*4]=p.x;path[i*4+1]=p.y;path[i*4+2]=scale;path[i*4+3]=(float)(age/duration);
    rect[0]=m_min(rect[0],p.x+(lo_x-head.x)*scale);rect[1]=m_min(rect[1],p.y+(lo_y-head.y)*scale);rect[2]=m_max(rect[2],p.x+(hi_x-head.x)*scale);rect[3]=m_max(rect[3],p.y+(hi_y-head.y)*scale);
    float dx=(p.x-head.x)*LM.w,dy=(p.y-head.y)*LM.h;maximum_pixels=m_max(maximum_pixels,m_sqrt(dx*dx+dy*dy));
  }
  if(maximum_pixels<.35f)return 0;samples=(int)m_clamp(maximum_pixels*2+1,g_effect_quality>=3?96.f:48.f,g_effect_quality>=3?192.f:128.f);
  for(int i=0;i<4;i++)rect[i]=m_clamp(rect[i]+(i<2?-2.f:2.f)/(i&1?LM.h:LM.w),0,1)*2-1;return count;
}
static void lamp_motion_trails_draw(const double *cam){
  GLint viewport[4];glGetIntegerv(0x0BA2,viewport);
  for(int i=0;i<g_scene_count;i++){const SceneModel &s=g_scene[i];if(s.car<0||s.car>=8)continue;float path[128]={},rect[4];int samples=0,count=lamp_motion_path(cam,s,viewport,path,rect,samples);if(!count)continue;
    glBindFramebuffer(GL_FRAMEBUFFER,LM.trail_fbo);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_BLEND);
    model_shader_begin(g_proj,1);glUniform1i(R.um_lamps,1);glUniform1i(R.um_lamp_pass,1);glUniform1i(R.um_rear_only,1);scene_damage_uniforms(s);glUniform1f(R.um_brake,RC.car[s.car].braking?1.f:0.f);g_lamp_pass=1;draw_model_mv(*s.model,s.mv,s.wheel,0);
    glBindFramebuffer(GL_FRAMEBUFFER,LM.fbo[0]);glViewport(0,0,LM.w,LM.h);glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glUseProgram(LM.trail_program);glBindVertexArray(R.empty_vao);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,LM.trail_tex);
    glUniform1i(glGetUniformLocation(LM.trail_program,"uMask"),0);glUniform1i(glGetUniformLocation(LM.trail_program,"uCount"),count);glUniform1i(glGetUniformLocation(LM.trail_program,"uSamples"),samples);glUniform2f(glGetUniformLocation(LM.trail_program,"uHead"),path[0],path[1]);glUniform4f(glGetUniformLocation(LM.trail_program,"uRect"),rect[0],rect[1],rect[2],rect[3]);glUniformMatrix4fv(glGetUniformLocation(LM.trail_program,"uPath"),(count+3)/4,0,path);glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  }
  glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);model_shader_begin(g_proj,1);glUniform1i(R.um_lamps,1);glUniform1i(R.um_lamp_pass,1);g_lamp_pass=1;
}
static void vehicle_lights_preload(int W,int H){
  if(g_tex_nearest||!g_vehicle_lights||W<2||H<2)return;
  GLint previous=0,program=0,vao=0,active=GL_TEXTURE0,texture=0;glGetIntegerv(0x8CA6,&previous);glGetIntegerv(0x8B8D,&program);glGetIntegerv(0x85B5,&vao);glGetIntegerv(0x84E0,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(0x8069,&texture);
  if(lamp_resources(W,H)&&g_effect_quality>=2)lamp_motion_resources(W,H);
  glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);glBindTexture(GL_TEXTURE_2D,(GLuint)texture);glActiveTexture((GLenum)active);glBindVertexArray((GLuint)vao);glUseProgram((GLuint)program);
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
