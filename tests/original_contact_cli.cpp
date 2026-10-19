#include <iostream>
#include "../src/original_contact.h"
int main(){int x,z,ax,az,bx,bz;while(std::cin>>x>>z>>ax>>az>>bx>>bz){
  original_contact_exchange(x,z,ax,az,bx,bz);
  std::cout<<ax<<' '<<az<<' '<<bx<<' '<<bz<<'\n';
}}
