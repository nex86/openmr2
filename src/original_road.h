// Independently expressed local road-plane operations. The stored PST chord
// direction, chord length and frame rotation are gameplay data, not values to
// recompute from the presentation's three-dimensional interpolation.
#pragma once
#include "original_drive.h"
struct OriginalPlane {int32_t x,z,heading;};
static int32_t op_product(int32_t a,int32_t b){return (int32_t)((uint32_t)a*(uint32_t)b);}
static OriginalPlane original_plane_next(OriginalPlane a,int turn,int length,int rotation,const int32_t *cosine,const int32_t *sine){
  int direction=(a.heading+turn)&1023;
  OriginalPlane b={a.x+(op_product(-length,sine[direction])>>15),a.z+(op_product(length,cosine[direction])>>15),(a.heading+rotation)&1023};return b;
}
static void original_plane_local(OriginalPlane frame,int x,int z,int &lateral,int &along,const int32_t *cosine,const int32_t *sine){
  int dx=x-frame.x,dz=z-frame.z,c=cosine[frame.heading],s=sine[frame.heading];
  lateral=(int32_t)((uint32_t)op_product(c,dx)+(uint32_t)op_product(s,dz))>>15;
  along=(int32_t)((uint32_t)op_product(c,dz)-(uint32_t)op_product(s,dx))>>15;
}
static int original_pilot_command(int lateral,int along,int heading,int steering,int player,const int32_t *cosine,const int32_t *sine){
  int x,z;original_plane_local({0,0,(-heading)&1023},lateral,along,x,z,cosine,sine);
  int error=op_product(x,-600)>>16,w=steering&1023;if(w>512)w-=1024;
  int response=op_product(w,z)>>16,remaining=3072-od_abs(error);if(remaining<0)remaining=0;
  int gain=player?-60:((-120-(remaining>>4))>>1);
  return od_clamp(error+(op_product(response,gain)>>8),-128,128);
}
