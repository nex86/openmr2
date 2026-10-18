"""Launch the actual candidate AppImage with isolated settings and software GL."""
from pathlib import Path
import os,subprocess,json,hashlib
from PIL import Image
root=Path(__file__).resolve().parents[1];build=root/'build'
app=build/'OpenMR2-0.7.13-dev-x86_64.AppImage'
run=build/'appimage-smoke-0.7.13';run.mkdir(exist_ok=True)
data=run/'data';cfg=data/'OpenMR2/revived_config';cfg.mkdir(parents=True,exist_ok=True)
(cfg/'openmr2_setup.cfg').write_text('graphics_mode = 0\nvideo_upscaling = 1\nvideo_filter = 0\nframe_rate = original\nmsaa = 0\nfxaa = 0\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n')
shot=run/'advanced-graphics.bmp'
env=dict(os.environ,APPIMAGE_EXTRACT_AND_RUN='1',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(data))
subprocess.run(['xvfb-run','-a',str(app),'--game',str(root.parents[1]/'Megarace 2 English'),'--screen','gfx','--size','1280x960','--shot',str(shot)],cwd=run,env=env,timeout=180,check=True)
image=Image.open(shot).convert('RGB');assert len(image.getcolors(2000000))>100;image.save(run/'advanced-graphics.png')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(build/'openmr2_linux')==sha(build/'OpenMR2.AppDir/usr/bin/openmr2')
(build/'appimage-0713-smoke-results.json').write_text(json.dumps({'version':'0.7.13-dev','actual_appimage_launch':True,'captures':1,'failures':0,'appimage_sha256':sha(app),'tested_elf_sha256':sha(build/'openmr2_linux')},indent=2))
print('Actual AppImage launch passed; bundled ELF matches tested ELF')
