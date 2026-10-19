#pragma once
#include "original_drive.h"
#include "original_wall_rules.h"
// Local, straight-road wall response. Extent is the road half-width.
// The original heading-dependent hull is subtracted before contact detection.
static void original_wall_step(OriginalDrive &s,int &x,int extent,int player){
  extent-=ORIGINAL_HULL_EXTENT[s.angle&1023];
  if(x>-extent&&x<extent)return;
  int side=x>0?1:-1;x=side*extent;
  if(side>0?s.vx>=0:s.vx<0)s.old_x=s.vx=od_mul(s.vx,-75,8);
  s.old_z=s.vz=od_mul(s.vz,0xf200,16);
  if(s.vz>=0&&s.vz<90)s.old_z=0;
  int aligned=s.vz;if((aligned^s.motor)<0)aligned=-aligned;
  s.motor=(aligned+3*s.motor)>>2;
  const OriginalWallRule &r=ORIGINAL_WALL_RULES[(((!!player*2+(side>0))*2+(s.vz<0))*1024)+(s.angle&1023)];
  int factor=s.vz<0?-s.vz+90:(s.vz>90?s.vz-90:0);
  s.yaw=od_clamp((s.yaw+4*od_mul(r.torque,factor,8))>>2,-1500,1500);
  s.slip=r.slip;
  if(s.motor)s.angle=s.motor>0?r.positive:r.negative;
}
