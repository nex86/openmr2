"""Run the actual AppImage Retro shaders and CFG migration in all supported languages."""
import json
import os
import pathlib
import subprocess

root=pathlib.Path(__file__).resolve().parents[1]
results=[]
for language in ('English','French','German'):
    run=root/'build/retro-linux'/language
    config=run/'data/OpenMR2/revived_config'
    config.mkdir(parents=True,exist_ok=True)
    cfg=config/'openmr2_setup.cfg'
    cfg.write_text('graphics_mode = 1\ntexture_filter = 0\nmsaa = 8\nfxaa = 1\nframe_rate = 60fps\ndebug_console = 1\nsound_effects = 0\nrace_music = 0\nmenu_music = 0\nlance_boyle_videos = 0\n')
    for case in ('gfx','shop','race'):
        command=['xvfb-run','-a',str(root/'build/OpenMR2-0.7.8-dev-x86_64.AppImage'),'--game',str(root.parents[1]/('Megarace 2 '+language)),'--size','960x720','--shot',str(run/(case+'.bmp'))]
        command+=['--track','1','--frame','150','--shot-after','40','--shotfps','25'] if case=='race' else ['--screen',case]
        env=dict(os.environ,APPIMAGE_EXTRACT_AND_RUN='1',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(run/'data'))
        proc=subprocess.run(command,cwd=run,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
        (run/(case+'.log')).write_text(proc.stdout)
        text=cfg.read_text()
        expected='320x400' if case=='race' else '640x480'
        passed=proc.returncode==0 and (run/(case+'.bmp')).exists() and all(s in text for s in ('msaa = 0','fxaa = 0','frame_rate = original','revived_msaa = 8','revived_frame_rate = 1')) and ('Render target: '+expected+'; Retro Mode') in proc.stdout and not any(s in proc.stdout for s in ('shader compile error','program link error','[OpenGL error]'))
        results.append({'language':language,'case':case,'pass':passed})
        (root/'build/linux-retro-results.json').write_text(json.dumps(results,indent=2))
        if not passed:raise RuntimeError(language+' Retro '+case+' failed')
    print(language,': PASS actual AppImage Retro shaders, resolution and preference retention',flush=True)
