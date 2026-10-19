#include <cstdio>
#include <initializer_list>
#include "../src/original_body.h"
int main(int argc,char **argv){
  int32_t trig[1280];FILE *f=fopen(argv[1],"rb");if(!f)return 2;fread(trig,4,1280,f);fclose(f);
  OriginalBody a,b;int sa,sb;
  while(scanf("%d",&a.old[0].x)==1){
    int *p=(int*)&a;for(unsigned i=1;i<sizeof(a)/4;i++)scanf("%d",p+i);
    p=(int*)&b;for(unsigned i=0;i<sizeof(b)/4;i++)scanf("%d",p+i);
    scanf("%d%d",&sa,&sb);int time=-1,ids=ob_detect(a,b,2048,time);printf("%d %d ",ids,ids?time:0);
    if(ids)ob_respond(a,b,ids,trig,trig+256,sa,sb);
    for(OriginalBody *body:{&a,&b}){p=(int*)body;for(unsigned i=0;i<sizeof(a)/4;i++)printf("%d ",p[i]);}printf("%d %d\n",sa,sb);
  }
}
