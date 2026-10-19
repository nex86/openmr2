// Stored two-dimensional road frames, staged movement and contacts.
// Validated against the complete locally executed DOS update loop.
static int32_t g_drive_trig[1280];static int g_drive_trig_ready;
static void drive_trig_load(){
  if(g_drive_trig_ready)return;g_drive_trig_ready=1;char path[512];game_path(path,"DATA640/TABLEAU.COS");size_t n=0;u8 *data=sys_read_file(path,&n);
  if(data&&n==sizeof g_drive_trig)mem_cpy(g_drive_trig,data,n);else for(int i=0;i<1280;i++)g_drive_trig[i]=contact_raw(d_cos(i*6.2831853071795864769/1024)*32768);sys_free(data);
}
static int road_plane_branch(int node,int next){return RP(node).nx[1]==next&&RP(node).nx[0]!=next?1:0;}
static int physics_length(int node,int next){int length=RP(node).physics_length[road_plane_branch(node,next)];return length>0?length:contact_raw(seg_len(node,next));}
struct PhysicsRoadFrame{OriginalPlane pose;int node,group,next[2],prev[2],flags;};
static PhysicsRoadFrame g_road_plane_frames[4096];static int g_road_plane_count,g_road_plane_car[8],g_road_plane_head[8],g_road_plane_link[8],g_road_plane_commit;
static int road_plane_add(int group,int node,OriginalPlane pose,int flags){if(g_road_plane_count>=4096)return -1;int i=g_road_plane_count++;g_road_plane_frames[i]={pose,node,group,{-1,-1},{-1,-1},flags};return i;}
static int road_plane_bind(int frame){
  int found=0;int order[8];for(int i=0;i<RC.ncar;i++)order[i]=i;
  for(int i=0;i<RC.ncar;i++)for(int j=i+1;j<RC.ncar;j++)if(RC.car[order[i]].fixed_commit<RC.car[order[j]].fixed_commit||(RC.car[order[i]].fixed_commit==RC.car[order[j]].fixed_commit&&order[i]<order[j])){int t=order[i];order[i]=order[j];order[j]=t;}
  for(int k=0;k<RC.ncar;k++){int i=order[k];if(g_road_plane_car[i]<0&&RC.car[i].pp.i==g_road_plane_frames[frame].node){g_road_plane_car[i]=frame;found=1;int group=g_road_plane_frames[frame].group;g_road_plane_link[i]=g_road_plane_head[group];g_road_plane_head[group]=i;}}
  return found;
}
static void road_plane_forward(int frame,int depth,int path=0,bool dispatch=true);
static void road_plane_backward(int frame,int depth,int path=0){
  // Backward traversal examines the destination junction before allocating its
  // frame. Its secondary arm resumes after the primary arm, with a fresh horizon.
  while(depth>0&&g_road_plane_count<4000){
    PhysicsRoadFrame f=g_road_plane_frames[frame];int node=RP(f.node).pv[path],flags=((f.flags<<3)|(RP(node).kind&7))&63;
    int branch=path;bool split=(flags==6||flags==54),fork_alt=flags==29,fork_main=flags==37;
    if(split||fork_alt||fork_main||flags==45)depth=11;
    if(fork_main)branch=1;
    const RoadPt &r=RP(node);OriginalPlane pose={0,0,(f.pose.heading-r.physics_rotation[branch])&1023};
    int dir=(pose.heading+r.physics_turn[branch])&1023,len=r.physics_length[branch];
    pose.x=f.pose.x+(op_product(len,g_drive_trig[dir+256])>>15);pose.z=f.pose.z+(op_product(-len,g_drive_trig[dir])>>15);
    int next=road_plane_add(f.group,node,pose,flags);if(next<0)return;
    g_road_plane_frames[frame].prev[path]=next;g_road_plane_frames[next].next[0]=frame;
    if(flags==37){g_road_plane_frames[frame].prev[0]=next;g_road_plane_frames[next].next[1]=frame;}
    depth=(road_plane_bind(next)?11:depth)-1;
    if(split||fork_alt||fork_main){road_plane_backward(next,depth);if(split)road_plane_backward(next,11,1);else road_plane_forward(next,11,fork_alt?1:0,false);return;}
    frame=next;path=0;
  }
}
static void road_plane_forward(int frame,int depth,int path,bool dispatch){
  // Preserve traversal order and duplicate unfolded nodes. Merging these frames
  // by PST node changes integer rounding and therefore the steering feedback.
  while(depth>0&&g_road_plane_count<4000){
    PhysicsRoadFrame f=g_road_plane_frames[frame];int flags=f.flags;
    if(dispatch&&(flags==5||flags==45||flags==14||flags==22||flags==54)){
      road_plane_forward(frame,11,0,false);
      if(flags==5||flags==45)road_plane_forward(frame,11,1,false);else road_plane_backward(frame,11,flags==22?0:1);
      return;
    }
    const RoadPt &r=RP(f.node);int node=r.nx[path],dir=(f.pose.heading+r.physics_turn[path])&1023,len=r.physics_length[path];
    OriginalPlane pose={f.pose.x+(op_product(-len,g_drive_trig[dir+256])>>15),f.pose.z+(op_product(len,g_drive_trig[dir])>>15),(f.pose.heading+r.physics_rotation[path])&1023};
    int next=road_plane_add(f.group,node,pose,((flags<<3)|(RP(node).kind&7))&63);if(next<0)return;
    if(flags==22)g_road_plane_frames[frame].prev[1]=g_road_plane_frames[frame].prev[0];
    g_road_plane_frames[frame].next[path]=next;g_road_plane_frames[next].prev[0]=frame;
    depth=(road_plane_bind(next)?11:depth)-1;frame=next;path=0;dispatch=true;
  }
}
static void road_plane_frames_build(){
  drive_trig_load();g_road_plane_count=0;for(int i=0;i<8;i++)g_road_plane_car[i]=g_road_plane_head[i]=g_road_plane_link[i]=-1;
  for(int i=0;i<RC.ncar;i++)if(g_road_plane_car[i]<0){int kind=RP(RC.car[i].pp.i).kind&7;int root=road_plane_add(i,RC.car[i].pp.i,{0,0,0},kind*9);road_plane_bind(root);road_plane_forward(root,11);road_plane_backward(root,11);}
}
static void road_plane_world(OriginalPlane f,int x,int z,int &wx,int &wz){int c=g_drive_trig[f.heading],s=g_drive_trig[f.heading+256];wx=f.x+((int32_t)((uint32_t)op_product(c,x)-(uint32_t)op_product(s,z))>>15);wz=f.z+((int32_t)((uint32_t)op_product(c,z)+(uint32_t)op_product(s,x))>>15);}
static int road_plane_after(OriginalPlane f,int node,int x,int z){int w=contact_raw(RP(node).w),dx=(int)(((int64_t)w*g_drive_trig[f.heading])>>15),dz=(int)(((int64_t)w*g_drive_trig[f.heading+256])>>15);return op_product(f.x+dx-x,f.z-dz-z)<=op_product(f.z+dz-z,f.x-dx-x);}
static int road_plane_side(OriginalPlane a,OriginalPlane b,int x,int z){
  int first=op_product(a.x-x,b.z-z),second=op_product(a.z-z,b.x-x);
  return first<second?-1:first>second?1:0;
}
static int road_plane_outgoing(int index,int x,int z){
  const PhysicsRoadFrame &f=g_road_plane_frames[index];
  if((f.flags&7)==5&&f.prev[0]>=0&&road_plane_side(f.pose,g_road_plane_frames[f.prev[0]].pose,x,z)>0)return 1;
  return 0;
}
static int road_plane_select(int index,int x,int z){
  bool forward=road_plane_after(g_road_plane_frames[index].pose,g_road_plane_frames[index].node,x,z);
  int last=index;
  for(int limit=0;limit<64;limit++){
    const PhysicsRoadFrame &f=g_road_plane_frames[index];bool after=road_plane_after(f.pose,f.node,x,z);
    if(forward){
      if(!after)return last;last=index;
      int next=f.next[road_plane_outgoing(index,x,z)];if(next<0)return index;index=next;
    }else{
      if(after)return index;int branch=0;
      if((f.flags&7)==6&&f.next[0]>=0&&road_plane_side(f.pose,g_road_plane_frames[f.next[0]].pose,x,z)<0)branch=1;
      int previous=f.prev[branch];if(previous<0)return index;index=previous;
    }
  }
  return last;
}
static OriginalBody g_original_body[8];static int g_original_old_angle[8],g_original_defer;static int g_original_saved_yaw_a,g_original_saved_yaw_b;
static void original_car_finish(RCar &c,double dt);
static void original_car_drive(RCar &c,int drive,double steer,double dt){
  drive_trig_load();OriginalDrive &s=c.fixed;int raw=contact_raw(c.v/V_RAW);
  if(!c.fixed_ready){mem_zero(&s,sizeof s);s.motor=s.speed=raw;s.vx=s.old_x=contact_raw(c.vlat/V_RAW);s.vz=s.old_z=contact_raw(c.vlong/V_RAW);s.oil=-1;c.fixed_ready=1;}
  if(raw!=s.speed)s.motor=s.speed=raw;
  int vx=contact_raw(c.vlat/V_RAW),vz=contact_raw(c.vlong/V_RAW);
  if(vx!=s.old_x)s.vx=s.old_x=vx;if(vz!=s.old_z)s.vz=s.old_z=vz;
  s.steer=-contact_raw(c.steer_st);s.angle=contact_raw(-c.psi*1024/6.2831853071795864769)&1023;s.autopilot=c.autopilot;
  if(c.oil_t>(s.oil>0?s.oil*.04:0)+.001)s.oil=contact_raw(c.oil_t/.04);
  const DriveProfile &p=PROFILES[c.profile>=1&&c.profile<=3?c.profile:1];
  const int32_t profile[17]={p.vitturn,-p.vitturn,p.anglmax,p.vitret,p.derplat,p.derpfrn,p.slip1,p.slip2,p.corr1,p.corr2,p.rotlim,p.rotderp,400,-200,-500,128,-128};
  g_original_old_angle[&c-RC.car]=s.angle;
  original_drive_step(s,profile,-contact_raw(d_clamp(steer,-1,1)*128),drive>0?400:drive<0?-500:-200,contact_raw(dt*51200),c.player,c.tactic_active==T_BOOSTER,g_drive_trig,g_drive_trig+256);
  // The original phase uses 1024 units per turn. Keep its signed, unwrapped
  // increment for rendering: a single update can exceed half a wheel rotation,
  // so recovering it from the wrapped phase would reverse/slow the animation.
  c.wheel+=od_wheel_increment(s.motor,contact_raw(dt*51200))*6.2831853071795864769/1024;
}
static void original_car_move(RCar &c,double dt){
  drive_trig_load();OriginalDrive &s=c.fixed;
  int along=contact_raw(c.pp.t*physics_length(c.pp.i,c.pp.nx));
  int index=(int)(&c-RC.car);if(index<0||index>=RC.ncar)index=0;
  if(g_road_plane_car[index]<0||g_road_plane_frames[g_road_plane_car[index]].node!=c.pp.i)road_plane_frames_build();
  int current_frame=g_road_plane_car[index];OriginalPlane initial=g_road_plane_frames[current_frame].pose;
  int x,z;road_plane_world(initial,contact_raw(c.x),along,x,z);
  int world_vx,world_vz;road_plane_world({0,0,initial.heading},s.vx,s.vz,world_vx,world_vz);
  x+=(int)(((int64_t)world_vx*contact_raw(dt*51200))>>8);z+=(int)(((int64_t)world_vz*contact_raw(dt*51200))>>8);
  OriginalBody &body=g_original_body[index];mem_zero(&body,sizeof body);
  road_plane_world(initial,contact_raw(c.x),along,body.old[0].x,body.old[0].z);body.next[0]={x,z};
  body.heading=(s.angle-initial.heading)&1023;body.road_heading=initial.heading;
  ob_shape(body.old,(g_original_old_angle[index]-initial.heading)&1023,g_drive_trig,g_drive_trig+256);ob_shape(body.next,body.heading,g_drive_trig,g_drive_trig+256);
  body.world_vx=world_vx;body.world_vz=world_vz;body.vx=s.vx;body.vz=s.vz;body.motor=s.motor;body.yaw=s.yaw;body.slip=s.slip;
  if(!g_original_defer)original_car_finish(c,dt);
}
static void original_car_finish(RCar &c,double dt){
  int index=&c-RC.car;OriginalDrive &s=c.fixed;OriginalBody &body=g_original_body[index];
  int current_frame=g_road_plane_car[index];OriginalPlane initial=g_road_plane_frames[current_frame].pose;
  int x=body.next[0].x,z=body.next[0].z;
  s.vx=body.vx;s.vz=body.vz;s.motor=body.motor;s.yaw=body.yaw;s.slip=body.slip;
  int selected=road_plane_select(current_frame,x,z);OriginalPlane frame=g_road_plane_frames[selected].pose;
  int old_node=c.pp.i;c.pp.i=g_road_plane_frames[selected].node;
  c.pp.nx=RP(c.pp.i).nx[road_plane_outgoing(selected,x,z)];
  if(c.player&&old_node!=c.pp.i){
    int trace=current_frame,previous=old_node;
    if(road_plane_after(initial,old_node,x,z))for(int limit=0;limit<64&&trace!=selected;limit++){
      int next=g_road_plane_frames[trace].next[road_plane_outgoing(trace,x,z)];if(next<0)break;
      int node=g_road_plane_frames[next].node;on_player_enter(previous,node);previous=node;trace=next;
    }
  }
  int local_x,local_z;original_plane_local(frame,x,z,local_x,local_z,g_drive_trig,g_drive_trig+256);
  int rotation=(frame.heading-initial.heading)&1023;s.angle=(s.angle+rotation)&1023;
  original_plane_local({0,0,rotation},s.vx,s.vz,s.old_x,s.old_z,g_drive_trig,g_drive_trig+256);
  double impact=(local_x>=0?1:-1)*s.vx*V_RAW;
  int width=contact_raw(RP(c.pp.i).w),contact=od_abs(local_x)>=width-ORIGINAL_HULL_EXTENT[s.angle];
  if(contact)original_wall_step(s,local_x,width,c.player);
  c.x=local_x;c.pp.t=local_z/(double)physics_length(c.pp.i,c.pp.nx);
  c.v=s.speed*V_RAW;c.vlat=s.old_x*V_RAW;c.vlong=s.old_z*V_RAW;c.steer_st=-s.steer;c.psi=-s.angle*6.2831853071795864769/1024;
  if(c.psi< -3.141592653589793)c.psi+=6.2831853071795864769;c.yaw=-s.yaw*6.2831853071795864769/1024;c.oil_t=s.oil>0?s.oil*.04:0;
  c.wall_scraping=contact&&!hard_wall_impact(impact)&&impact>=0&&!c.out&&d_abs(c.vlong)>.03*V_PLAYER;
  if(contact&&hard_wall_impact(impact)&&c.wall_cd<=0){damage_car(c,impact/V_PLAYER*3,-1);car_update_world(c);impact_sound(c.W,.9f);c.wall_cd=.4;}
  if(contact&&impact>0&&g_damage_effects&&!g_tex_nearest&&c.spark_cd<=0){
    car_update_world(c);double w[3],velocity[3];double side=c.x>=0?1:-1;
    for(int k=0;k<3;k++){w[k]=c.W[k]+c.Rt[k]*side*c.hwm+c.U[k]*600;velocity[k]=-c.F[k]*c.vlong*.18-c.Rt[k]*side*impact*.18;}
    contact_sparks(w,c.U,velocity,(float)d_clamp(impact/V_PLAYER,.15,1));c.spark_cd=.07;
  }
  if(c.player){hist_sync();RC.ply_D+=s.vz*V_RAW*dt;}
  double M=pp_M(c.pp);c.prog+=wrapM(M-c.lastM);c.lastM=M;c.fixed_commit=++g_road_plane_commit;
  car_update_world(c);
  if(c.spark_cd>0)c.spark_cd-=dt;
  if(c.wall_scraping&&g_damage_effects&&(!g_tex_nearest)&&c.spark_cd<=0){double w[3],velocity[3];for(int k=0;k<3;k++){w[k]=c.W[k]+c.Rt[k]*(c.x>=0?c.hwm:-c.hwm)+c.U[k]*600;velocity[k]=-c.F[k]*c.vlong*.32-c.Rt[k]*(c.x>=0?1:-1)*3500;}contact_sparks(w,c.U,velocity,(float)d_clamp(d_abs(c.vlong)/V_PLAYER,.1,1),1);c.spark_cd=.035;}
  if (c.wall_cd > 0) c.wall_cd -= dt;
  if (c.stun > 0) c.stun -= dt;
  if (c.flash > 0) c.flash -= dt;
  if (c.hit_cd > 0) c.hit_cd -= dt;
  if (c.front_cd > 0) c.front_cd -= cnt_dt(dt);
  if (c.boomer_cd > 0) c.boomer_cd -= cnt_dt(dt);
  if (c.rear_cd > 0) c.rear_cd -= cnt_dt(dt);
}

