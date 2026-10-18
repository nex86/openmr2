"""Extract original HUD/symbol assets and prepare deterministic Imagegen edit targets."""
from pathlib import Path
import json,struct,hashlib,math
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parents[1];game=root.parents[1]/'Megarace 2 English';out=root/'build/ui-remaster-input';out.mkdir(exist_ok=True)
def u32(d,o):return struct.unpack_from('<I',d,o)[0]
class Iso:
 def __init__(self,p):self.f=p.open('rb');self.f.seek(16*2048);d=self.f.read(2048);self.root=(u32(d,158),u32(d,166))
 def read(self,path):
  loc=self.root
  for component in path.split('/'):
   self.f.seek(loc[0]*2048);d=self.f.read(loc[1]);o=0;found=None
   while o<len(d):
    length=d[o]
    if not length:o=(o//2048+1)*2048;continue
    name=d[o+33:o+33+d[o+32]].split(b';')[0].decode('latin1').rstrip('.')
    if name.upper()==component.upper():found=(u32(d,o+2),u32(d,o+10));break
    o+=length
   if found is None:raise FileNotFoundError(path)
   loc=found
  self.f.seek(loc[0]*2048);return self.f.read(loc[1])
iso=Iso(game/'MEGARACE2.gog');entries=[];unique={}
def add_sprite(source,palette,category):
 data=iso.read(source[3:]) if source.startswith('CD/') else (game/source[6:]).read_bytes();o=0;frame=0
 while o+24<=len(data):
  size,x0,x1,y0,y1=struct.unpack_from('<5I',data,o+4);w,h=x1-x0,y1-y0
  if size!=w*h+16:break
  pixels=data[o+24:o+24+w*h];rgba=bytes(v for index in pixels for v in (*palette[index*3:index*3+3],255 if index else 0));im=Image.frombytes('RGBA',(w,h),rgba)
  digest=hashlib.sha256(rgba).hexdigest();key=source+f'/frame_{frame:03}_pal_'+hashlib.sha256(palette).hexdigest()+'.png'
  if digest not in unique:
   asset={'id':len(entries),'category':category,'w':w,'h':h,'source_keys':[key],'source_frames':[[source,frame]],'original_png':f'original-{len(entries):03}.png'};entries.append(asset);unique[digest]=asset;im.save(out/asset['original_png'])
  else:unique[digest]['source_keys'].append(key);unique[digest]['source_frames'].append([source,frame])
  o+=8+size;frame+=1
for folder,name in (('TRACK1','FACTO'),('TRACK1','TIBET'),('TRACK1','EXTRA'),('TRACK2','LUNAR'),('TRACK2','BAYOU'),('TRACK2','BRONX')):
 pal=bytearray(iso.read(folder+'/'+name+'.HLZ')[:768]);pal[251*3:]= (game/'DATA640/PAL_RACE.HLZ').read_bytes()[251*3:768];add_sprite(f'CD/{folder}/{name}.SPR',bytes(pal),'hud')
for stem,palette,category in [('ICON','SHOPFOND','arsenal'),('SHOP','SHOPFOND','shop'),('SHOP2','SHOPFOND','shop'),('VAL','OPTION3','controls'),('BUT','OPTION3','controls'),('EXIT','OPTION3','controls'),('SAVE','SAVE2','storage'),('REPL_P','REPLAYER','storage')]:
 add_sprite('LOCAL/DATA640/'+stem+'.SPR',(game/('DATA640/'+palette+'.HLZ')).read_bytes()[:768],category)
add_sprite('LOCAL/SOURIS/FLECHE.SPR',(game/'SOURIS/FLECHE.HLZ').read_bytes()[:768],'controls')
# User scope: preserve buttons which blend into their original backgrounds.
# Keep the full source catalog, but never offer these assets to Imagegen or the
# eventual built-in remaster registry. The eleven ICON 0..10 shop tiles were
# subsequently explicitly requested; other background-integrated controls stay.
for e in entries:
 source,frame=e['source_frames'][0]
 if source=='LOCAL/DATA640/ICON.SPR' and frame<11:e['category']='arsenal_buttons'
 e['remaster_enabled']=(e['category']=='hud' or
    source=='LOCAL/DATA640/ICON.SPR' or
    source=='LOCAL/SOURIS/FLECHE.SPR')
 if not e['remaster_enabled']:
  e['exclusion_reason']='Background-integrated button: preserve original artwork.'
# One coherent sprite sheet per category, with fixed cells and no embedded labels.
for category in ('hud','arsenal','arsenal_buttons','shop','controls','storage'):
 group=[e for e in entries if e['category']==category and e['remaster_enabled']];columns=5 if category=='hud' else 2 if category=='controls' else 3 if category=='storage' else 4;rows=math.ceil(len(group)/columns);cell=256
 if not group:
  (out/(category+'-target.png')).unlink(missing_ok=True)
  (out/(category+'-manifest.json')).write_text(json.dumps({'category':category,'entries':[], 'excluded':True},indent=2))
  print(category,'preserved original; excluded from AI remaster')
  continue
 atlas=Image.new('RGBA',(columns*cell,rows*cell))
 for i,e in enumerate(group):
  im=Image.open(out/e['original_png']);ratio=min(208/im.width,180/im.height);w,h=round(im.width*ratio),round(im.height*ratio);im=im.resize((w,h),Image.Resampling.NEAREST);x=i%columns*cell+(cell-w)//2;y=i//columns*cell+(cell-h)//2;atlas.alpha_composite(im,(x,y));e['cell']=[i%columns,i//columns];e['input_rect']=[x,y,w,h]
 atlas.save(out/(category+'-target.png'))
 manifest={'category':category,'columns':columns,'rows':rows,'cell':cell,'entries':group};(out/(category+'-manifest.json')).write_text(json.dumps(manifest,indent=2))
 print(category,len(group),'unique sprites;',sum(len(e['source_keys']) for e in group),'source mappings;',atlas.size)
(out/'catalog.json').write_text(json.dumps(entries,indent=2));print('Total unique symbols:',len(entries),'; eligible:',sum(e['remaster_enabled'] for e in entries))
