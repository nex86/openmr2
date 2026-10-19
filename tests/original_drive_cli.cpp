// Numerical test adapter; original executable/data are never compiled in.
#include "../src/original_drive.h"
#include <stdio.h>
#include <math.h>
int main(){
  int32_t cosine[1024],sine[1024];for(int i=0;i<1024;i++){double a=i*6.2831853071795864769/1024;cosine[i]=(int32_t)llround(cos(a)*32768);sine[i]=(int32_t)-llround(sin(a)*32768);}
  int player,booster,turn,command,dt;int32_t p[17];OriginalDrive s;
  static_assert(sizeof s==19*4,"flat numeric test protocol");
  while(scanf("%d%d%d%d%d",&player,&booster,&turn,&command,&dt)==5){
    for(int i=0;i<17;i++)if(scanf("%d",&p[i])!=1)return 2;
    int32_t *values=(int32_t *)&s;for(int i=0;i<19;i++)if(scanf("%d",values+i)!=1)return 3;
    original_drive_step(s,p,turn,command,dt,player,booster,cosine,sine);
    for(int i=0;i<19;i++)printf("%d%c",values[i],i==18?'\n':' ');
  }
}