static void original_contacts(double dt){
  double damage[8]={};
  for(int group=0;group<RC.ncar;group++)if(g_road_plane_head[group]>=0){
#ifndef OPENMR2_REFERENCE_NO_CAR_CONTACT
    for(int pass=0;pass<16;pass++){
      int best=-1,hit_a=-1,hit_b=-1,ids=0;
      for(int a=g_road_plane_head[group];a>=0;a=g_road_plane_link[a])for(int b=g_road_plane_link[a];b>=0;b=g_road_plane_link[b]){
        int time;int contact=ob_detect(g_original_body[a],g_original_body[b],contact_raw(dt*51200),time);
        if(contact&&(best<0||time<best)){best=time;hit_a=a;hit_b=b;ids=contact;}
      }
      if(hit_a<0)break;
      RCar &a=RC.car[hit_a],&b=RC.car[hit_b];OriginalBody &ba=g_original_body[hit_a],&bb=g_original_body[hit_b];
      int dx=ba.vx-bb.vx,dz=ba.vz-bb.vz;int relative2=dx*dx+dz*dz;
      if(g_damage_effects&&!g_tex_nearest&&relative2>1&&(a.spark_cd<=0||b.spark_cd<=0)){
        double w[3],velocity[3];for(int k=0;k<3;k++){w[k]=(a.W[k]+b.W[k])*.5+a.U[k]*1000;velocity[k]=(a.Rt[k]*dx+a.F[k]*dz)*V_RAW*.18;}
        contact_sparks(w,a.U,velocity,(float)d_clamp(d_sqrt((double)relative2)*V_RAW/V_PLAYER,.12,1));a.spark_cd=b.spark_cd=.065;
      }
      int magnitude=ob_respond(ba,bb,ids,g_drive_trig,g_drive_trig+256,g_original_saved_yaw_a,g_original_saved_yaw_b);
      if(!a.out&&!b.out){double loss=((magnitude>>3)*16)/655.36;damage[hit_a]+=loss;damage[hit_b]+=loss;}
      if(relative2>=0x7300&&(a.hit_cd<=0||b.hit_cd<=0)){impact_sound(a.player?a.W:b.W,.9f);a.hit_cd=b.hit_cd=.35;}
    }
#endif
    for(int i=g_road_plane_head[group];i>=0;i=g_road_plane_link[i])original_car_finish(RC.car[i],dt);
  }
  for(int i=0;i<RC.ncar;i++)if(damage[i]>0)damage_car(RC.car[i],damage[i],-1);
}

static void original_motion_reset(){
  g_road_plane_commit=0;g_road_plane_count=0;g_original_defer=0;g_original_saved_yaw_a=g_original_saved_yaw_b=0;
  for(int i=0;i<8;i++)g_road_plane_car[i]=g_road_plane_head[i]=g_road_plane_link[i]=-1;
}
