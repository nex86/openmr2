#pragma once
#include "original_drive.h"
// Equal-mass normal velocity exchange, retaining each tangential component.
// Signed division truncates towards zero, as in the DOS contact solver.
static void original_contact_exchange(int dx,int dz,int &ax,int &az,int &bx,int &bz){
  int64_t xx=(int64_t)dx*dx,zz=(int64_t)dz*dz,xz=(int64_t)dx*dz,d=xx+zz;
  if(!d)return;
  int na=(int)((xx*bx+zz*ax+xz*(bz-az))/d);
  int nz=(int)((xx*az+zz*bz+xz*(bx-ax))/d);
  int nb=(int)((xx*ax+zz*bx+xz*(az-bz))/d);
  int nw=(int)((xx*bz+zz*az+xz*(ax-bx))/d);
  ax=na;az=nz;bx=nb;bz=nw;
}
static int original_contact_retention(int ax,int az,int bx,int bz,int cars){
  int dx=od_abs(bx-ax),dz=od_abs(bz-az);
  int metric=od_clamp(dx+dz+(dx>>1),0,1024);
  return 256-((metric>128?metric-128:0)>>(cars?3:5));
}
