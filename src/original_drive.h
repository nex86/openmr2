// Independently expressed fixed-point driving rules, checked against locally
// executed original routines. No executable bytes or translated instruction
// stream are embedded here. Time: 51200 units/s; angle: 1024 units/turn.
#pragma once
#include <stdint.h>
struct OriginalDrive {
  int32_t speed,motor,steer,angle,yaw,old_yaw,slip,ax,az,vx,vz,old_x,old_z;
  int32_t oil,bypass,wheel,slip_frames,oil_frames,autopilot;
};
static int32_t od_mul(int32_t a,int32_t b,int bits){return (int32_t)(((int64_t)a*b)>>bits);}
static int32_t od_div(int64_t a,int32_t b){return b?(int32_t)(a/b):0;}
static int32_t od_abs(int32_t a){return a<0?-a:a;}
static int32_t od_clamp(int32_t a,int32_t lo,int32_t hi){return a<lo?lo:a>hi?hi:a;}
static int32_t od_half(int32_t a){
  // Retain the original carry-dependent signed quantization rather than
  // replacing it with a floating point acceleration increment.
  if(a>=0){int carry=a&1;a>>=1;if(!carry)return a;++a;}
  int carry=a&1;a>>=1;return a-carry;
}
static void original_drive_step(OriginalDrive &s,const int32_t *p,int32_t turn,
                               int32_t command,int32_t dt,int player,int booster,
                               const int32_t *cosine,const int32_t *sine){
  s.slip_frames++;s.oil--;
  if(s.oil>=0){s.slip=253;s.slip_frames=0;s.oil_frames++;if(s.oil==28)s.yaw=s.yaw>=0?(s.yaw+256)*3:(s.yaw-256)*3;}
  else s.oil_frames=0;
  s.old_yaw=s.yaw;
  int32_t ratio=od_div((int64_t)od_abs(s.speed)*65536,1140),delta;
  if(turn){
    int32_t difference=(turn>0?p[0]:p[1])-s.steer;
    delta=turn>0?(difference<0?0:od_mul(od_mul(difference,3200,8),ratio,16))
                :(difference>=0?0:od_mul(od_mul(difference,3200,8),ratio,16));
    delta+=turn>0?p[2]:-p[2];
    delta=od_div((int64_t)delta*dt,51200);
    delta=(int32_t)(delta*od_abs(turn))>>7;
  }else delta=od_mul(od_mul(s.steer,p[3],8),ratio,16)*dt>>11;
  if(od_abs(s.speed)<128)s.steer+=((128-od_abs(s.speed))*delta>>8)*3;
  s.steer=od_clamp(s.steer+delta,-32768,32768);
  int32_t effective=s.steer-od_mul(od_mul(s.steer,ratio,16),100,8);
  if(command==p[14]&&s.speed>345)s.slip=240;
  if(command==p[13]){
    command=od_clamp((s.speed*-140)>>8,-120,120);
    int32_t moving=od_abs(s.vx)+od_abs(s.vz);
    s.motor=s.motor<0?-((moving< -s.motor)?moving:-s.motor):((moving<s.motor)?moving:s.motor);
  }
  int32_t acceleration=od_half(od_div((int64_t)command*dt,25600));
  if(!acceleration&&player&&!s.autopilot)s.motor=s.vx=s.vz=s.old_x=s.old_z=0;
  s.speed=s.motor+acceleration;
  if(player||!booster)s.speed=od_clamp(s.speed,-691,player?1036:1140);
  else if(s.speed< -691)s.speed=-691;
  s.motor=s.speed;
  int32_t previous_angle=s.angle;
  s.yaw=od_mul(od_div((int64_t)effective*s.speed,1140),4096,16);
  s.angle=(s.angle+od_div((int64_t)s.yaw*dt,51200))&1023;
  int32_t c=cosine[s.angle],sn=sine[s.angle];
  int32_t target_z=od_mul(s.motor,c,15),target_x=od_mul(s.motor,sn,15);
  int32_t dz=od_div((int64_t)(target_z-s.old_z)*51200,dt);
  int32_t dx=od_div((int64_t)(target_x-s.old_x)*51200,dt);
  int32_t longitudinal=od_mul(dz,c,15)+od_mul(dx,sn,15);
  int32_t lateral=od_mul(dx,c,15)-od_mul(dz,sn,15);
  int32_t slip=0;
  if(od_abs(longitudinal)>p[5])slip=od_div((int64_t)od_abs(longitudinal)*16,p[5]);
  if(od_abs(lateral)>p[4]){int r=od_div((int64_t)od_abs(lateral)*16,p[4]);if(r>slip)slip=r;}
  if(!slip)s.oil_frames=0;
  int32_t threshold;
  if(s.slip>slip){slip=(slip+(s.slip>>1))>>1;threshold=player?p[7]:77;}
  else{slip>>=1;threshold=player?p[6]:27;}
  int32_t correction_x,correction_z;
  if(slip>threshold&&!s.bypass){
    int32_t retained_x=od_div((int64_t)lateral*16,slip),retained_z=od_div((int64_t)longitudinal*16,slip);
    correction_z=od_mul(retained_z,p[8],8);correction_x=od_mul(retained_x,p[9],8);
    s.slip=slip;
    int32_t mix=(int32_t)(slip*p[11])>>8;mix=od_clamp(mix,0,player?p[10]:144);
    s.yaw=od_mul(s.old_yaw,mix,8)+od_mul(s.yaw,256-mix,8);
    s.angle=(previous_angle+od_div((int64_t)s.yaw*dt,51200))&1023;
  }else{s.slip=s.slip_frames=s.ax=s.az=0;correction_x=lateral;correction_z=longitudinal;}
  s.slip=player?(s.slip>>2):0;s.bypass=0;
  c=cosine[s.angle];sn=sine[s.angle];
  int32_t world_z=od_mul(correction_z,c,15)-od_mul(correction_x,sn,15);
  int32_t world_x=od_mul(correction_z,sn,15)+od_mul(correction_x,c,15);
  s.az=(od_div((int64_t)world_z*dt,51200)+s.az+1)>>1;
  s.ax=(od_div((int64_t)world_x*dt,51200)+s.ax+1)>>1;
  s.vx=s.old_x+s.ax;
  int32_t forward=s.old_z+s.az;
  s.vz=forward-((int32_t)(forward*s.slip)>>11);
  s.wheel=(s.wheel+od_mul((int32_t)(s.motor*21),dt,16))&1023;
  s.old_yaw=s.yaw;
}
