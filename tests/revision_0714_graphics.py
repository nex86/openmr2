from pathlib import Path
import argparse,os,shutil,subprocess,json
from PIL import Image,ImageChops
parser=argparse.ArgumentParser(description='Compare whole Retro pause-frame displacement on a real GPU.')
parser.add_argument('--binary',type=Path,required=True);parser.add_argument('--game',type=Path,required=True)
parser.add_argument('--receipt',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
exe=args.binary.resolve()
if os.name=='nt':shutil.copy2(exe,out/'OpenMR2.exe');exe=out/'OpenMR2.exe'
cfg=out/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True)
receipt='\n'.join(l for l in args.receipt.read_text().splitlines() if l.startswith(('Files_OK =','verified_directory =','verified_language =')))
imgs={}
for name,retro,scene,screen in [('menu-filtered',0,0,'menu'),('shop-filtered',0,0,'shopmessage'),('retro-base',1,21,''),('retro-pause',1,19,''),('retro-half-pause',1,20,''),('revived-pause',0,19,'')]:
 cfg.write_text(receipt+f'\ngraphics_mode = {retro}\nremember_startup_mode = 1\nstartup_mode = {retro}\nretro_scale = 1\ninteger_scale = 0\nretro_output_filter = point\ntexture_filter = 0\nmsaa = 0\nfxaa = 0\nframe_rate = original\nvideo_upscaling = 0\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n')
 cmd=[str(exe),'--game',str(args.game.resolve()),'--size','640x480','--shot',str(out/(name+'.bmp')),'--shotfps','25','--car','0','--frame','150','--graphics-test',str(scene)]
 if screen:cmd+=['--screen',screen]
 else:cmd+=['--track','1','--objtest']
 kwargs={};env=dict(os.environ)
 if os.name=='nt':
  si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;kwargs['startupinfo']=si
 else:cmd=['xvfb-run','-a']+cmd;env['LIBGL_ALWAYS_SOFTWARE']='1'
 subprocess.run(cmd,cwd=out,timeout=120,check=True,env=env,**kwargs)
 im=Image.open(out/(name+'.bmp')).convert('RGB');im.save(out/(name+'.png'));imgs[name]=im
 print('Captured',name,flush=True)
# The native 80-row DOS menu maps to 96 display pixels at 640x480.
# Compare footage, models and the in-race text below the menu.
base=imgs['retro-base'];checks=[]
for name,dy in [('retro-pause',96),('retro-half-pause',48)]:
 a=base.crop((0,110,640,270));b=imgs[name].crop((0,110+dy,640,270+dy));diff=ImageChops.difference(a,b)
 pixels=list(diff.getdata());bad=sum(max(p)>1 for p in pixels)
 checks.append({'name':name,'shift':dy,'different_pixels':bad,'total_pixels':len(pixels)})
 # Software GL can choose adjacent texels at exact sampling ties in translated
 # glyphs/triangles. Require exact footage and >99% agreement over the mixed
 # scene; the Windows GPU reference additionally retains exact agreement.
 assert ImageChops.difference(a.crop((0,0,180,160)),b.crop((0,0,180,160))).getbbox() is None
 assert bad< len(pixels)//100 and (os.name!='nt' or bad==0),(name,bad)
(out/'results.json').write_text(json.dumps({'checks':checks,'captures':len(imgs),'failures':0},indent=2));print(checks)
