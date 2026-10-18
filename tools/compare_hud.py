"""Deterministic, labeled pixel crops for local review; reference artwork is not packaged."""
from PIL import Image, ImageDraw
import pathlib

root=pathlib.Path(__file__).resolve().parents[1]
dest=root.parent/'releases/0.7.0-dev/comparisons'
dest.mkdir(parents=True,exist_ok=True)
reference=Image.open(root.parent/'Anweisungen/hud.png').convert('RGB')
current=Image.open(root/'build/smoke-windows/German/hud-original.bmp').convert('RGB')
# The supplied original capture is 640 pixels wide. Sprite widths match the
# original 320-column HUD expanded to our 640-column canvas; retain pixel aspect.
panels=[
 ('Original reference (provided)',reference.crop((194,267,834,327))),
 ('Port 0.7.0-dev (actual Windows run)',current.crop((0,30,640,90))),
]
sheet=Image.new('RGB',(900,202),'#17212b');draw=ImageDraw.Draw(sheet)
for i,(label,im) in enumerate(panels):
    y=i*90+12;draw.text((12,y+16),label,fill='white');sheet.paste(im,(250,y))
draw.text((12,190),'Different real race states; ammo/rank/fill values are not expected to match.',fill='white')
sheet.save(dest/'hud-comparison.png')
regions={'missile':(0,0,82,54),'mine':(82,0,154,54),'armor':(154,0,236,30),'armor-bar':(154,24,236,46),'damage-bar':(394,7,478,26)}
for name,box in regions.items():
    tile=Image.new('RGB',(600,230),'#17212b');draw=ImageDraw.Draw(tile)
    for i,(label,im) in enumerate(panels):
        x=i*300;draw.text((x+10,8),label,fill='white')
        crop=im.crop(box);crop=crop.resize((crop.width*3,crop.height*3),Image.Resampling.NEAREST);tile.paste(crop,(x+10,40))
    draw.text((10,215),name+' | nearest-neighbor 3x detail after common canvas normalization',fill='white')
    tile.save(dest/(name+'.png'))
(dest/'README.txt').write_text('Local review artwork; excluded from source and binary archives.\nThe supplied original reference is compared at a common 640-pixel HUD canvas width, matched by the known original sprite widths.\nOriginal reference crop: x=194, y=267, width=640. Current actual Windows run: 640x480, HUD origin y=30.\nThe already differently scaled upper "old port" panel is not used to infer raw HUD dimensions.\nThe montage is an assessment aid, not a same-state original/port golden-frame test.\nAmmo, shield, damage, rank, lap and scenery may differ. HUD typography/layout still need review.\n')
print('Created',len(regions)+1,'labeled HUD comparisons')
