"""Measure discrete wall-angle rules and verify variable-speed reactions against M.EXE.

Only numeric input/output facts are emitted, not original executable bytes.
Requires the German GOG executable, Unicorn and Capstone in the private fixture.
"""
from pathlib import Path
import argparse,json,random,struct,subprocess,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EDX
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from inspect_executable import le_image
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game',type=Path,required=True);parser.add_argument('--generate',type=Path)
parser.add_argument('--cli',type=Path);parser.add_argument('--wsl',action='store_true')
parser.add_argument('--output',type=Path,required=True);parser.add_argument('--cases',type=int,default=6000)
args=parser.parse_args();memory,_=le_image(args.game/'M.EXE');assert memory[0x36d99]==0x55
machine=Uc(UC_ARCH_X86,UC_MODE_32);machine.mem_map(0,8<<20);machine.mem_write(0,bytes(memory));machine.mem_write(0x2a423c,(args.game/'DATA640/TABLEAU.COS').read_bytes())
offsets=[0x80,0xc0,0xb8,0x44,0x7c,0x84,0xa8,0xac,0xb0,0x68,0x64,0x70,0x6c,0x1a0,0xb4,0xbc,0x170,0x174,0x1ac]
def put(a,v):machine.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<i',machine.mem_read(a,4))[0]
observed=[]
md=Cs(CS_ARCH_X86,CS_MODE_32)
sites={i.address for i in md.disasm(memory[0x36d99:0x37a11],0x36d99) if i.mnemonic=='sar' and i.op_str=='edx, 8'}
def hook(m,address,size,u):
    if address in sites:
        raw=m.reg_read(UC_X86_REG_EDX);observed.append(raw if raw<0x80000000 else raw-0x100000000)
machine.hook_add(UC_HOOK_CODE,hook,begin=0x36d99,end=0x37a10)
def original(player,side,extent,x,state):
    car=0xacf34 if player else 0xad134
    machine.mem_write(car,bytes(512))
    for offset,value in zip(offsets,state):put(car+offset,value)
    put(car+0x34,0x600000);put(0x600004,0);put(0x1e323c,0);put(0x1e3284,extent)
    put(0x1e329c,0);put(0x1e3288,0);put(car+0x3c,x)
    machine.reg_write(UC_X86_REG_EBP,car);machine.reg_write(UC_X86_REG_ESP,0x700000)
    put(0x700000,0x6ff000);observed.clear()
    machine.emu_start(0x36d99,0x6ff000,count=20000)
    return [get(car+o) for o in offsets],get(car+0x3c),observed[:]
if args.generate:
    rules=[]
    for player in (0,1):
      for side in (-1,1):
       for reverse in (0,1):
        for angle in range(1024):
          state=[0]*19;state[1]=600;state[3]=angle;state[9]=side*100;state[10]=-175 if reverse else 367
          first,x,products=original(player,side,40000,side*40001,state)
          assert len(products)==1,(player,side,reverse,angle,products)
          assert products[0]%256==0,products
          coefficient=products[0]//256
          state[1]=-600
          second,_,_=original(player,side,40000,side*40001,state)
          assert first[6]==second[6]
          rules.append((coefficient,first[3],second[3],first[6]))
    args.generate.write_text('// Numeric wall-response measurements; regenerate with tests/original_wall_reference.py.\n'
                            '// Each row: yaw coefficient, positive/negative motor angle, grip state. No executable bytes.\n'
                            'struct OriginalWallRule { short torque; unsigned short positive,negative; unsigned char slip; };\n'
                            'static const OriginalWallRule ORIGINAL_WALL_RULES[8192]={\n'+
                            ',\n'.join('{'+','.join(map(str,r))+'}' for r in rules)+'\n};\n'+
                            'static const int ORIGINAL_HULL_EXTENT[1024]={\n'+
                            ','.join(str(struct.unpack_from('<i',memory,0xb3b08+i*8)[0]) for i in range(1024))+'\n};\n')
    print('Measured',len(rules),'angle/player/side/direction rules.',flush=True)
if args.cli:
    rng=random.Random(71401);inputs=[];expected=[]
    for case in range(args.cases):
      player=rng.randrange(2);side=rng.choice([-1,1]);extent=rng.randrange(18000,45001);x=side*(extent+rng.randrange(1,5001))
      state=[rng.randrange(-691,1141),rng.randrange(-1000,1201),0,rng.randrange(1024),rng.randrange(-1500,1501),0,0,0,0,rng.randrange(-1000,1001),rng.randrange(-1000,1201),rng.randrange(-1000,1001),rng.randrange(-1000,1001),-1,0,0,0,0,0]
      result,position,_=original(player,side,extent,x,state)
      inputs.append(' '.join(map(str,[player,extent,x,*state])));expected.append([position,*result])
    cli=str(args.cli.resolve());command=[cli]
    if args.wsl:
      p=args.cli.resolve();command=['wsl','-d','Ubuntu-24.04','--','/mnt/'+p.drive[0].lower()+p.as_posix()[2:]]
    result=subprocess.run(command,input='\n'.join(inputs)+'\n',text=True,capture_output=True,check=True)
    values=[[int(x) for x in row.split()] for row in result.stdout.splitlines()]
    bad=[{'case':i,'input':inputs[i],'port':a,'original':b} for i,(a,b) in enumerate(zip(values,expected)) if a!=b]
    assert len(values)==len(expected)
    report={'cases':len(values),'compared_fields':20,'failures':len(bad),'first_failures':bad[:4]}
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));sys.exit(1 if bad else 0)
