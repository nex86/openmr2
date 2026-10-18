"""Actual object rendering in all six world palettes; native tests additionally audit every HLZ pixel."""
import json
import os
import pathlib
import shutil
import subprocess
from PIL import Image,ImageDraw

root=pathlib.Path(__file__).resolve().parents[1]
platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('objects-'+platform)
run.mkdir(parents=True,exist_ok=True)
if platform=='windows':
    executable=run/'OpenMR2.exe';shutil.copy2(root/'build/OpenMR2.exe',executable);workspace=run
else:
    executable=root/'build/OpenMR2-0.7.8-dev-x86_64.AppImage';workspace=run/'data/OpenMR2'
cfg=workspace/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(parents=True,exist_ok=True)
cfg.write_text('graphics_mode = 0\ncustom_textures = 0\ndebug_console = 0\ntexture_filter = 1\nmsaa = 0\nmenu_music = 0\nsound_effects = 0\nlance_boyle_videos = 0\n')
results=[]
sheet=Image.new('RGB',(6*240,3*200))
draw=ImageDraw.Draw(sheet)
for world_index,world in enumerate(('FAC','TIB','EXT','LUN','BAY','BRO')):
    for model_index,model in enumerate(('OBJET2','OBJET1','PLOT')):
        shot=run/(world+'-'+model+'.bmp');shot.unlink(missing_ok=True)
        env=dict(os.environ);kwargs={}
        if platform=='windows':
            command=[str(executable)];startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
        else:
            command=['xvfb-run','-a',str(executable)];env.update(APPIMAGE_EXTRACT_AND_RUN='1',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(run/'data'))
        command+=['--game',str(root.parents[1]/'Megarace 2 English'),'--set','CARS/'+world,'--model',model,'--yaw','0','--pitch','10','--nohelp','--size','640x480','--shot',str(shot)]
        subprocess.run(command,cwd=run,env=env,check=True,timeout=90,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,**kwargs)
        with Image.open(shot) as img:
            crop=img.convert('RGB').crop((100,60,540,430))
            pixels=list(crop.getdata())
            # A barrel skull must contain bright neutral pixels, with no substituted neon green.
            if model=='OBJET2':
                neutral=sum(min(r,g,b)>70 and max(r,g,b)-min(r,g,b)<25 for r,g,b in pixels)
                green=sum(g>130 and g>r*2 and g>b*2 for r,g,b in pixels)
                assert neutral>25,(world,neutral,green)
                if world=='FAC':assert green==0,(world,green)
            sheet.paste(crop.resize((230,180)),(world_index*240,model_index*200+20));draw.text((world_index*240,model_index*200),world+' '+model,fill='white')
        results.append({'world':world,'model':model,'pass':True})
    print(platform,world,': PASS barrel, obstacle and cone render',flush=True)
sheet.save(run/'object-palette-review.png')
(root/'build'/(platform+'-object-results.json')).write_text(json.dumps(results,indent=2)+'\n')
