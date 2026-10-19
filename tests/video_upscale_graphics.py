"""Exercise real GPU video filters on track footage and an original cinematic."""
from pathlib import Path
import json,os,shutil,subprocess
from PIL import Image,ImageChops
root=Path(__file__).resolve().parents[1];platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('video-upscale-'+platform);run.mkdir(exist_ok=True)
exe=root/'build'/('OpenMR2.exe' if os.name=='nt' else 'openmr2_linux')
if os.name=='nt':shutil.copy2(exe,run/'OpenMR2.exe');exe=run/'OpenMR2.exe'
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True)
checks=[];captures=0;profiles={}
def launch(name,filter=0,enabled=1,mode=0,clip=False,screen=None):
 global captures
 receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
 cfg.write_text(receipt+f'\ngraphics_mode = {mode}\nvideo_upscaling = {enabled}\nvideo_filter = {filter}\nframe_rate = original\nretro_scale = 2\ninteger_scale = 0\nretro_output_filter = point\nmsaa = 0\nfxaa = 0\nreal_time_shadows = 0\nvehicle_lights = 0\ndamage_effects = 0\nmodern_explosions = 0\nmissile_effects = 0\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n')
 shot=run/(name+'.bmp');cmd=[str(exe),'--game',str(root.parents[1]/'Megarace 2 English'),'--size','1280x960','--shot',str(shot),'--shotfps','25','--graphics-test','16']
 if screen:cmd+=['--screen',screen]
 elif clip:cmd+=['--screen','clip:BOYLE/INTRO25.UBB']
 else:cmd+=['--track','1','--frame','150','--car','0','--objtest']
 profile=run/'video-upscale-profile.txt';profile.unlink(missing_ok=True)
 (run/'video-filter-error.txt').unlink(missing_ok=True)
 kwargs={};env=dict(os.environ)
 if os.name=='nt':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
 else:cmd=['xvfb-run','-a']+cmd;env['LIBGL_ALWAYS_SOFTWARE']='1'
 result=subprocess.run(cmd,cwd=run,env=env,timeout=240,**kwargs);assert result.returncode==0 and shot.exists(),name
 image=Image.open(shot).convert('RGB');image.save(run/(name+'.png'));captures+=1
 if profile.exists():profiles[name]=profile.read_text()
 return image
def diff(a,b):return sum(max(p)>2 for p in ImageChops.difference(a,b).getdata())
original=launch('track-original',enabled=0);cinematic=launch('cinematic-original',enabled=0,clip=True)
for filter,name in enumerate(('FSR','HQ4x','4xBR','Lanczos3','FSRCNNX-AI','FSRCNNX-AI-HQ')):
 a=launch('track-'+name,filter);changed=diff(a,original);assert changed>500,(name,changed)
 assert len(a.crop((40,200,400,700)).getcolors(1000000))>100,(name,'video detail lost')
 assert 'track-'+name in profiles,(name,'filter silently fell back or did not run')
 b=launch('cinematic-'+name,filter,clip=True);changed_clip=diff(b,cinematic);assert changed_clip>500,(name,changed_clip)
 checks.append({'check':name+' GPU filter changes both track footage and cinematic output','track_pixels':changed,'cinematic_pixels':changed_clip})
 if filter==3:lanczos_track,lanczos_cinematic=a,b
 if filter>=4:
  learned_track,learned_cinematic=diff(a,lanczos_track),diff(b,lanczos_cinematic)
  assert learned_track>500 and learned_cinematic>500,(name,'neural model silently fell back to Lanczos3')
  checks.append({'check':name+' runs learned reconstruction rather than its Lanczos3 fallback','track_pixels':learned_track,'cinematic_pixels':learned_cinematic})
 repeated=launch('track-'+name+'-repeat',filter);assert a.tobytes()==repeated.tobytes(),name
 checks.append({'check':name+' produces deterministic output for identical source frames (no random detail/history)','pass':True})
 a=launch('retro-'+name,filter,mode=1);b=launch('retro-'+name+'-disabled',filter,enabled=0,mode=1);assert a.tobytes()==b.tobytes(),name
 checks.append({'check':name+' is pixel-identically bypassed in Retro','pass':True})
launch('advanced-video-controls',screen='gfx')
launch('retro-controls-hidden',screen='gfx',mode=1)
(root/'build'/f'{platform}-video-upscale-results.json').write_text(json.dumps({'captures':captures,'checks':checks,'profiles':profiles,'failures':0},indent=2))
print(f'{platform}: {captures} GPU captures, {len(checks)} video checks passed')
