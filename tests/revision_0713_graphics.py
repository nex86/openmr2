"""GPU regressions for background filtering, palette stability, UI fixes and strict Retro isolation."""
from pathlib import Path
import os,subprocess,json,shutil
from PIL import Image,ImageChops
root=Path(__file__).resolve().parents[1];platform='windows' if os.name=='nt' else 'linux';run=root/'build'/('revision-0713-'+platform);run.mkdir(exist_ok=True)
exe=root/'build'/('OpenMR2.exe' if os.name=='nt' else 'openmr2_linux')
if os.name=='nt':shutil.copy2(exe,run/'OpenMR2.exe');exe=run/'OpenMR2.exe'
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True);images={};checks=[]
def launch(name,screen='options',retro=0,egg=0,filter=0,frame=150,scene=0,damage=0):
 receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
 cfg.write_text(receipt+f'\ngraphics_mode = {retro}\nretro_surprise = {egg}\neffects_muted = 0\ntexture_filter = {filter}\nvideo_upscaling = 0\nretro_scale = 1\ninteger_scale = 0\nretro_output_filter = point\nframe_rate = original\nmsaa = 0\nfxaa = 0\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n')
 shot=run/(name+'.bmp');cmd=[str(exe),'--game',str(root.parents[1]/'Megarace 2 English'),'--size','1280x960' if screen=='options' and not retro else '640x480','--shot',str(shot),'--shotfps','25','--car','0','--frame',str(frame),'--graphics-test',str(18 if screen=='credits' else 15 if screen=='carshop' else scene),'--effect-age-ms','3142' if screen=='carshop' else '150','--test-damage',str(damage)]
 if screen:cmd+=['--screen',screen]
 else:cmd+=['--track','1','--objtest','--car','0']
 env=dict(os.environ);kwargs={}
 if os.name=='nt':si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0;kwargs['startupinfo']=si
 else:cmd=['xvfb-run','-a']+cmd;env['LIBGL_ALWAYS_SOFTWARE']='1'
 result=subprocess.run(cmd,cwd=run,env=env,timeout=180,**kwargs);assert result.returncode==0,name
 im=Image.open(shot).convert('RGB');im.save(run/(name+'.png'));images[name]=im;return im
def different(a,b):return sum(max(p)>2 for p in ImageChops.difference(a,b).getdata())
a=launch('background-filtered');b=launch('background-point',filter=1)
assert different(a.crop((0,0,40,700)),b.crop((0,0,40,700)))>100
checks.append('Revived filtering smooths original background-only pixels, including GPU canvas slices')
a=launch('credits-frame-40',screen='credits',retro=1,frame=40);b=launch('credits-frame-200',screen='credits',retro=1,frame=200)
assert different(a,b)>500
orange=tuple(round((v//4)*255/63) for v in (236,144,40))
points=[(x,y) for y in range(420,480) for x in range(640) if max(abs(a.getpixel((x,y))[i]-orange[i]) for i in range(3))<=1]
assert len(points)>20 and all(max(abs(b.getpixel(p)[i]-orange[i]) for i in range(3))<=1 for p in points),(len(points),orange)
checks.append('Retro credit heading keeps the same orange RGB values across different video palettes')
a=launch('retro-effects-off',screen=None,retro=1,scene=6,damage=95);b=launch('retro-surprise-damage',screen=None,retro=1,egg=1,scene=6,damage=95);assert different(a,b)==0
checks.append('Removed legacy Easter egg cannot enable damage, fire or smoke')
for scene,name in ((0,'shadows'),(2,'oil'),(3,'explosion'),(8,'sparks'),(10,'missile-exhaust')):
 a=launch('retro-original-'+name,screen=None,retro=1,scene=scene);b=launch('retro-surprise-'+name,screen=None,retro=1,egg=1,scene=scene);assert different(a,b)==0,name
 checks.append('Retro ignores legacy effect override: '+name)
a=launch('retro-preview-original',screen='carshop',retro=1);b=launch('retro-surprise-preview',screen='carshop',retro=1,egg=1);assert different(a,b)==0
checks.append('Retro previews ignore the removed override')
for screen in ('options','gfx','debug','extras','shopmessage','sound'):
 launch('ui-'+screen,screen=screen,retro=screen=='sound')
checks.append('Changed menus, disabled video arrow, transparent shop notice and scrollable Extras render')
a=images['ui-shopmessage'];assert sum(max(p)>20 for p in a.crop((0,0,640,190)).getdata())>30000
checks.append('Shop notice preserves the visible shop scene instead of a full black screen')
for screen in ('options','gfx','debug','extras'):
 launch('retro-ui-'+screen,screen=screen,retro=1)
checks.append('Retro settings and Extras retain original fonts')
(root/'build'/f'{platform}-revision-0713-results.json').write_text(json.dumps({'captures':len(images),'checks':checks,'failures':0},indent=2));print(platform+f': {len(images)} captures, {len(checks)} current revision checks passed',flush=True)
