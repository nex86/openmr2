"""Capture every vehicle in both graphics modes using real game assets and shaders."""
import json
import os
import pathlib
import shutil
import subprocess

root=pathlib.Path(__file__).resolve().parents[1]
platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('vehicles-'+platform)
config=run/('revived_config' if platform=='windows' else 'data/OpenMR2/revived_config')
config.mkdir(parents=True,exist_ok=True)
if platform=='windows':
    executable=run/'OpenMR2.exe'
    shutil.copy2(root/'build/OpenMR2.exe',executable)
else:
    executable=root/'build/OpenMR2-0.7.8-dev-x86_64.AppImage'
results=[]
for mode in (0,1):
    for car in range(14):
        cfg=config/'openmr2_setup.cfg'
        receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
        cfg.write_text(receipt+f'\ngraphics_mode = {mode}\nmsaa = 4\nfxaa = 0\ntexture_filter = 0\nframe_rate = 60fps\ndebug_console = 0\nsound_effects = 0\nmenu_music = 0\nlance_boyle_videos = 0\n')
        shot=run/f'mode{mode}-car{car:02}.bmp'
        shot.unlink(missing_ok=True)
        command=([str(executable)] if platform=='windows' else ['xvfb-run','-a',str(executable)])+['--game',str(root.parents[1]/'Megarace 2 English'),'--screen','carshop','--car',str(car),'--size','640x480','--shotfps','1000','--shot',str(shot)]
        env=dict(os.environ)
        kwargs={}
        if platform=='windows':
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
        else:
            env.update(APPIMAGE_EXTRACT_AND_RUN='1',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(run/'data'))
        result=subprocess.run(command,cwd=run,env=env,timeout=90,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,**kwargs)
        passed=result.returncode==0 and shot.exists()
        results.append({'car':car,'mode':'Retro' if mode else 'Revived','pass':passed})
        (root/'build'/(platform+'-vehicle-results.json')).write_text(json.dumps(results,indent=2))
        if not passed:raise RuntimeError(f'{platform} car {car} mode {mode} failed')
    print(platform,('Retro' if mode else 'Revived'),': PASS all 14 vehicle captures',flush=True)
