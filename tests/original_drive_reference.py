"""Compare independent fixed-point rules with the locally owned original x86 routine.

Requires Unicorn and a German GOG installation. No original code/data is exported.
The separately compiled original_drive_cli accepts/returns plain numeric states.
"""
from pathlib import Path
import argparse, hashlib, json, math, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_ESP
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from inspect_executable import le_image

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game',type=Path,required=True)
parser.add_argument('--cli',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--cases',type=int,default=4000)
parser.add_argument('--wsl',action='store_true')
args=parser.parse_args()
memory,_=le_image(args.game/'M.EXE')
assert memory[0x33d95]==0x55 and struct.unpack_from('<i',memory,0xb3a3c)[0]==6144,'German GOG executable expected'
trig=(args.game/'DATA640/TABLEAU.COS').read_bytes()
table=struct.unpack('<1280i',trig)
assert all(table[i]==round(math.cos(i*math.tau/1024)*32768) for i in range(1024))
profiles=[struct.unpack_from('<17i',memory,0xb39f8+i*68) for i in range(4)]
assert list(struct.unpack_from('<14i',memory,0x85da8))==[1,2,3,1,2,3,1,2,3,1,2,3,1,2], 'Unexpected original car-profile mapping'
offsets=[0x80,0xc0,0xb8,0x44,0x7c,0x84,0xa8,0xac,0xb0,0x68,0x64,0x70,0x6c,0x1a0,0xb4,0xbc,0x170,0x174,0x1ac]
names='speed motor steer angle yaw old_yaw slip ax az vx vz old_x old_z oil bypass wheel slip_frames oil_frames autopilot'.split()
machine=Uc(UC_ARCH_X86,UC_MODE_32);machine.mem_map(0,8<<20);machine.mem_write(0,bytes(memory));machine.mem_write(0x2a423c,trig)
rng=random.Random(714);inputs=[];expected=[];descriptions=[]
for case in range(args.cases):
    player=rng.randrange(2);boost=rng.randrange(2);profile=rng.randrange(1,4)
    turn=rng.choice([-128,-64,0,0,64,128]);command=rng.choice([400,-200,-500])
    dt=rng.choice([256,512,768,1024,1536,2048,2560,3072,4096])
    speed=rng.randrange(-691,1291);angle=rng.randrange(1024)
    vx=(speed*table[angle+256])>>15;vz=(speed*table[angle])>>15
    state=[speed,speed,rng.randrange(-20000,20001),angle,rng.randrange(-1500,1501),0,
           rng.choice([0,0,0,40,120,240]),rng.randrange(-15,16),rng.randrange(-15,16),
           vx,vz,vx+rng.randrange(-100,101),vz+rng.randrange(-100,101),
           rng.choice([-1,-1,0,1,29,30]),rng.randrange(2),rng.randrange(1024),0,0,rng.randrange(2)]
    car=0xacf34 if player else 0xad134
    machine.mem_write(car,bytes(512))
    def put(address,value):machine.mem_write(address,struct.pack('<I',value&0xffffffff))
    for offset,value in zip(offsets,state):put(car+offset,value)
    put(car+0x1b0,profile);put(car+0xc4,turn);put(car+0xc8,command)
    machine.mem_write(car+0x1fd,bytes([32 if boost else 0]))
    put(0xb39b0,-1);put(0xb37d0,dt);put(0xdfe30,0)
    machine.reg_write(UC_X86_REG_EBP,car);machine.reg_write(UC_X86_REG_ESP,0x700000)
    machine.emu_start(0x33d95,0x34557,count=20000)
    actual=[struct.unpack('<i',machine.mem_read(car+offset,4))[0] for offset in offsets]
    inputs.append(' '.join(map(str,[player,boost,turn,command,dt,*profiles[profile],*state])))
    expected.append(actual);descriptions.append({'case':case,'profile':profile,'player':player,'boost':boost,'turn':turn,'command':command,'dt':dt,'initial':state})
cli=str(args.cli.resolve());cmd=[cli]
if args.wsl:
    p=args.cli.resolve();cli='/mnt/'+p.drive[0].lower()+p.as_posix()[2:];cmd=['wsl','-d','Ubuntu-24.04','--',cli]
run=subprocess.run(cmd,input='\n'.join(inputs)+'\n',text=True,capture_output=True,check=True)
results=[[int(x) for x in line.split()] for line in run.stdout.splitlines()]
assert len(results)==len(expected)
failures=[]
for description,a,b in zip(descriptions,results,expected):
    if a!=b:failures.append(dict(description,differences={name:{'port':x,'original':y} for name,x,y in zip(names,a,b) if x!=y}))
report={'cases':args.cases,'compared_fields':len(offsets),'failures':len(failures),
        'original_exe_sha256':hashlib.sha256((args.game/'M.EXE').read_bytes()).hexdigest(),
        'table_sha256':hashlib.sha256(trig).hexdigest(),'first_failures':failures[:12]}
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
sys.exit(1 if failures else 0)
