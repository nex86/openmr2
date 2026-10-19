// Independently expressed five-circle contact rules. Verified against the
// locally executed original detector, response and full multi-car update loop.
#pragma once
#include <stdint.h>
#include "original_road.h"
struct OriginalPoint {int x,z;};
struct OriginalBody {
  OriginalPoint old[5],next[5];
  int world_vx,world_vz,vx,vz,yaw,motor,slip,heading,road_heading;
};
static uint32_t ob_sqrt(uint32_t n){uint32_t r=0,bit=1u<<30;while(bit>n)bit>>=2;while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}return r;}
static int ob_distance(int x,int z){x=(int8_t)(x>>8);z=(int8_t)(z>>8);int a=x*2+1,b=z*2+1;return (ob_sqrt(a*a+b*b)+1)/2;}
static int ob_overlap(OriginalPoint a,OriginalPoint b,int radius,int grid){int x=a.x-b.x,z=a.z-b.z;return x<radius&&x>-radius&&z<radius&&z>-radius&&ob_distance(x,z)<=grid;}
static void ob_shape(OriginalPoint *p,int heading,const int32_t *cosine,const int32_t *sine){
  int c=cosine[heading&1023],s=sine[heading&1023];
  OriginalPoint first={(2500*c+5800*s)>>15,(5800*c-2500*s)>>15},second={(-2500*c+5800*s)>>15,(5800*c+2500*s)>>15};
  p[1]={p[0].x+first.x,p[0].z+first.z};p[2]={p[0].x+second.x,p[0].z+second.z};p[3]={p[0].x-first.x,p[0].z-first.z};p[4]={p[0].x-second.x,p[0].z-second.z};
}
static int ob_time(OriginalPoint old_a,OriginalPoint next_a,OriginalPoint old_b,OriginalPoint next_b,int radius,int grid,int dt,int best,bool &changed){
  int time=dt>>1,step=time;OriginalPoint a=old_a,b=old_b;bool add_end=true;
  for(;;){
    OriginalPoint ta=add_end?next_a:old_a,tb=add_end?next_b:old_b;
    a={(a.x+ta.x)>>1,(a.z+ta.z)>>1};b={(b.x+tb.x)>>1,(b.z+tb.z)>>1};
    bool inside=ob_overlap(a,b,radius,grid);
    if(!inside&&best>=0&&time>=best){changed=false;return best;}
    step>>=1;if(!step){changed=true;return time;}
    time+=inside?-step:step;add_end=!inside;
  }
}
static int ob_detect(const OriginalBody &a,const OriginalBody &b,int dt,int &time){
  if(od_abs(a.next[0].x-b.next[0].x)>=20600||od_abs(a.next[0].z-b.next[0].z)>=20600)return 0;
  int ids=0;time=-1;
  for(int i=0;i<5;i++)for(int j=0;j<5;j++){
    int radius=i&&j?8000:i||j?10000:12000,grid=i&&j?31:i||j?39:47;
    if(!ob_overlap(b.next[j],a.next[i],radius,grid-1))continue;
    bool changed;int next_time=ob_time(b.old[j],b.next[j],a.old[i],a.next[i],radius,grid,dt,time,changed);
    if(changed){time=next_time;ids=((i+1)<<8)|(j+1);}
  }
  return ids;
}
static bool ob_any_overlap(const OriginalBody &a,const OriginalBody &b){
  if(od_abs(a.next[0].x-b.next[0].x)>=20600||od_abs(a.next[0].z-b.next[0].z)>=20600)return false;
  for(int i=0;i<5;i++)for(int j=0;j<5;j++)if(ob_overlap(b.next[j],a.next[i],i&&j?8000:i||j?10000:12000,i&&j?31:i||j?39:47))return true;
  return false;
}
static int ob_respond(OriginalBody &a,OriginalBody &b,int ids,const int32_t *cosine,const int32_t *sine,int &saved_yaw_a,int &saved_yaw_b){
  static const int direction[5]={0,50,974,562,462};
  int mean_x=(a.world_vx+b.world_vx)>>1,mean_z=(a.world_vz+b.world_vz)>>1;
  int ax=a.world_vx-mean_x,az=a.world_vz-mean_z,bx=b.world_vx-mean_x,bz=b.world_vz-mean_z;
  int ia=(ids>>8)-1,ib=(ids&255)-1;
  int acx=ax,acz=az,bcx=bx,bcz=bz;
  if(ia){int rot=(a.yaw*-116)>>10,phi=(a.heading+direction[ia])&1023;acx+=(-rot*cosine[phi])>>15;acz+=(rot*sine[phi])>>15;}
  if(ib){int rot=(b.yaw*-116)>>10,phi=(b.heading+direction[ib])&1023;bcx+=(rot*cosine[phi])>>15;bcz+=(rot*sine[phi])>>15;}
  int dx=od_abs(bcx-acx),dz=od_abs(bcz-acz),m=dx+dz+(dx>>1);if(m>1024)m=1024;
  int retain=256-((m>128?m-128:0)>>3);
  auto one=[&](OriginalBody &body,int index,int own_x,int own_z,int target_x,int target_z,int &saved_yaw){
    int x=target_x-own_x,z=target_z-own_z;
    if(index){
      int phi=(direction[index]-body.heading)&1023;
      int tangent=(op_product(z,cosine[phi])-op_product(x,sine[phi]))>>15;
      int radial=(op_product(x,cosine[phi])+op_product(z,sine[phi]))>>15;
      saved_yaw=((radial*-220)>>7)+body.yaw;
      int change=tangent,sign=cosine[direction[index]]>>16;if(sign)change=(tangent*sign*retain)>>9;
      body.motor=(change+body.motor*2)>>1;
      x=op_product(-tangent,sine[phi])>>15;z=op_product(tangent,cosine[phi])>>15;
    }
    // The executable uses arithmetic shifts, including negative products.
    body.world_vx=(op_product(x+own_x,retain)>>8)+mean_x;
    body.world_vz=(op_product(z+own_z,retain)>>8)+mean_z;
    int phi=(-body.road_heading)&1023;
    body.vx=(op_product(body.world_vx,cosine[phi])-op_product(body.world_vz,sine[phi]))>>15;
    body.vz=(op_product(body.world_vz,cosine[phi])+op_product(body.world_vx,sine[phi]))>>15;
    body.yaw=od_clamp(saved_yaw,-1500,1500);body.slip=240;
  };
  one(a,ia,ax,az,bcx,bcz,saved_yaw_a);one(b,ib,bx,bz,acx,acz,saved_yaw_b);
  int step_x=(a.old[0].x-b.old[0].x)>>5,step_z=(a.old[0].z-b.old[0].z)>>5;
  for(int i=0;i<5;i++){a.old[i]=a.next[i];b.old[i]=b.next[i];}
  int iterations=0;
  while(ob_any_overlap(a,b)&&iterations++<4096){
    if(!step_x&&!step_z)break;
    for(int i=0;i<5;i++){a.old[i].x+=step_x;a.old[i].z+=step_z;a.next[i]=a.old[i];b.old[i].x-=step_x;b.old[i].z-=step_z;b.next[i]=b.old[i];}
  }
  return m;
}
