"""Verify contact point selection, impact time, velocity, yaw, slip, motor and
integer separation with original x86 code. No original code/assets are exported.
"""
from pathlib import Path
import json,struct,random,subprocess,math
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
import argparse,hashlib
from original_fixture import original_memory,wsl_path
parser=argparse.ArgumentParser(description='Compare complete five-circle detection and contact response with the locally owned German DOS executable.')
parser.add_argument('--game',type=Path,required=True);parser.add_argument('--cli',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--wsl',action='store_true');parser.add_argument('--cases',type=int,default=2000);args=parser.parse_args()
memory,cos=original_memory(args.game.resolve());bias=cb=0
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,32<<20);u.mem_write(0,memory)
def put(a,v):u.mem_write(a,struct.pack('<i',v))
def get(a):return struct.unpack('<i',u.mem_read(a,4))[0]
def call(a):
 u.reg_write(UC_X86_REG_ESP,0x1f00000);put(0x1f00000,0x1e00000);u.emu_start(a+cb,0x1e00000,count=5000000)
 if u.reg_read(UC_X86_REG_EIP)!=0x1e00000:raise RuntimeError(hex(a))
def shape(x,z,h):
 c,s=cos[h],cos[h+256];fx,fz=(2500*c+5800*s)>>15,(5800*c-2500*s)>>15;sx,sz=(-2500*c+5800*s)>>15,(5800*c+2500*s)>>15
 return [x,z,x+fx,z+fz,x+sx,z+sz,x-fx,z-fz,x-sx,z-sz]
def body(rng,x,z):
 angle=rng.randrange(1024);yaw=rng.randrange(-400,401);ox=x+rng.randrange(-4000,4001);oz=z+rng.randrange(-8000,8001)
 return shape(ox,oz,(angle-yaw//25)&1023)+shape(x,z,angle)+[rng.randrange(-1100,1101),rng.randrange(-1100,1101),rng.randrange(-1100,1101),rng.randrange(-1100,1101),yaw,rng.randrange(1141),0,angle,rng.randrange(1024)]
def write_body(addr,v,frame):
 u.mem_write(addr,bytes(512));put(addr+0x30,frame);put(frame+0x18,v[28]);u.mem_write(addr+0x5c,struct.pack('<2i',*v[:2]));u.mem_write(addr+0xd0,struct.pack('<8i',*v[2:10]));u.mem_write(addr+0x54,struct.pack('<2i',*v[10:12]));u.mem_write(addr+0xf0,struct.pack('<8i',*v[12:20]))
 for off,n in [(0x74,20),(0x78,21),(0x68,22),(0x64,23),(0x7c,24),(0xc0,25),(0xa8,26),(0x94,27)]:put(addr+off,v[n])
def read_body(addr):
 return list(struct.unpack('<2i',u.mem_read(addr+0x5c,8)))+list(struct.unpack('<8i',u.mem_read(addr+0xd0,32)))+list(struct.unpack('<2i',u.mem_read(addr+0x54,8)))+list(struct.unpack('<8i',u.mem_read(addr+0xf0,32)))+[get(addr+off) for off in (0x74,0x78,0x68,0x64,0x7c,0xc0,0xa8,0x94)]+[get(get(addr+0x30)+0x18)]
rng=random.Random(71603);inputs=[];expected=[];a=0x1a00000;c=0x1a00200
for i in range(args.cases):
 av=body(rng,0,0);bv=body(rng,rng.randrange(-18000,18001),rng.randrange(-18000,18001));sa=sb=0
 write_body(a,av,0x1b00000);write_body(c,bv,0x1b00100);put(0xb37d0+bias,2048);u.reg_write(UC_X86_REG_EBP,a);u.reg_write(UC_X86_REG_EDI,c);call(0x3568a);ids=u.reg_read(UC_X86_REG_ESI);time=get(0xb3854+bias) if ids else 0
 if ids:
  u.mem_write(0x1b00200,struct.pack('<4i',a,c,time,ids));put(0xb3868+bias,0x1b00200);put(0xb38a4+bias,sa);put(0xb38b8+bias,sb);u.reg_write(UC_X86_REG_EBP,a);u.reg_write(UC_X86_REG_EDI,c);call(0x39d34);sa=get(0xb38a4+bias);sb=get(0xb38b8+bias)
 inputs.append(' '.join(map(str,av+bv+[0,0])));expected.append([ids,time]+read_body(a)+read_body(c)+[sa,sb])
command=[str(args.cli.resolve()),str((args.game/'DATA640/TABLEAU.COS').resolve())]
if args.wsl:command=['wsl','-d','Ubuntu-24.04','--',wsl_path(args.cli),wsl_path(args.game/'DATA640/TABLEAU.COS')]
result=subprocess.run(command,input='\n'.join(inputs)+'\n',capture_output=True,text=True,check=True)
actual=[list(map(int,line.split())) for line in result.stdout.splitlines()];assert len(actual)==len(expected)
bad=[{'case':i,'input':inputs[i],'differences':[(k,x,y) for k,(x,y) in enumerate(zip(a,e)) if x!=y]} for i,(a,e) in enumerate(zip(actual,expected)) if a!=e]
report={'cases':len(expected),'contacts':sum(bool(e[0]) for e in expected),'failures':len(bad),'first_failures':bad[:5]};args.output.write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));raise SystemExit(bool(bad))
