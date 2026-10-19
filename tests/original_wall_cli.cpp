#include <iostream>
#include "../src/original_wall.h"
int main(){int player,extent,x;OriginalDrive s;
  while(std::cin>>player>>extent>>x){
    int32_t *p=&s.speed;for(int i=0;i<19;i++)std::cin>>p[i];
    original_wall_step(s,x,extent,player);std::cout<<x;
    for(int i=0;i<19;i++)std::cout<<' '<<p[i];std::cout<<'\n';
  }
}
