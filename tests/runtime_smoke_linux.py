import json
import os
import pathlib
import subprocess

root=pathlib.Path(__file__).resolve().parents[1]
fixtures=root.parents[1]
app=root/'build/OpenMR2-0.7.11-dev-x86_64.AppImage'
results=[]
for language in ('English','French','German'):
    run=root/'build/smoke-linux'/language
    run.mkdir(parents=True,exist_ok=True)
    data=run/'data/OpenMR2/revived_config'
    data.mkdir(parents=True,exist_ok=True)
    for case in ('menu','options','gfx','debug','padmap','gameover','hud-original','hud-60fps','quit'):
        mode=case[4:] if case.startswith('hud-') else 'original'
        cfg=data/'openmr2_setup.cfg'
        receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
        cfg.write_text(f'{receipt}\nframe_rate = {mode}\nsound_effects = 0\nrace_music = 0\nmenu_music = 0\nlance_boyle_videos = 0\n')
        shot=run/(case+'.bmp')
        cmd=['xvfb-run','-a',str(app),'--game',str(fixtures/('Megarace 2 '+language)),'--size','640x480','--shot',str(shot)]
        if case.startswith('hud-'):cmd+=['--track','1','--frame','150','--shot-after','240','--shotfps','60']
        else:cmd+=['--screen',case]
        env=dict(os.environ,APPIMAGE_EXTRACT_AND_RUN='1',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(run/'data'))
        with (run/(case+'.log')).open('w') as log:
            p=subprocess.run(cmd,cwd=run,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90)
        passed=p.returncode==0 and shot.exists()
        results.append({'language':language,'case':case,'exit_code':p.returncode,'screenshot':language+'/'+case+'.bmp','pass':passed})
        (root/'build/linux-runtime-results.json').write_text(json.dumps(results,indent=2))
        if not passed:raise RuntimeError(language+' '+case+' graphics smoke failed')
    print(language,': PASS nine actual AppImage graphics runs',flush=True)
