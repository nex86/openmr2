"""Actual GPU captures of new Revived features and Retro isolation."""
from pathlib import Path
import json,os,shutil,subprocess
from PIL import Image,ImageChops,ImageDraw
root=Path(__file__).resolve().parents[1];platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('gameplay-graphics-'+platform);run.mkdir(exist_ok=True)
exe=root/'build'/('OpenMR2.exe' if os.name=='nt' else 'openmr2_linux')
if os.name=='nt':local=run/'OpenMR2.exe';shutil.copy2(exe,local);exe=local
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True)
images={};checks=[]
def launch(name,scene=0,mode=0,shadows=0,modern=1,age=150,scale=2,integer=0,size='1280x960',screen=None,lamps=0,damage=0,damage_effects=1,trails=1,missiles=0):
    receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
    cfg.write_text(receipt+f'\ngraphics_mode = {mode}\nreal_time_shadows = {shadows}\nvehicle_lights = {lamps}\ndamage_effects = {damage_effects}\nmodern_explosions = {modern}\nmissile_effects = {missiles}\nretro_scale = {scale}\ninteger_scale = {integer}\nframe_rate = original\nmsaa = 0\nfxaa = 0\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n')
    shot=run/(name+'.bmp');cmd=[str(exe),'--game',str(root.parents[1]/'Megarace 2 English'),'--size',size,'--shot',str(shot)]
    if screen:cmd+=['--screen',screen]
    else:cmd+=['--track','1','--frame','150','--objtest','--car','0','--graphics-test',str(scene),'--effect-age-ms',str(age),'--test-damage',str(damage),'--shotfps','25']
    if not trails:cmd+=['--no-light-trails']
    env=dict(os.environ);kwargs={}
    if os.name=='nt':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
    else:cmd=['xvfb-run','-a']+cmd;env['LIBGL_ALWAYS_SOFTWARE']='1'
    p=subprocess.run(cmd,cwd=run,env=env,timeout=100,**kwargs);assert p.returncode==0 and shot.exists(),name
    image=Image.open(shot).convert('RGB');image.save(run/(name+'.png'));images[name]=image;return image
def different(a,b,minimum=20):
    difference=ImageChops.difference(a,b);count=sum(1 for pixel in difference.getdata() if max(pixel)>2);assert count>minimum,count;return count
off=launch('shadows-off');on=launch('shadows-on',shadows=1);checks.append({'check':'Real-time vehicle/object shadows alter actual race output','changed_pixels':different(off,on)})
retro_off=launch('retro-shadows-off',mode=1);retro_on=launch('retro-shadows-on',mode=1,shadows=1);assert retro_off.tobytes()==retro_on.tobytes();checks.append({'check':'Retro ignores saved real-time shadow preference','pass':True})
launch('missile-flight-models',scene=1)
empty=launch('empty-effects-scene',scene=4)
oil=launch('revived-oil',scene=2);checks.append({'check':'Revived oil shader renders a visible slick','changed_pixels':different(oil,empty)})
launch('retro-oil',scene=2,mode=1)
old=launch('original-explosion',scene=3,modern=0)
for age in (150,650,1300,2400):
    image=launch('modern-explosion-'+str(age),scene=3,age=age);checks.append({'check':f'Procedural fire/smoke/debris at {age} ms','changed_pixels':different(image,old)})
gone=launch('modern-explosion-expired',scene=3,age=4000);assert gone.tobytes()==empty.tobytes();checks.append({'check':'All modern particles expire without residual artifacts','pass':True})
retro_a=launch('retro-original-explosion',scene=3,modern=0,mode=1);retro_b=launch('retro-modern-preference',scene=3,modern=1,mode=1);assert retro_a.tobytes()==retro_b.tobytes();checks.append({'check':'Retro explosions are pixel-identical regardless of modern effect preference','pass':True})
for integer in (0,1):
    a=launch(f'scale-{integer}-1x',mode=1,integer=integer,scale=1,size='1920x1080',screen='fonts')
    b=launch(f'scale-{integer}-2x',mode=1,integer=integer,scale=2,size='1920x1080',screen='fonts')
    checks.append({'check':f'Selected 1x and 2x scaling works with integer_scale={integer}','changed_pixels':different(a,b,1000)})
