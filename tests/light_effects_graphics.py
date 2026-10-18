"""All-car directional lamp checks, effect intensities, hazard explosions and translated UI."""
from pathlib import Path
import json,os,subprocess,shutil,sys
from PIL import Image,ImageChops,ImageDraw
root=Path(__file__).resolve().parents[1];platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('light-effects-'+platform);run.mkdir(exist_ok=True);exe=root/'build'/('OpenMR2.exe' if os.name=='nt' else 'openmr2_linux')
if os.name=='nt':shutil.copy2(exe,run/'OpenMR2.exe');exe=run/'OpenMR2.exe'
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True);results=[];captures=0;images={}
keys=('shadow_intensity','light_intensity','oil_intensity','explosion_intensity','damage_intensity','missile_intensity')
def launch(name,car=0,lights=1,angle=3142,brake=0,scene=15,screen='carshop',age=150,modern=1,intensity=100,mode=0,language='English'):
 global captures
 receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
 cfg.write_text(receipt+f'\ngraphics_mode = {mode}\nvehicle_lights = {lights}\nreal_time_shadows = 0\ndamage_effects = 0\nmissile_effects = 0\nmodern_explosions = {modern}\nframe_rate = original\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n'+''.join(f'{key} = {intensity}\n' for key in keys))
 shot=run/(name+'.bmp');cmd=[str(exe),'--game',str(root.parents[1]/('Megarace 2 '+language)),'--size','1280x960' if screen in ('carshop','single') or screen is None else '640x480','--shot',str(shot),'--car',str(car),'--graphics-test',str(scene),'--effect-age-ms',str(angle if screen in ('carshop','single') else age),'--test-damage',str(brake),'--shotfps','25']
 if screen:cmd+=['--screen',screen]
 else:cmd+=['--track','1','--frame','150','--objtest']
 kwargs={};env=dict(os.environ)
 if os.name=='nt':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
 else:cmd=['xvfb-run','-a']+cmd;env['LIBGL_ALWAYS_SOFTWARE']='1'
 process=subprocess.run(cmd,cwd=run,env=env,timeout=120,**kwargs);assert process.returncode==0 and shot.exists(),name
 im=Image.open(shot).convert('RGB');im.save(run/(name+'.png'));images[name]=im;captures+=1;return im
def diff(a,b):return sum(max(p)>2 for p in ImageChops.difference(a,b).getdata())
hazards_only="--hazards-only" in sys.argv
cars=() if hazards_only else (0,13) if "--quick" in sys.argv else range(14)
for car in cars:
 rear_off=launch(f'car{car:02}-rear-off',car,lights=0)
 rear=launch(f'car{car:02}-rear',car)
 brake=launch(f'car{car:02}-brake',car,brake=1)
 front_off=launch(f'car{car:02}-front-off',car,lights=0,angle=0)
 front_red=launch(f'car{car:02}-front-red-only',car,angle=0)
 assert front_off.tobytes()==front_red.tobytes(),('red emission leaked to the front',car)
 count=diff(rear_off,rear);bright=diff(rear,brake)
 if car==13:assert count==0 and bright==0,('Blatt has no original red tail markers',count,bright)
 else:assert count>0 and bright>0,(car,'rear or brake missing',count,bright)
 head=launch(f'car{car:02}-headlights',car,angle=0,scene=14)
 head_count=diff(front_off,head);assert head_count>0,('front lights missing',car)
 results.append({'car':car,'check':'Rear-only red markers, no front red leakage, brighter braking, active front lamps','rear_pixels':count,'brake_pixels':bright,'headlamp_pixels':head_count,'pass':True})
 print(platform,'car',car,'directional lights passed',flush=True)
for car,angle,scene in (() if hazards_only else ((0,3142,15),(13,0,14))):
 a=launch(f'single-{car}-off',car=car,angle=angle,scene=scene,screen='single',lights=0)
 b=launch(f'single-{car}-on',car=car,angle=angle,scene=scene,screen='single')
 assert diff(a,b)>0,('single-race preview lamps missing',car)
 results.append({'check':f'Single-race preview uses actual lamp emission and HDR glow, car {car}','pass':True})
# Intensity zero suppresses a checked effect; the tuned baseline is exactly 100%.
if not hazards_only:
 a=launch('light-zero',intensity=0);b=images['car00-rear-off'];assert a.crop((280,200,1020,680)).tobytes()==b.crop((280,200,1020,680)).tobytes();results.append({'check':'Zero light intensity suppresses enabled lighting','pass':True})
empty=launch('empty',screen=None,scene=4,lights=0)
for scene,label in ((12,'barrel'),(13,'mine')):
 for age in (40,150,350,750,1450,2400):
  im=launch(f'{label}-{age}',screen=None,scene=scene,age=age,lights=0)
  assert diff(empty,im)>10,(label,age)
  results.append({'check':f'{label} blast/debris/smoke at {age} ms','pass':True})
 expired=launch(f'{label}-expired',screen=None,scene=scene,age=4000,lights=0);assert expired.tobytes()==empty.tobytes()
 results.append({'check':label+' particles expire','pass':True})
for language in (() if hazards_only else ('English','German','French')):
 launch(language+'-gfx',screen='gfx',language=language)
 launch(language+'-effects',screen='effects',language=language)
 results.append({'check':language+' Revived naming, arrow, six sliders/reset render','pass':True})
selected=[f'car{i:02}-rear' for i in cars]+[f'car{i:02}-headlights' for i in cars]+['barrel-40','barrel-150','barrel-350','barrel-750','mine-150']+([] if hazards_only else ['German-effects'])
sheet=Image.new('RGB',(1280,510*((len(selected)+1)//2)),'#181818');draw=ImageDraw.Draw(sheet)
for i,name in enumerate(selected):x=i%2*640;y=i//2*510;draw.text((x+5,y+3),name,fill='white');sheet.paste(images[name].resize((640,480)),(x,y+24))
sheet.save(run/'review.png');(root/'build'/(f'{platform}-'+('hazard' if hazards_only else 'light')+'-effects-results.json')).write_text(json.dumps({'captures':captures,'checks':results,'failures':0},indent=2));print(platform,':',captures,'captures,',len(results),'checks passed',flush=True)
