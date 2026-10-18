"""Exercise real GL output filters; prove their native framebuffer is unchanged."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess
from PIL import Image,ImageChops,ImageStat,ImageDraw
root=Path(__file__).resolve().parents[1];platform='windows' if os.name=='nt' else 'linux'
run=root/'build'/('output-filters-'+platform);run.mkdir(parents=True,exist_ok=True)
if platform=='windows':
    exe=run/'OpenMR2.exe';shutil.copy2(root/'build/OpenMR2.exe',exe)
else:exe=root/'build/openmr2_linux'
cfg=run/'revived_config/openmr2_setup.cfg';cfg.parent.mkdir(exist_ok=True)
filters=('point','linear','hq2x','brz','interlacing','crt');results=[];images={}
def launch(name,mode,filter,scene):
    receipt='\n'.join(s for s in cfg.read_text().splitlines() if s.startswith(('Files_OK =','verified_directory =','verified_language ='))) if cfg.exists() else ''
    cfg.write_text(receipt+f'\ngraphics_mode = {mode}\nretro_output_filter = {filter}\ninteger_scale = 0\nmsaa = 0\nfxaa = 0\nsounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\nmenu_music_volume = 0\nmenu_sounds_volume = 0\n')
    final=run/(name+'.bmp');native=run/(name+'-native.bmp')
    command=[str(exe),'--game',str(root.parents[1]/'Megarace 2 English'),'--size','1280x960','--shot',str(final),'--shot-native',str(native)]
    command+=['--screen','fonts'] if scene=='fonts' else ['--track','1','--frame','150','--shot-after','2','--shotfps','25']
    kwargs={};env=dict(os.environ)
    if platform=='windows':
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;kwargs['startupinfo']=startup
    else:command=['xvfb-run','-a']+command;env['LIBGL_ALWAYS_SOFTWARE']='1'
    process=subprocess.run(command,cwd=run,env=env,timeout=120,**kwargs)
    assert process.returncode==0 and final.exists() and native.exists(),name
    out=Image.open(final).convert('RGB');source=Image.open(native).convert('RGB');out.save(final.with_suffix('.png'))
    return out,source
for scene in ('fonts','race'):
    baseline=None
    for filter in filters:
        out,native=launch(scene+'-'+filter,1,filter,scene)
        signature=hashlib.sha256(native.tobytes()).hexdigest()
        assert native.size==((640,480) if scene=='fonts' else (320,400))
        if baseline is None:baseline=signature
        assert signature==baseline,scene+' '+filter+' modified the native renderer/framebuffer'
        if filter=='point':assert ImageChops.difference(out,native.resize(out.size,Image.Resampling.NEAREST)).getbbox() is None
        else:assert ImageChops.difference(out,images[scene+'-point']).getbbox(),scene+' '+filter+' silently fell back to Point'
        images[scene+'-'+filter]=out
        results.append({'scene':scene,'filter':filter,'native_sha256':signature,'pass':True})
    assert len({hashlib.sha256(images[scene+'-'+f].tobytes()).hexdigest() for f in filters})==6
    print(platform,scene,': PASS all six filters; unchanged native image',flush=True)
# Independent CPU HQ2x check of a glyph region against the upstream weight LUT.
source=Image.open(run/'fonts-hq2x-native.bmp').convert('RGB');output=images['fonts-hq2x']
lut=Image.open(root/'src/third_party/output_filters/hq2x.png').convert('RGBA');cache={}
def sample(x,y):return source.getpixel((min(max(x,0),639),479-min(max(y,0),479)))
def yuv(rgb):
    if rgb not in cache:
        r,g,b=(c/255 for c in rgb)
        cache[rgb]=(.299072266*r+.586914062*g+.114013672*b,-.168945312*r-.331054688*g+.5*b,.5*r-.418945312*g-.0809936523*b)
    return cache[rgb]
def differs(a,b):return any(abs(x-y)>t for x,y,t in zip(yuv(a),yuv(b),(.188232422,.0274505615,.0235290527)))
checked=matched=0
for yy in range(496,568):
    for xx in range(20,520):
        x=xx//2;y=(959-yy)//2;qx=xx%2;qy=(959-yy)%2;dx=qx*2-1;dy=qy*2-1
        offsets=((-1,-1),(0,-1),(1,-1),(-1,0),(1,0),(-1,1),(0,1),(1,1))
        center=sample(x,y);neighbors=[sample(x+a,y+b) for a,b in offsets]
        pattern=sum((1<<i) for i,c in enumerate(neighbors) if differs(center,c))
        cross=sum((1<<i) for i,(a,b) in enumerate(((3,1),(1,4),(6,3),(4,6))) if differs(neighbors[a],neighbors[b]))
        weights=lut.getpixel((pattern,cross*4+qx+2*qy));pixels=(center,sample(x+dx,y+dy),sample(x+dx,y),sample(x,y+dy));total=sum(weights)
        expected=tuple(round(sum(c[channel]*w for c,w in zip(pixels,weights))/total) for channel in range(3))
        actual=output.getpixel((xx,yy));matched+=max(abs(a-b) for a,b in zip(expected,actual))<=1;checked+=1
assert matched/checked>.9995,('HQ2x lookup reference mismatch',matched,checked)
results.append({'case':'hq2x_independent_cpu_lookup_reference','pixels_checked':checked,'pixels_matching_within_one':matched,'pass':True})
print(platform,': PASS independent HQ2x lookup reference',matched,'/',checked,flush=True)
point,_=launch('revived-point',0,'point','fonts');crt,_=launch('revived-crt',0,'crt','fonts')
assert ImageChops.difference(point,crt).getbbox() is None
results.append({'case':'retro_filters_have_no_effect_in_revived','pass':True})
crop=images['fonts-interlacing'].crop((1000,700,1010,732));levels={crop.getpixel((0,y)) for y in range(crop.height)}
assert len(levels)==2,('interlacing has no alternate field lines',levels)
results.append({'case':'interlacing_alternates_native_line_brightness','pass':True})
crt=images['fonts-crt'];assert len({crt.getpixel((x,704)) for x in range(1000,1006)})==3
results.append({'case':'crt_rgb_phosphor_mask','pass':True})
sheet=Image.new('RGB',(1280,3*510));draw=ImageDraw.Draw(sheet)
for i,f in enumerate(filters):
    x=i%2*640;y=i//2*510;draw.text((x+5,y+5),f,fill='white');sheet.paste(images['fonts-'+f].resize((640,480)),(x,y+25))
sheet.save(run/'review.png')
(root/'build'/(platform+'-output-filter-results.json')).write_text(json.dumps(results,indent=2))
print(platform,': PASS',len(results),'output filter checks',flush=True)
