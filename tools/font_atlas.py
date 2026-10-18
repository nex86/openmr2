import pathlib
from PIL import Image, ImageDraw
from inspect_assets import picture
import sys

w,h,pal,px=picture(pathlib.Path(sys.argv[1]))
y=0;ch=32;glyphs=[]
while y<h:
    def inkrow(y): return any(c!=255 for c in px[y*w:(y+1)*w])
    while y<h and not inkrow(y):y+=1
    y0=y
    while y<h and inkrow(y):y+=1
    y1=y
    if y1-y0<4 or y0>380:break
    x=0
    def sep(x):return all(px[yy*w+x]==255 for yy in range(y0,y1))
    while x<w:
        while x<w and sep(x):x+=1
        x0=x
        while x<w and not sep(x):x+=1
        if x-x0<1:break
        if x-x0<=2:continue
        glyphs.append((ch,x0,y0,x-x0,y1-y0));ch+=1
im=Image.new('RGB',(900,300),(20,30,40));draw=ImageDraw.Draw(im)
for i,(ch,x,y,gw,gh) in enumerate(g for g in glyphs if g[0]>=123):
    gx=i%12*75;gy=i//12*90
    draw.text((gx,gy),hex(ch),fill='white')
    for yy in range(gh):
        for xx in range(gw):
            if px[(y+yy)*w+x+xx] in (254,1):
                draw.rectangle((gx+xx*2,gy+20+yy*2,gx+xx*2+1,gy+21+yy*2),fill='white')
im.save(sys.argv[2])
