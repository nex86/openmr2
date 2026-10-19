"""Compare the contact velocity exchange with a locally owned German M.EXE.

Requires Unicorn. No original code or assets are written to the report.
"""
from pathlib import Path
import argparse,json,random,struct,subprocess,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from inspect_executable import le_image
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--game',type=Path,required=True);p.add_argument('--cli',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--cases',type=int,default=6000)
p.add_argument('--wsl',action='store_true');a=p.parse_args()
memory,_=le_image(a.game/'M.EXE');assert memory[0x37c4f:0x37c51]==b'\x83\x3d'
m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(0,8<<20);m.mem_write(0,bytes(memory))
r=random.Random(71402);inputs=[];expected=[]
for i in range(a.cases):
    x=z=0
    while x*x+z*z==0:x=r.randrange(-18000,18001);z=r.randrange(-18000,18001)
    values=[x,z,*[r.randrange(-1140,1141) for _ in range(4)]]
    m.mem_write(0x700000,struct.pack('<7i',0x6ff000,*values));m.reg_write(UC_X86_REG_ESP,0x700000)
    m.emu_start(0x37c4f,0x6ff000,count=1000)
    expected.append(list(struct.unpack('<4i',m.mem_read(0xb3884,16))))
    inputs.append(' '.join(map(str,values)))
cmd=[str(a.cli.resolve())]
if a.wsl:
    f=a.cli.resolve();cmd=['wsl','-d','Ubuntu-24.04','--','/mnt/'+f.drive[0].lower()+f.as_posix()[2:]]
result=subprocess.run(cmd,input='\n'.join(inputs)+'\n',text=True,capture_output=True,check=True)
actual=[list(map(int,row.split())) for row in result.stdout.splitlines()]
assert len(actual)==len(expected)
bad=[{'case':i,'input':inputs[i],'port':v,'original':w} for i,(v,w) in enumerate(zip(actual,expected)) if v!=w]
report={'cases':len(actual),'compared_fields':4,'failures':len(bad),'first_failures':bad[:4]}
a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));sys.exit(bool(bad))
