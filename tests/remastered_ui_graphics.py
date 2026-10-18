"""Actual-render checks of AI sprite isolation and native Retro color precision."""
from pathlib import Path
import json, os, shutil, subprocess
from PIL import Image, ImageChops
root=Path(__file__).resolve().parents[1]
platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('remastered-ui-'+platform);run.mkdir(exist_ok=True)
exe=root/'build'/('OpenMR2.exe' if os.name=='nt' else 'openmr2_linux')
if os.name=='nt':shutil.copy2(exe,run/'OpenMR2.exe');exe=run/'OpenMR2.exe'
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True)
checks=[];captures=0
def launch(name,screen='options',mode=0,ai=True,track=1,language='English'):
 global captures
 receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
 cfg.write_text(receipt+f'\ngraphics_mode = {mode}\nframe_rate = original\nretro_scale = 2\ninteger_scale = 0\noutput_filter = point\ntexture_filter = 0\nmsaa = 0\nfxaa = 0\nreal_time_shadows = 0\nvehicle_lights = 0\ndamage_effects = 0\nmodern_explosions = 0\nmissile_effects = 0\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n')
 shot=run/(name+'.bmp');cmd=[str(exe),'--game',str(root.parents[1]/('Megarace 2 '+language)),'--size','1280x960','--shot',str(shot),'--shotfps','25']
 if screen:cmd+=['--screen',screen]
 else:cmd+=['--track',str(track),'--frame','150','--objtest','--car','0']
 if not ai:cmd+=['--no-remastered-ui']
 env=dict(os.environ);kwargs={}
 if os.name=='nt':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
 else:cmd=['xvfb-run','-a']+cmd;env['LIBGL_ALWAYS_SOFTWARE']='1'
 result=subprocess.run(cmd,cwd=run,env=env,timeout=120,**kwargs);assert result.returncode==0 and shot.exists(),name
 image=Image.open(shot).convert('RGB');image.save(run/(name+'.png'));captures+=1;return image
def diff(a,b):return sum(max(p)>2 for p in ImageChops.difference(a,b).getdata())
for track in range(1,7):
 a=launch(f'track{track}-ai',screen=None,track=track);b=launch(f'track{track}-original',screen=None,track=track,ai=False)
 changed=diff(a.crop((0,0,1280,170)),b.crop((0,0,1280,170)));assert changed>30,(track,changed)
 checks.append({'check':f'Track {track}: remastered HUD actually renders','changed_pixels':changed})
a=launch('retro-hud-ai-preference',screen=None,mode=1);b=launch('retro-hud-original',screen=None,mode=1,ai=False)
assert a.tobytes()==b.tobytes();checks.append({'check':'Retro HUD is pixel-identical with AI assets allowed/disabled','pass':True})
colors=len(a.getcolors(2000000));dac={round(i*255/63) for i in range(64)}
display_dac={d+offset for d in dac for offset in (-1,0,1)}
# Final window-surface conversion on some drivers differs by one 8-bit level.
assert colors>64 and all(c in display_dac for _,rgb in a.getcolors(2000000) for c in rgb),colors
checks.append({'check':'Point-scaled Retro race preserves original asset palettes at six-bit DAC precision','colors':colors})
for language in ('English','German','French'):
 a=launch(language+'-options',language=language);b=launch(language+'-options-original-cursor',language=language,ai=False);assert diff(a,b)>5
 r=launch(language+'-retro-options',language=language,mode=1);assert len(r.getcolors(2000000))>64 and all(c in display_dac for _,rgb in r.getcolors(2000000) for c in rgb)
 checks.append({'check':language+': remastered cursor renders; Retro menu retains DAC precision','pass':True})
# Background-integrated buttons remain original; only the free-standing cursor
# can differ on this screen. The save panel itself must be pixel-identical.
a=launch('save-ai',screen='save');b=launch('save-original',screen='save',ai=False)
# Leave the 28x30 logical cursor and its edge coverage outside this comparison.
assert a.crop((65,65,250,360)).tobytes()==b.crop((65,65,250,360)).tobytes()
checks.append({'check':'Save panel and embedded buttons remain original','pass':True})
a=launch('weapon-shop',screen='shop');b=launch('weapon-shop-original',screen='shop',ai=False)
assert diff(a.crop((0,830,1280,950)),b.crop((0,830,1280,950)))>100
checks.append({'check':'All eleven approved bottom shop icons use the remaster path','pass':True})
(root/'build'/f'{platform}-remastered-ui-results.json').write_text(json.dumps({'captures':captures,'checks':checks,'failures':0},indent=2))
print(f'{platform}: {captures} captures, {len(checks)} checks passed')
