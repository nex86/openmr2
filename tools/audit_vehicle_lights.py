"""Audit indexed lamp markers on all 14 cars, six worlds and three editions."""
from pathlib import Path
import json,struct,re,math
from PIL import Image,ImageDraw,ImageChops
root=Path(__file__).resolve().parents[1];fixtures=root.parents[1]
car_names=re.findall(r'\{"([^"]+)", "([^"]+)", \d+,', (root/'src/g_track.inc').read_text())[:14]
def u32(d,o):return struct.unpack_from('<I',d,o)[0]
def hlz(path):
 d=path.read_bytes();w,h=struct.unpack_from('<HH',d,768);pos=772;bits=0;remaining=0;out=bytearray()
 def bit():
  nonlocal pos,bits,remaining
  if not remaining:bits=u32(d,pos);pos+=4;remaining=32
  remaining-=1;return (bits>>remaining)&1
 while True:
  if bit():out.append(d[pos]);pos+=1;continue
  if bit():
   word=struct.unpack_from('<H',d,pos)[0];pos+=2;off=(word|(-65536))>>3;count=word&7
   if not count:
    count=d[pos];pos+=1
    if not count:break
  else:count=(bit()<<1)|bit();off=d[pos]-256;pos+=1
  for _ in range(count+2):out.append(out[len(out)+off])
 assert len(out)==w*h,path
 red=Image.frombytes('L',(w,h),bytes(255 if p==248 and d[744:747]==bytes((128,0,128)) else 0 for p in out))
 yellow=Image.frombytes('L',(w,h),bytes(255 if p==249 and d[747:750]==bytes((0,252,0)) else 0 for p in out))
 # Match the runtime's four-connected, minimum-four-pixel marker rule.
 pixels=bytearray(red.tobytes());seen=set()
 for start,value in enumerate(pixels):
  if not value or start in seen:continue
  group=[start];seen.add(start)
  for current in group:
   x,y=current%w,current//w
   for adjacent,valid in ((current-1,x>0),(current+1,x+1<w),(current-w,y>0),(current+w,y+1<h)):
    if valid and pixels[adjacent] and adjacent not in seen:seen.add(adjacent);group.append(adjacent)
  if len(group)<4:
   for point in group:pixels[point]=0
 red=Image.frombytes('L',(w,h),bytes(pixels))
 return red,yellow
results=[];cache={}
for edition in ('English','German','French'):
 for world in ('FAC','TIB','EXT','LUN','BAY','BRO'):
  directory=fixtures/('Megarace 2 '+edition)/'CARS'/world
  for name,model in car_names:
   d=(directory/(model+'.M3D')).read_bytes();nn,nt=u32(d,8),u32(d,12);base,ofs=u32(d,20),u32(d,24);materials=[]
   for o in range(64,len(d)-30,30):
    if u32(d,o)==0xffffffff:break
    tex=d[o:o+14].split(b'\0')[0].decode('latin1');typ=struct.unpack_from('<H',d,o+26)[0];path=directory/(tex.split('.')[0]+'.HLZ')
    if typ==1 and path.exists():
     if path not in cache:cache[path]=hlz(path)
     materials.append(cache[path])
    else:materials.append(None)
   faces=[];bounds=[];offsets=[]
   for node in range(nn):
    no=nt+node*248;par=u32(d,no+4);parent=(par-nt)//248 if node and nt<=par<no else -1
    offset=struct.unpack_from('<iii',d,no+12) if node else (0,0,0)
    if parent>=0:offset=tuple(a+b for a,b in zip(offset,offsets[parent]))
    offsets.append(offset);nf,fp=u32(d,no+0x78+20),u32(d,no+0x78+24)
    for i in range(nf):
     f=fp+i*28;mat=struct.unpack_from('<H',d,f+24)[0]//30
     if mat>=len(materials) or materials[mat] is None:continue
     vertices=[struct.unpack_from('<hhh',d,u32(d,f+k*4)-base+ofs) for k in range(3)]
     xyz=[tuple(a+b for a,b in zip(v,offset)) for v in vertices];bounds+=xyz
     uv=[(d[f+14+k*4],d[f+15+k*4]) for k in range(3)]
     spin=nn==3 and parent==0
     faces.append((xyz,uv,materials[mat],spin))
   lo=min(v[2] for v in bounds);hi=max(v[2] for v in bounds);middle=(lo+hi)/2;extent=(hi-lo)/2;rear=front=bad_red=0
   for xyz,uv,masks,spin in faces:
    a=[xyz[1][k]-xyz[0][k] for k in range(3)];b=[xyz[2][k]-xyz[0][k] for k in range(3)];n=(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]);length=math.sqrt(sum(x*x for x in n))
    raster=Image.new('L',masks[0].size);ImageDraw.Draw(raster).polygon(uv,fill=255)
    hit=[ImageChops.multiply(raster,mask).getbbox() is not None for mask in masks]
    eligible=not spin and length and abs(n[2])/length>.35
    if hit[0] and eligible and min(v[2] for v in xyz)<=middle-extent*.2:rear+=1
    elif hit[0]:bad_red+=1
    if hit[1] and not spin and max(v[2] for v in xyz)>=middle+extent*.2:front+=1
   results.append({'edition':edition,'world':world,'car':name,'model':model,'rear_marker_faces':rear,'front_marker_faces':front,'other_marker_faces_rejected':bad_red,'texture_red_marker_pixels':sum(m[0].histogram()[255] for m in materials if m)})
missing=[x for x in results if x['texture_red_marker_pixels'] and not x['rear_marker_faces']]
absent=[x for x in results if not x['texture_red_marker_pixels']]
(root/'build/vehicle-light-mask-audit.json').write_text(json.dumps({'models':len(results),'results':results,'without_rear_marker_faces':missing,'no_original_red_markers':absent},indent=2))
print('Audited',len(results),'car/world/edition combinations; unexpected missing rear lights:',len(missing),'; no original red markers:',len(absent))
assert not missing,missing
for x in missing[:20]:print(x['world'],x['car'],x['front_marker_faces'])
