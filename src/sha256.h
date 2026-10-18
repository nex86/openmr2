// SHA-256, implemented from the algorithm specification. No external runtime.
#pragma once
#include "sys.h"
struct Sha256 {
  uint32_t h[8]; uint8_t block[64]; uint64_t bytes; unsigned used;
  static uint32_t rotr(uint32_t a, unsigned n) { return (a >> n) | (a << (32-n)); }
  void transform() {
    static const uint32_t k[64] = {
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64];
    for (unsigned i=0;i<16;i++) w[i]=(uint32_t(block[i*4])<<24)|(uint32_t(block[i*4+1])<<16)|(uint32_t(block[i*4+2])<<8)|block[i*4+3];
    for (unsigned i=16;i<64;i++) {
      uint32_t a=w[i-15],b=w[i-2];
      w[i]=w[i-16]+(rotr(a,7)^rotr(a,18)^(a>>3))+w[i-7]+(rotr(b,17)^rotr(b,19)^(b>>10));
    }
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
    for (unsigned i=0;i<64;i++) {
      uint32_t t=v+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
      uint32_t u=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));
      v=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
  }
  void init() {
    static const uint32_t initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for(unsigned i=0;i<8;i++)h[i]=initial[i]; bytes=used=0;
  }
  void update(const uint8_t *p, size_t n) {
    bytes+=n;
    while(n--) { block[used++]=*p++; if(used==64) {transform();used=0;} }
  }
  void finish(char out[65]) {
    uint64_t bits=bytes*8; uint8_t b=128; update(&b,1); b=0;
    while(used!=56)update(&b,1);
    for(int i=7;i>=0;i--){b=(uint8_t)(bits>>(i*8));update(&b,1);}
    const char *hex="0123456789abcdef";
    for(unsigned i=0;i<32;i++){unsigned v=(h[i/4]>>(24-(i%4)*8))&255;out[i*2]=hex[v>>4];out[i*2+1]=hex[v&15];}out[64]=0;
  }
};
static void (*sha256_progress)(const char *,uint64_t,uint64_t);
static int sha256_file(const char *path, char out[65], uint64_t *size=0) {
  void *f=sys_fopen(path); if(!f)return 0;
  uint64_t n=sys_fsize(f); if(size)*size=n;
  uint8_t *buf=(uint8_t *)sys_alloc(65536); if(!buf){sys_fclose(f);return 0;}
  Sha256 s;s.init();int ok=1;
  if(sha256_progress)sha256_progress(path,0,n);
  for(uint64_t off=0;off<n;){uint32_t count=(uint32_t)(n-off>65536?65536:n-off);if(!sys_fread_at(f,off,buf,count)){ok=0;break;}s.update(buf,count);off+=count;if(sha256_progress && (!(off%1048576)||off==n))sha256_progress(path,off,n);}
  sys_fclose(f);sys_free(buf);if(ok)s.finish(out);return ok;
}
