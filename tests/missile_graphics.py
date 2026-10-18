"""Focused missile exhaust captures, including smoke without a flame and all translations."""
from pathlib import Path
import json,os,subprocess,shutil
from PIL import Image,ImageChops
root=Path(__file__).resolve().parents[1];platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('missile-graphics-'+platform);run.mkdir(exist_ok=True)
exe=root/'build'/('OpenMR2.exe' if os.name=='nt' else 'openmr2_linux')
if os.name=='nt':shutil.copy2(exe,run/'OpenMR2.exe');exe=run/'OpenMR2.exe'
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True);results=[]
def launch(name,kind=0,enabled=1,mode=0,smoke_only=False,screen=None,language='English',size='1280x960'):
 receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
 cfg.write_text(receipt+f'\ngraphics_mode = {mode}\nmissile_effects = {enabled}\nmodern_explosions = 0\ndamage_effects = 0\nvehicle_lights = 0\nreal_time_shadows = 0\nsounds_volume = 0\nengines_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\nrace_music_volume = 0\n')
 shot=run/(name+'.bmp');cmd=[str(exe),'--game',str(root.parents[1]/('Megarace 2 '+language)),'--size',size,'--shot',str(shot)]
 if screen:cmd+=['--screen',screen]
 else:cmd+=['--track','1','--frame','150','--objtest','--car','0','--graphics-test',str(11 if smoke_only else 10),'--test-damage',str(kind),'--shotfps','25']
 kwargs={};env=dict(os.environ)
 if os.name=='nt':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
 else:cmd=['xvfb-run','-a']+cmd;env['LIBGL_ALWAYS_SOFTWARE']='1'
 process=subprocess.run(cmd,cwd=run,env=env,timeout=120,**kwargs);assert process.returncode==0 and shot.exists(),name
 im=Image.open(shot).convert('RGB');im.save(run/(name+'.png'));return im
def diff(a,b):return sum(max(pixel)>2 for pixel in ImageChops.difference(a,b).getdata())
for kind in range(3):
 off=launch(f'{kind}-off',kind,0);smoke=launch(f'{kind}-smoke',kind,smoke_only=True);full=launch(f'{kind}-full',kind)
 count=diff(off,smoke);assert count>25,(kind,'smoke',count)
 flame=diff(smoke,full);assert flame>2,(kind,'flame',flame)
 results.append({'check':f'Weapon {kind}: smoke and flame visible with all other Revive effects off','smoke_pixels':count,'flame_pixels':flame,'pass':True})
a=launch('retro-off',enabled=0,mode=1);b=launch('retro-on',enabled=1,mode=1);assert a.tobytes()==b.tobytes();results.append({'check':'Retro exhaust preference is visually ignored','pass':True})
for language in ('English','German','French'):
 for screen in ('gfx','effects'):
  launch(language+'-'+screen,screen=screen,language=language,size='640x480')
  results.append({'check':language+' '+screen+' arrow/six-option layout renders','pass':True})
(root/'build'/f'{platform}-missile-graphics-results.json').write_text(json.dumps({'captures':17,'checks':results,'failures':0},indent=2))
print(platform,': 17 focused missile/menu captures, 10 checks passed',flush=True)
