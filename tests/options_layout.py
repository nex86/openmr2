"""Real screenshots of all option translations with both font paths at two sizes."""
import json, os, pathlib, shutil, subprocess, itertools, math
from PIL import Image, ImageDraw
root=pathlib.Path(__file__).resolve().parents[1]
platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('options-layout-'+platform)
run.mkdir(parents=True,exist_ok=True)
if platform=='windows':
    executable=run/'OpenMR2.exe';shutil.copy2(root/'build/OpenMR2.exe',executable)
else:
    executable=root/'build/openmr2_linux'
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True)
results=[];thumbs=[]
for language in ('German','English','French'):
    for retro in (0,1):
        screens=('options','sound','gfx','debug','quit') if retro else ('options','gfx','effects','debug','quit')
        for size,screen in itertools.product(('640x480','1280x960'),screens):
            receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
            cfg.write_text(receipt+f'\ngraphics_mode = {retro}\nsounds_volume = 25\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 40\n')
            name=f'{language}-{retro}-{screen}-{size}'
            shot=run/(name+'.bmp')
            cmd=[str(executable),'--game',str(root.parents[1]/('Megarace 2 '+language)),'--size',size,'--screen',screen,'--shot',str(shot)]
            kwargs={};env=dict(os.environ)
            if platform=='windows':
                startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
            else:cmd=['xvfb-run','-a']+cmd;env['LIBGL_ALWAYS_SOFTWARE']='1'
            process=subprocess.run(cmd,cwd=run,env=env,timeout=90,**kwargs)
            assert process.returncode==0 and shot.exists(),name
            image=Image.open(shot).convert('RGB');image.save(run/(name+'.png'))
            if size=='640x480':thumbs.append((name,image))
            results.append({'language':language,'retro':retro,'screen':screen,'size':size,'pass':True})
for language in ('German','English','French'):
    selected=[(n,i) for n,i in thumbs if n.startswith(language)]
    sheet=Image.new('RGB',(1280,math.ceil(len(selected)/2)*510),'#191919');draw=ImageDraw.Draw(sheet)
    for i,(name,im) in enumerate(selected):
        x=(i%2)*640;y=(i//2)*510;draw.text((x+5,y+5),name,fill='white');sheet.paste(im,(x,y+25))
    sheet.save(run/(language+'-review.png'))
(root/'build'/(platform+'-options-layout-results.json')).write_text(json.dumps(results,indent=2))
print(f'{platform}: PASS {len(results)} real option screens (3 languages, 2 sizes; both modes including graphics/debug/quit and Retro sound)')
