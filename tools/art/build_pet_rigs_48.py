#!/usr/bin/env python3
"""Build a separate 48px art variant from original source sheets."""
import json, zipfile
import numpy as np
from PIL import Image, ImageDraw
import extract_pet_rigs as art

DEST=art.ROOT/'assets/purplemonkey/rig-48'
art.BASE=DEST
art.MAX_SIZE=48
DEST.mkdir(parents=True,exist_ok=True)
report={}
board=Image.new('RGB',(1152,4*406),art.NIGHT[:3])
labels=ImageDraw.Draw(board)
for row,pet in enumerate(art.REGIONS):
    rig,parts,faces=art.extract(pet)
    folder=DEST/pet
    def render(angles=None,pivot=False,root=(96,100)):
        return art.render(rig,parts,angles or {},root=root,pivot=pivot,size=192)
    for col,(label,angles,pivot) in enumerate((('neutral',{},False),('dance',rig['anims']['dance'][0]['a'],False),('pivots',{},True))):
        im=render(angles,pivot)
        board.paste(im.resize((384,384),Image.Resampling.NEAREST),(col*384,row*406+22))
        labels.text((col*384+12,row*406+5),pet+' / '+label,fill='white')
    render().save(folder/'assembled.png')
    render(pivot=True).resize((576,576),Image.Resampling.NEAREST).save(folder/'pivot-preview.png')
    # 16 cells, exactly 48x48 each. Body parts first, followed by six heads.
    entries=list(parts.items())+[('head_'+name,im) for name,im in faces.items()]
    atlas=Image.new('RGBA',(192,192))
    manifest={'cell_size':[48,48],'sheet':'spritesheet-48.png','frames':{}}
    sheetpreview=Image.new('RGBA',(768,864),art.NIGHT)
    draw=ImageDraw.Draw(sheetpreview)
    for i,(name,im) in enumerate(entries):
        x,y=(i%4)*48,(i//4)*48
        ox,oy=(48-im.width)//2,(48-im.height)//2
        atlas.paste(im,(x+ox,y+oy))
        p=next(p for p in rig['parts'] if p['name']==('head' if name.startswith('head_') else name))
        manifest['frames'][name]={'cell':[x,y,48,48],'content':[x+ox,y+oy,im.width,im.height], 'file':name+'.png','pivot_in_cell':[p['pivot'][0]+ox,p['pivot'][1]+oy]}
        draw.text((i%4*192+8,i//4*216+5),name,fill='white')
        tile=Image.new('RGBA',(48,48));tile.paste(im,(ox,oy))
        sheetpreview.alpha_composite(tile.resize((192,192),Image.Resampling.NEAREST),(i%4*192,i//4*216+24))
    atlas.save(folder/'spritesheet-48.png')
    sheetpreview.save(folder/'spritesheet-preview.png')
    (folder/'spritesheet-48.json').write_text(json.dumps(manifest,indent=2)+'\n')
    seq=[]
    for name,im in faces.items():
        alt=dict(parts);alt['head']=im
        seq.append(art.render(rig,alt,root=(96,100),size=192).resize((384,384),Image.Resampling.NEAREST).convert('RGB'))
    seq[0].save(folder/'expressions-preview.gif',save_all=True,append_images=seq[1:],duration=[750,150,600,600,600,600],loop=0)
    motion=[];durations=[]
    for state,keys in rig['anims'].items():
        for j,k in enumerate(keys):
            nxt=keys[(j+1)%len(keys)]
            for step in range(8):
                t=step/8
                angles={n:k['a'].get(n,0)*(1-t)+nxt['a'].get(n,0)*t for n in parts}
                root=tuple(base+k['root'][i]*(1-t)+nxt['root'][i]*t for i,base in enumerate((96,100)))
                motion.append(render(angles,root=root).resize((384,384),Image.Resampling.NEAREST).convert('RGB'))
                durations.append(max(20,round(k['ms']/8/10)*10))
    motion[0].save(folder/'motion-preview.gif',save_all=True,append_images=motion[1:],duration=durations,loop=0)
    colors=set()
    for im in list(parts.values())+list(faces.values()):
        assert max(im.size)<=48
        a=np.array(im);assert set(np.unique(a[:,:,3]))<={0,255}
        colors.update(map(tuple,a[a[:,:,3]>0,:3].tolist()))
    assert len(colors)<=31
    assert all(im.size==parts['head'].size for im in faces.values())
    for p in rig['parts']:
        w,h=parts[p['name']].size
        assert 0<=p['pivot'][0]<w and 0<=p['pivot'][1]<h
        if p['parent']:
            w,h=parts[p['parent']].size
            assert 0<=p['at'][0]<w and 0<=p['at'][1]<h
    for name,f in manifest['frames'].items():
        x,y,w,h=f['content']
        assert atlas.crop((x,y,x+w,y+h)).tobytes()==Image.open(folder/f['file']).tobytes()
    report[pet]={'parts':len(parts),'expressions':len(faces),'max_dimension':max(max(im.size) for im in parts.values()),'opaque_colors':len(colors),'body_pixel_bytes':sum(im.width*im.height for im in parts.values()),'extra_face_pixel_bytes':sum(im.width*im.height for name,im in faces.items() if name!='neutral')}
board.save(DEST/'rig-overview-48.png')
(DEST/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
