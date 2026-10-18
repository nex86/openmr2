// Sweep the complete flight segment so fast missiles cannot tunnel through small targets.
static double sweep_box(const double a[3],const double b[3],const double center[3],const double R0[3],const double U[3],const double F[3],V3 lo,V3 hi){
  const double *axis[3]={R0,U,F};double lower[3]={lo.x,lo.y,lo.z},upper[3]={hi.x,hi.y,hi.z},enter=0,leave=1;
  for(int k=0;k<3;k++){
    double start=0,delta=0;for(int j=0;j<3;j++){start+=(a[j]-center[j])*axis[k][j];delta+=(b[j]-a[j])*axis[k][j];}
    if(d_abs(delta)<1e-9){if(start<lower[k]||start>upper[k])return 2;continue;}
    double t0=(lower[k]-start)/delta,t1=(upper[k]-start)/delta;if(t0>t1){double t=t0;t0=t1;t1=t;}
    if(t0>enter)enter=t0;if(t1<leave)leave=t1;if(enter>leave)return 2;
  }return enter;
}
static int projectile_road(const PPos &from,const PPos &to,int node){return nodes_connected(from.i,node,12)||nodes_connected(to.i,node,12);}
static void projectile_end(Proj &p){p.alive=0;if(p.kind==W_HITRAX&&p.owner>=0&&p.owner<RC.ncar)RC.car[p.owner].hitrax_live=0;}
static void explosive_detonate(int type,int index,int owner){
  struct Pending{int type,index;};Pending queue[MAXOBJ+48];int count=1;queue[0]={type,index};
  if(type==0)TK.obj[index].alive=0;else RC.haz[index].alive=0;
  for(int at=0;at<count;at++){
    Pending e=queue[at];PPos pp;double x,w[3];
    if(e.type==0){TrackObj &o=TK.obj[e.index];pp=o.pp;x=o.x;for(int k=0;k<3;k++)w[k]=o.W[k];}
    else{Hazard &h=RC.haz[e.index];pp=h.pp;x=h.x;for(int k=0;k<3;k++)w[k]=h.W[k];}
    fx_add(OM_EXP_BIG,pp,x,2200,!g_tex_nearest&&e.type==0?1.4:1.3);sfx_at(e.type==0?S_EXPLO1:S_EXPLO3,w,1);
    if(sys_debug_enabled())sys_log(e.type==0?"[event] Explosive barrel detonated by missile/blast.":"[event] Mine detonated by missile/blast.");
    for(int i=0;i<RC.ncar;i++){RCar &c=RC.car[i];double a,l;rel_to(c,w,&a,&l);double distance=d_sqrt(a*a+l*l);
      if(distance<18000&&same_road_at(c,w,pp.i,NEAR_CONTACT)){damage_car(c,(6666/655.36)*(1-distance/24000),owner);c.stun=0.6;c.v*=0.7;c.vlong*=0.7;}}
    auto near=[&](const double other[3],int node){double d2=0,height=0;for(int k=0;k<3;k++){double d=other[k]-w[k];d2+=d*d;height+=d*RP(pp.i).up[k];}return d2<13000.0*13000&&d_abs(height)<5000&&nodes_connected(pp.i,node,6);};
    for(int i=0;i<TK.nobj;i++){TrackObj &o=TK.obj[i];if(o.alive&&o.type==2&&near(o.W,o.node)){o.alive=0;queue[count++]={0,i};}}
    for(int i=0;i<48;i++){Hazard &h=RC.haz[i];if(h.alive&&h.kind==W_MINE&&near(h.W,h.pp.i)){h.alive=0;queue[count++]={1,i};}}
  }
}
static void projectile_step(Proj &p,double dt){
  if(!p.alive)return;
  if(projectile_model(p.kind)<0||((p.life-=dt)<=0)){projectile_end(p);return;}
  PPos previous=p.pp;double oldx=p.x,start[3];pp_world(p.pp,p.x,2200,start);
  if(p.kind==W_HITRAX){double des=d_atan2(-p.x,60000);p.psi+=d_clamp(des-p.psi,-3*dt,3*dt);}
  double mix=p.kind==W_HITRAX?0.5:0.75,k=1-d_pow(1-mix,dt/SIM_STEP);
  p.vlong+=(p.v*d_cos(p.psi)-p.vlong)*k;p.vlat+=(p.v*d_sin(p.psi)-p.vlat)*k;
  double distance=p.vlong*dt;p.x+=p.vlat*dt;p.psi-=pp_kappa(p.pp)*distance;pp_move(p.pp,p.x,distance);
  double end[3];pp_world(p.pp,p.x,2200,end);
  double best=2;int type=-1,index=-1;
  if(d_abs(p.x)>pp_halfw(p.pp)-1000){best=d_abs(p.x-oldx)>1?d_clamp(((p.x>0?1:-1)*(pp_halfw(p.pp)-1000)-oldx)/(p.x-oldx),0,1):1;type=3;}
  // Original weapons collide in the road plane; vertical padding includes low mines under the flight path.
  const float nose=p.kind==W_HITRAX?3000.f:p.kind==W_BOOMER?2150.f:1650.f;
  auto candidate=[&](int target,int idx,const double pos[3],const double r[3],const double up[3],const double fw[3],V3 lo,V3 hi){
    lo=lo-v3(1000,2000,nose);hi=hi+v3(1000,2000,nose);double t=sweep_box(start,end,pos,r,up,fw,lo,hi);if(t<best){best=t;type=target;index=idx;}};
  for(int i=0;i<TK.nobj;i++){
    const TrackObj &o=TK.obj[i];if(!o.alive||o.type!=2||!projectile_road(previous,p.pp,o.node))continue;
    const Model *m=TK.obj_models[OM_BARREL];double pos[3],r[3],up[3],fw[3];obj_pose(o,pos,r,up,fw);
    candidate(0,i,pos,r,up,fw,m?m->bmin*(1/MODEL_SCALE):v3(-3000,0,-3000),m?m->bmax*(1/MODEL_SCALE):v3(3000,6400,3000));
  }
  for(int i=0;i<48;i++){
    const Hazard &h=RC.haz[i];if(!h.alive||h.kind!=W_MINE||!projectile_road(previous,p.pp,h.pp.i))continue;
    double pos[3],r[3],up[3],fw[3];pose_of(h.pp,h.x,0,0,pos,r,up,fw);const Model *m=TK.obj_models[OM_MINE];
    candidate(1,i,pos,r,up,fw,m?m->bmin*(1/MODEL_SCALE):v3(-2000,0,-2000),m?m->bmax*(1/MODEL_SCALE):v3(2000,500,2000));
  }
  for(int i=0;i<RC.ncar;i++){
    const RCar &c=RC.car[i];if(i==p.owner||c.out||!projectile_road(previous,p.pp,c.pp.i))continue;
    double pos[3],r[3],up[3],fw[3];pose_of(c.pp,c.x,0,c.psi,pos,r,up,fw);candidate(2,i,pos,r,up,fw,v3(-6500,0,-9800),v3(6500,6500,9800));
  }
  if(best>1)return;
  p.pp=previous;p.x=oldx+p.vlat*dt*best;pp_move(p.pp,p.x,distance*best);projectile_end(p);
  if(type==0||type==1)explosive_detonate(type,index,p.owner);
  else if(type==2){RCar &c=RC.car[index];damage_car(c,WEAPONS[p.kind].damage,p.owner);c.stun=0.6;c.vlong*=0.7;c.v*=0.7;c.psi+=frnd(-0.3,0.3);fx_add(OM_EXP_MID,p.pp,p.x,1500,1.2);sfx_at(S_EXPLO2,c.W,1);}
  else{double w[3];pp_world(p.pp,p.x,1500,w);fx_add(OM_EXP_SMALL,p.pp,p.x,1500,1);sfx_at(S_EXPLO1,w,0.7f);}
}
