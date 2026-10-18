"""Real catalog export and GPU replacements; requires the original English GOG fixture and Pillow."""
import json
import hashlib
import os
import pathlib
import re
import shutil
import subprocess
import tempfile
from PIL import Image

root=pathlib.Path(__file__).resolve().parents[1]
platform='windows' if os.name=='nt' else 'linux'
run=(root/'build'/('custom-textures-final-076-'+platform)) if platform=='windows' else pathlib.Path(tempfile.gettempdir())/'openmr2-custom-textures-076'
run.mkdir(parents=True,exist_ok=True)
if platform=='windows':
    executable=run/'OpenMR2.exe'
    shutil.copy2(root/'build/OpenMR2.exe',executable)
    workspace=run
else:
    executable=root/'build/OpenMR2-0.7.8-dev-x86_64.AppImage'
    workspace=run/'data/OpenMR2'
config=workspace/'revived_config/openmr2_setup.cfg'
config.parent.mkdir(parents=True,exist_ok=True)
results=[]

def settings(custom=0,retro=0):
    receipt='\n'.join(s for s in config.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if config.exists() else ''
    config.write_text(receipt+f'\ngraphics_mode = {retro}\ncustom_textures = {custom}\ndump_textures = 0\nmsaa = 0\nfxaa = 0\ntexture_filter = 1\nframe_rate = original\ndebug_console = 0\nsound_effects = 0\nmenu_music = 0\nlance_boyle_videos = 0\n')

def launch(arguments):
    env=dict(os.environ)
    kwargs={}
    command=[str(executable)]
    if platform=='windows':
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
    else:
        command=['xvfb-run','-a',str(executable)]
        env.update(APPIMAGE_EXTRACT_AND_RUN='1',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(run/'data'))
    command+=['--game',str(root.parents[1]/'Megarace 2 English')]+arguments
    subprocess.run(command,cwd=run,env=env,timeout=1200,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,**kwargs)

settings()
progress_shot=run/'dump-progress.bmp'
launch(['--screen','debug','--size','640x480','--dump-textures','--shot-after','30','--shot',str(progress_shot)])
with Image.open(progress_shot) as progress:
    assert progress.getpixel((168,418))[:3]==(150,140,120),'Export loading-bar frame is missing'
    assert progress.getpixel((180,426))[:3]==(236,144,40),'Export progress fill is missing'
results.append({'case':'one_shot_export_uses_track_loading_progress_bar','pass':True})
settings()
launch(['--screen','debug','--size','640x480','--dump-textures','--quit','900'])
dump=workspace/'revived_config/texture_dumps'
status=(dump/'status.txt').read_text()
counts=re.search(r'Catalog assets: (\d+) / (\d+).*PNG exports: (\d+).*Errors: (\d+)',status,re.S)
assert counts and counts[1]==counts[2] and int(counts[3])>1000 and int(counts[4])==0,status
files=list(dump.rglob('*.png'))
assert len(files)==int(counts[3]),(len(files),counts[3])
assert list(dump.glob('LOCAL/CARS/FAC/*.M3D.txt'))
assert list(dump.glob('LOCAL/OBJS/TIB/*.HLZ.png'))
assert list(dump.glob('CD/TRACK1/FACTO.SPR/*.png'))
for p in files:
    with Image.open(p) as img:
        assert img.mode=='RGBA',str(p)
        img.verify()
results.append({'case':'full_catalog_original_png_alpha_and_material_metadata','png_files':len(files),'pass':True})
print(platform,': PASS full original catalog export:',len(files),'PNG files',flush=True)

replacement=workspace/'revived_config/texture_replacements'
background=replacement/'LOCAL/DATA640/MENUFOND.HLZ.png'
background.parent.mkdir(parents=True,exist_ok=True)
img=Image.new('RGBA',(1280,960),(12,34,210,255))
for y in range(128,256):
    for x in range(8,136):img.putpixel((x,y),(200,25,150,255) if (x+y)%2 else (20,210,180,255))
img.save(background)
for p in dump.glob('LOCAL/CARS/FAC/*.HLZ.png'):
    with Image.open(p) as original:w,h=original.size
    target=replacement/p.relative_to(dump);target.parent.mkdir(parents=True,exist_ok=True)
    Image.new('RGBA',(w*2,h*2),(10,250,20,255)).save(target)

def capture(name,custom=0,retro=0,screen='menu'):
    settings(custom,retro)
    shot=run/(name+'.bmp');shot.unlink(missing_ok=True)
    launch(['--screen',screen,'--car','0','--size','1280x960','--shotfps','1000','--shot',str(shot)])
    return Image.open(shot).convert('RGB')

baseline=capture('menu-original')
custom=capture('menu-custom',1)
assert custom.size==(1280,960)
pixels=list(custom.get_flattened_data() if hasattr(custom,'get_flattened_data') else custom.getdata())
assert sum(c==(12,34,210) for c in pixels)>100000
assert sum(c==(200,25,150) for c in pixels)>1000 and sum(c==(20,210,180) for c in pixels)>1000,'High-resolution checkerboard was reduced to the original canvas'
assert baseline.tobytes()!=custom.tobytes()
results.append({'case':'high_resolution_background_retains_one_pixel_detail','pass':True})
shop_original=capture('shop-original',screen='carshop')
shop_custom=capture('shop-custom',1,screen='carshop')
assert sum(g>r*2 and g>b*2 for r,g,b in shop_custom.crop((260,200,1020,600)).getdata())>1500
assert shop_original.tobytes()!=shop_custom.tobytes()
results.append({'case':'high_resolution_model_texture_uses_original_uv_layout','pass':True})
retro_original=capture('retro-original',0,1)
retro_custom=capture('retro-custom-suppressed',1,1)
assert retro_original.tobytes()==retro_custom.tobytes(),'Custom textures leaked into Retro'
results.append({'case':'retro_suppresses_replacements_pixel_identical','pass':True})
# Replace a UI checkbox, retaining fully transparent and partially transparent halves.
palette=(root.parents[1]/'Megarace 2 English/DATA640/OPTION3.HLZ').read_bytes()[:768]
hash_id=hashlib.sha256(palette).hexdigest()
source=dump/'LOCAL/DATA640/VAL.SPR'/('frame_000_pal_'+hash_id+'.png')
assert source.exists(),source
with Image.open(source) as original:sw,sh=original.size
checkbox=replacement/source.relative_to(dump);checkbox.parent.mkdir(parents=True,exist_ok=True)
ui=Image.new('RGBA',(sw*2,sh*2),(255,0,255,128))
for y in range(sh*2):
    for x in range(sw):ui.putpixel((x,y),(255,0,255,0))
ui.save(checkbox)
Image.new('RGBA',(1280,960),(20,40,60,255)).save(replacement/'LOCAL/DATA640/OPTION3.HLZ.png')
gfx_original=capture('gfx-original',0,screen='gfx')
gfx_custom=capture('gfx-alpha-custom',1,screen='gfx')
assert gfx_original.tobytes()!=gfx_custom.tobytes()
# The left half remains the original background; the right half blends the new PNG.
assert gfx_custom.getpixel((80,100))==(20,40,60),gfx_custom.getpixel((80,100))
assert all(abs(a-b)<=1 for a,b in zip(gfx_custom.getpixel((110,100)),(138,20,158))),gfx_custom.getpixel((110,100))
results.append({'case':'sprite_palette_identity_and_partial_alpha_composition','pass':True})
# Scaled font atlas is sampled at original glyph positions, preserving screen layout.
source=dump/'LOCAL/TEXTE/HNDEL14.HLZ.png'
with Image.open(source) as original:
    atlas=original.resize((original.width*2,original.height*2),Image.Resampling.NEAREST)
font_path=replacement/source.relative_to(dump);font_path.parent.mkdir(parents=True,exist_ok=True);atlas.save(font_path)
font_custom=capture('gfx-font-atlas-custom',1,screen='gfx')
assert font_custom.size==gfx_custom.size
assert font_custom.getpixel((430,50))==gfx_custom.getpixel((430,50)),'Neutral font atlas changed the requested UI text color'
results.append({'case':'higher_resolution_font_atlas_with_original_glyph_metrics','pass':True})
# Every HUD frame/palette variant is eligible independently of its source atlas.
for source in dump.glob('CD/TRACK1/FACTO.SPR/*.png'):
    with Image.open(source) as original:sw,sh=original.size
    target=replacement/source.relative_to(dump);target.parent.mkdir(parents=True,exist_ok=True)
    Image.new('RGBA',(sw*2,sh*2),(255,0,255,128)).save(target)
settings(0)
shot=run/'hud-original.bmp';launch(['--track','1','--frame','150','--shot-after','40','--shotfps','25','--shot',str(shot)])
settings(1)
changed=run/'hud-custom.bmp';launch(['--track','1','--frame','150','--shot-after','40','--shotfps','25','--shot',str(changed)])
with Image.open(shot) as original,Image.open(changed) as modified:
    assert original.crop((0,0,640,100)).tobytes()!=modified.crop((0,0,640,100)).tobytes()
results.append({'case':'disc_track_hud_replacements_with_partial_alpha','pass':True})
# Remove the temporary atlas and checkbox before checking full original fallback.
font_path.unlink();checkbox.unlink()
background.write_bytes(b'not a PNG')
invalid=capture('menu-invalid-fallback',1)
assert baseline.tobytes()==invalid.tobytes(),'Invalid PNG did not fall back to original'
results.append({'case':'invalid_png_falls_back_pixel_identical','pass':True})
(root/'build'/(platform+'-custom-texture-results.json')).write_text(json.dumps(results,indent=2)+'\n')
print(platform,': PASS real high-resolution UI/model replacement, Retro isolation and invalid-PNG fallback',flush=True)
