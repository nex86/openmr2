// One command list supplies identical interpolated poses to the camera and light passes.
struct SceneModel {const Model *model;M4 mv;float wheel;int oil,car;};
static SceneModel g_scene[192];static int g_scene_count;
static void scene_add(const Model &model,const M4 &mv,float wheel,int car){
  if(g_scene_count<192)g_scene[g_scene_count++]={&model,mv,wheel,&model==TK.obj_models[OM_OIL],car-1};
}
static void scene_damage_uniforms(const SceneModel &s){
  float damage=s.car>=0&&g_damage_effects&&!(g_tex_nearest)?(float)(RC.car[s.car].damage/100.0):0;
  if(damage<1)damage=m_min(damage,.9999f);
  glUniform1f(R.um_damage,damage);if(damage>=.7f){V3 center=(s.model->bmin+s.model->bmax)*.5f,extent=(s.model->bmax-s.model->bmin)*.5f;glUniform3f(R.um_damage_center,center.x,center.y,center.z);glUniform3f(R.um_damage_extent,m_max(extent.x,.01f),m_max(extent.y,.01f),m_max(extent.z,.01f));}
}
static V3 transform_point(const M4 &m,V3 p){return v3(m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z+m.m[12],m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z+m.m[13],m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z+m.m[14]);}
static M4 shadow_ortho(V3 lo,V3 hi){M4 m=m4_id();m.m[0]=2/(hi.x-lo.x);m.m[5]=2/(hi.y-lo.y);m.m[10]=-2/(hi.z-lo.z);m.m[12]=-(hi.x+lo.x)/(hi.x-lo.x);m.m[13]=-(hi.y+lo.y)/(hi.y-lo.y);m.m[14]=(hi.z+lo.z)/(hi.z-lo.z);return m;}
struct ShadowResources{GLuint fbo,depth,color,road_program,vao,vbo;int failed;M4 matrix;V3 light;};
static ShadowResources SH;
static int shadow_resources(){
  if(SH.failed)return 0;if(SH.fbo)return 1;
  GLint previous=0;glGetIntegerv(0x8CA6,&previous);
  glGenFramebuffers(1,&SH.fbo);glBindFramebuffer(GL_FRAMEBUFFER,SH.fbo);
  glGenTextures(1,&SH.depth);glBindTexture(GL_TEXTURE_2D,SH.depth);
  glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,2048,2048,0,0x1902,GL_UNSIGNED_INT,0);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,SH.depth,0);
  glGenTextures(1,&SH.color);glBindTexture(GL_TEXTURE_2D,SH.color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2048,2048,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,SH.color,0);
  int ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);
  SH.road_program=shader_build("#version 330 core\nlayout(location=0) in vec3 aPos;uniform mat4 uVP,uShadowVP;out vec4 vShadow;void main(){vec4 p=vec4(aPos,1.0);vShadow=uShadowVP*p;gl_Position=uVP*p;}\n",
    "#version 330 core\nin vec4 vShadow;out vec4 o;uniform float uShadowStrength;\n" SHADOW_SAMPLE_GLSL "void main(){float s=shadowCoverage(vShadow,0.00012);if(s<=0.0)discard;o=vec4(0.018,0.025,0.04,min(0.92,s*0.46*uShadowStrength));}\n");
  if(!ok||!SH.road_program){SH.failed=1;sys_log("[error] Real-time shadows unavailable; retaining original model shadows.");return 0;}
  glGenVertexArrays(1,&SH.vao);glGenBuffers(1,&SH.vbo);glBindVertexArray(SH.vao);glBindBuffer(GL_ARRAY_BUFFER,SH.vbo);
  glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),0);glBindVertexArray(0);
  sys_log("[event] Real-time shadow map created (2048x2048; filtered vehicle/object shadows).");return 1;
}
static V3 shadow_eye(const double *cam,const double p[3],int shifted=1){double d[3];for(int k=0;k<3;k++)d[k]=p[k]+(shifted?g_shift[k]:0)-cam[k];return v3((float)((d[0]*cam[3]+d[1]*cam[4]+d[2]*cam[5])*MODEL_SCALE),(float)((d[0]*cam[6]+d[1]*cam[7]+d[2]*cam[8])*MODEL_SCALE),(float)(-(d[0]*cam[9]+d[1]*cam[10]+d[2]*cam[11])*MODEL_SCALE));}
// The shop contains no DOS drop shadow. Revived uses the same depth-map
// projection and intensity as the race, with the preview pose as its caster.
static void preview_shadow_draw(const Model &model,const M4 &vp,const M4 &pose,float wheel){
  if(g_tex_nearest||!g_realtime_shadows||!shadow_resources())return;
  GLint previous=0,viewport[4];glGetIntegerv(0x8CA6,&previous);glGetIntegerv(0x0BA2,viewport);
  float radius=m_sqrt(dot(model.bmax-model.bmin,model.bmax-model.bmin))*.6f;
  V3 light=norm(v3(.45f,.85f,.35f));M4 view=m4_look(light*radius*4,v3(0,0,0),v3(0,1,0));
  V3 lo=v3(-radius,-radius,-radius*6),hi=v3(radius,radius,0);
  M4 light_vp=m4_mul(shadow_ortho(lo,hi),view);
  glActiveTexture(GL_TEXTURE0+4);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);
  glBindFramebuffer(GL_FRAMEBUFFER,SH.fbo);glViewport(0,0,2048,2048);glDepthMask(GL_TRUE);glColorMask(0,0,0,0);glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1.5f,2);
  model_shader_begin(light_vp,1);glUniform1i(R.um_shadow_pass,1);g_shadow_pass=1;draw_model_mv(model,pose,wheel,0);g_shadow_pass=0;
  glDisable(GL_POLYGON_OFFSET_FILL);glColorMask(1,1,1,1);glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
  glActiveTexture(GL_TEXTURE0+4);glBindTexture(GL_TEXTURE_2D,SH.depth);glActiveTexture(GL_TEXTURE0);
  glUseProgram(SH.road_program);glUniformMatrix4fv(glGetUniformLocation(SH.road_program,"uVP"),1,0,vp.m);glUniformMatrix4fv(glGetUniformLocation(SH.road_program,"uShadowVP"),1,0,light_vp.m);
  glUniform1i(glGetUniformLocation(SH.road_program,"uShadowMap"),4);glUniform1i(glGetUniformLocation(SH.road_program,"uShadowEnabled"),1);glUniform1f(glGetUniformLocation(SH.road_program,"uShadowStrength"),g_effect_strength[0]);
  V3 floor[6]={v3(-radius,0,-radius),v3(radius,0,-radius),v3(-radius,0,radius),v3(radius,0,-radius),v3(radius,0,radius),v3(-radius,0,radius)};
  glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glBindVertexArray(SH.vao);glBindBuffer(GL_ARRAY_BUFFER,SH.vbo);glBufferData(GL_ARRAY_BUFFER,sizeof floor,floor,GL_DYNAMIC_DRAW);glDrawArrays(GL_TRIANGLES,0,6);
  glDisable(GL_BLEND);glDepthMask(GL_TRUE);glActiveTexture(GL_TEXTURE0);
}
static V3 shadow_axis(const double *cam,const double p[3]){return v3((float)(p[0]*cam[3]+p[1]*cam[4]+p[2]*cam[5]),(float)(p[0]*cam[6]+p[1]*cam[7]+p[2]*cam[8]),(float)(-p[0]*cam[9]-p[1]*cam[10]-p[2]*cam[11]));}
static V3 shadow_road[256*2*4*6];static int shadow_road_count;
static void shadow_road_build(const double *cam){
  shadow_road_count=0;
  for(int n=0;n<g_route_n;n++){
    int node=g_route[n];if(!v3d_visible(RC.frame,node))continue;const RoadPt &r=RP(node);
    for(int branch=0;branch<2;branch++){if(branch&&r.nx[1]==r.nx[0])continue;
      for(int step=0;step<4;step++){
        V3 corners[4];for(int end=0;end<2;end++){
          PPos pp={node,r.nx[branch],(step+end)*0.25};double p[3],rt[3],up[3],fw[3],w;pp_frame(pp,p,rt,up,fw,&w);
          for(int side=0;side<2;side++){double v[3];for(int k=0;k<3;k++)v[k]=p[k]+rt[k]*w*(side?1:-1)+up[k]*40;corners[end*2+side]=shadow_eye(cam,v);}
        }
        if(corners[0].z>-2||corners[2].z>-2||corners[0].z<-90*PT*MODEL_SCALE)continue;
        const int indices[6]={0,1,2,1,3,2};for(int j=0;j<6;j++)shadow_road[shadow_road_count++]=corners[indices[j]];
      }
    }
  }
}
#include "g_lights.inc"
static void scene_render(const double *cam,int W,int H){
  g_shadow_active=g_shadow_pass=g_oil_render=0;
  int enabled=!(g_tex_nearest)&&g_realtime_shadows&&g_scene_count&&shadow_resources();
  if(enabled){
    V3 center=shadow_eye(cam,player().W),up=shadow_axis(cam,player().F);
    double light_world[3];for(int k=0;k<3;k++)light_world[k]=player().U[k]*0.88+player().Rt[k]*0.42+player().F[k]*0.25;
    SH.light=norm(shadow_axis(cam,light_world));M4 view=m4_look(center+SH.light*1500,center,up);
    V3 lo=v3(1e9f,1e9f,1e9f),hi=v3(-1e9f,-1e9f,-1e9f);
    auto bound=[&](V3 p){p=transform_point(view,p);lo=v3(m_min(lo.x,p.x),m_min(lo.y,p.y),m_min(lo.z,p.z));hi=v3(m_max(hi.x,p.x),m_max(hi.y,p.y),m_max(hi.z,p.z));};
    for(int i=0;i<g_scene_count;i++){const SceneModel &s=g_scene[i];for(int corner=0;corner<8;corner++){V3 p=v3(corner&1?s.model->bmax.x:s.model->bmin.x,corner&2?s.model->bmax.y:s.model->bmin.y,corner&4?s.model->bmax.z:s.model->bmin.z);bound(transform_point(s.mv,p));}}
    shadow_road_build(cam);for(int i=0;i<shadow_road_count;i++)bound(shadow_road[i]);
    lo=lo-v3(12,12,25);hi=hi+v3(12,12,25);SH.matrix=m4_mul(shadow_ortho(lo,hi),view);
    GLint previous=0,viewport[4];glGetIntegerv(0x8CA6,&previous);glGetIntegerv(0x0BA2,viewport);
    glActiveTexture(GL_TEXTURE0+4);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER,SH.fbo);glViewport(0,0,2048,2048);glDepthMask(GL_TRUE);glColorMask(0,0,0,0);glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1.5f,2);
    model_shader_begin(SH.matrix,1);glUniform1i(R.um_shadow_pass,1);g_shadow_pass=1;
    for(int i=0;i<g_scene_count;i++){SceneModel &s=g_scene[i];if(!s.oil){scene_damage_uniforms(s);draw_model_mv(*s.model,s.mv,s.wheel,0);}}
    g_shadow_pass=0;glDisable(GL_POLYGON_OFFSET_FILL);glColorMask(1,1,1,1);glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glActiveTexture(GL_TEXTURE0+4);glBindTexture(GL_TEXTURE_2D,SH.depth);glActiveTexture(GL_TEXTURE0);
    glUseProgram(SH.road_program);glUniform1f(glGetUniformLocation(SH.road_program,"uShadowStrength"),g_effect_strength[0]);glUniformMatrix4fv(glGetUniformLocation(SH.road_program,"uVP"),1,0,g_proj.m);glUniformMatrix4fv(glGetUniformLocation(SH.road_program,"uShadowVP"),1,0,SH.matrix.m);
    glUniform1i(glGetUniformLocation(SH.road_program,"uShadowMap"),4);glUniform1i(glGetUniformLocation(SH.road_program,"uShadowEnabled"),1);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glBindVertexArray(SH.vao);glBindBuffer(GL_ARRAY_BUFFER,SH.vbo);
    glBufferData(GL_ARRAY_BUFFER,shadow_road_count*sizeof(V3),shadow_road,GL_DYNAMIC_DRAW);glDrawArrays(GL_TRIANGLES,0,shadow_road_count);
    glDisable(GL_BLEND);glDepthMask(GL_TRUE);g_shadow_active=1;
  }
  model_shader_begin(g_proj,RC.gain);
  if(g_shadow_active){glUniformMatrix4fv(R.um_shadow_vp,1,0,SH.matrix.m);glUniform1i(R.um_shadow_enabled,1);glUniform1i(R.um_shadow_map,4);glUniform3f(R.um_light,SH.light.x,SH.light.y,SH.light.z);}
  for(int i=0;i<g_scene_count;i++){SceneModel &s=g_scene[i];scene_damage_uniforms(s);glUniform1i(R.um_lamps,g_vehicle_lights&&!(g_tex_nearest)&&s.car>=0);glUniform1f(R.um_brake,s.car>=0&&RC.car[s.car].braking?1.f:0.f);g_oil_render=!(g_tex_nearest)&&g_enhanced_oil&&s.oil;glUniform1i(R.um_oil,g_oil_render);
    if(g_oil_render){V3 center=(s.model->bmin+s.model->bmax)*0.5f,half=(s.model->bmax-s.model->bmin)*0.5f;glUniform4f(R.um_oil_bounds,center.x,center.z,m_max(half.x,0.001f),m_max(half.z,0.001f));}
    int old_shadow=g_shadow_active;if(s.oil)g_shadow_active=0;draw_model_mv(*s.model,s.mv,s.wheel,0);g_shadow_active=old_shadow;
  }
  glUniform1i(R.um_lamps,0);glUniform1i(R.um_shadow_enabled,0);glUniform1i(R.um_oil,0);g_shadow_active=g_oil_render=0;glActiveTexture(GL_TEXTURE0);
  vehicle_lights_draw(cam,W,H);model_shader_begin(g_proj,RC.gain);
}