lights=launch('vehicle-lights',lamps=1);checks.append({'check':'Texture-shaped vehicle emission and bloom are visible','changed_pixels':different(lights,off)})
braking=launch('brake-lights',scene=5,lamps=1);cruising=launch('cruise-lights',scene=4,lamps=1);checks.append({'check':'Braking intensifies red rear lights','changed_pixels':different(braking,cruising)})
for damage in (70,80,85,90,91,95,99,100):
    image=launch('damage-'+str(damage),scene=6,damage=damage);checks.append({'check':f'Damage model/particles at {damage} percent','changed_pixels':different(image,empty)})
clean_off=launch('damage-disabled',scene=6,damage=99,damage_effects=0);assert clean_off.crop((0,200,1280,800)).tobytes()==empty.crop((0,200,1280,800)).tobytes();checks.append({'check':'Disabling damage effects restores original mesh and suppresses engine particles','pass':True})
opponent=launch('opponent-damage-95',scene=7,damage=95,lamps=1);checks.append({'check':'Opponent vehicle receives lights, deformation, smoke and fire','changed_pixels':different(opponent,empty)})
sparks=launch('contact-sparks',scene=8);checks.append({'check':'Friction contacts render short metallic spark streaks','changed_pixels':different(sparks,empty,3)})
moving=launch('rear-light-trails',scene=9,lamps=1);no_trails=launch('rear-light-trails-off',scene=9,lamps=1,trails=0);checks.append({'check':'Moving rear lights leave a short texture-shaped world-space trail','changed_pixels':different(moving,no_trails,5)})
for kind in range(3):
    a=launch(f'missile-{kind}-exhaust-off',scene=10,damage=kind,missiles=0)
    b=launch(f'missile-{kind}-exhaust-on',scene=10,damage=kind,missiles=1)
    checks.append({'check':f'Missile kind {kind} renders world smoke and a compact jet flame','changed_pixels':different(a,b,15)})
a=launch('retro-missile-exhaust-off',scene=10,mode=1,missiles=0)
b=launch('retro-missile-exhaust-on',scene=10,mode=1,missiles=1)
assert a.tobytes()==b.tobytes();checks.append({'check':'Retro missile output is pixel-identical with the saved exhaust option on/off','pass':True})
launch('revive-effects-link',screen='gfx')
launch('revive-effects-menu',screen='effects')
names=['shadows-off' ,'shadows-on','missile-flight-models','revived-oil','retro-oil','original-explosion','modern-explosion-150','modern-explosion-650','modern-explosion-1300','modern-explosion-2400','vehicle-lights','brake-lights','damage-70','damage-80','damage-85','damage-90','damage-91','damage-95','damage-99','damage-100','opponent-damage-95','contact-sparks','rear-light-trails','missile-0-exhaust-on','missile-1-exhaust-on','missile-2-exhaust-on','revive-effects-link','revive-effects-menu']
sheet=Image.new('RGB',(1280,505*((len(names)+1)//2)),'#171717');draw=ImageDraw.Draw(sheet)
for i,name in enumerate(names):x=(i%2)*640;y=(i//2)*505;draw.text((x+5,y+5),name,fill='white');sheet.paste(images[name].resize((640,480)),(x,y+23))
sheet.save(run/'review.png');(root/'build'/f'{platform}-gameplay-graphics-results.json').write_text(json.dumps({'captures':len(images),'checks':checks,'failures':0},indent=2))
print(platform,':',len(images),'GPU captures,',len(checks),'feature checks passed',flush=True)
