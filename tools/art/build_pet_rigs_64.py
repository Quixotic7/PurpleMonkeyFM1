#!/usr/bin/env python3
"""Build a separate 64px art variant from original source sheets."""
import json, zipfile
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
import extract_pet_rigs as art

DEST=art.ROOT/'assets/purplemonkey/rig-64'
art.BASE=DEST
art.MAX_SIZE=64
DEST.mkdir(parents=True,exist_ok=True)
def clean_islands(im):
    a=np.array(im); todo=set(map(tuple,np.argwhere(a[:,:,3]>0))); groups=[]
    while todo:
        seed=todo.pop(); group=[seed]; pending=[seed]
        while pending:
            y,x=pending.pop()
            for dy in (-1,0,1):
                for dx in (-1,0,1):
                    q=(y+dy,x+dx)
                    if q in todo: todo.remove(q);group.append(q);pending.append(q)
        groups.append(group)
    if groups:
        largest=max(len(g) for g in groups)
        for group in groups:
            if len(group)<=max(3,largest*.015):
                for y,x in group:a[y,x,3]=0
    return Image.fromarray(a)

def split_hands(pet,rig,parts,folder):
    if pet=='llama':
        return  # Four-legged llama retains its four hooves.
    for side in ('l','r'):
        name='arm_'+side; hand='hand_'+side
        im=parts[name]; w,h=im.size; a=np.array(im)
        yy,xx=np.mgrid[0:h,0:w]; x=xx/(w-1); y=yy/(h-1)
        if pet=='dog':
            if side=='r': x=1-x
            selected=((x<.46)&(y>.32)) | (y>(.70+.42*(x-.5)))
            wrist=(round((.60 if side=='l' else .40)*(w-1)),round(.75*(h-1)))
        else:
            cut=.565 if pet=='monkey' else .49
            selected=y>=cut
            wrist=(round(.5*(w-1)),round(cut*(h-1)))
        # Share one row/pixel of original wrist art across the joint, no new colors.
        mask=Image.fromarray((selected*255).astype('uint8'))
        masks=(Image.fromarray((~selected*255).astype('uint8')).filter(ImageFilter.MaxFilter(3)),mask.filter(ImageFilter.MaxFilter(3)))
        crops=[]
        for m in masks:
            out=a.copy();out[:,:,3]=np.minimum(a[:,:,3],np.array(m))
            tile=Image.fromarray(out); box=tile.getchannel('A').getbbox()
            crops.append((tile.crop(box),box))
        arm,ab=crops[0];palm,hb=crops[1]
        arm=clean_islands(arm);palm=clean_islands(palm)
        parent=next(p for p in rig['parts'] if p['name']==name)
        parent['pivot']=[parent['pivot'][0]-ab[0],parent['pivot'][1]-ab[1]]
        hp={'name':hand,'file':hand+'.png','parent':name,'at':[wrist[0]-ab[0],wrist[1]-ab[1]],'pivot':[wrist[0]-hb[0],wrist[1]-hb[1]]}
        rig['parts'].append(hp)
        rig['order'].insert(rig['order'].index(name)+1,hand)
        parts[name]=arm;parts[hand]=palm
        arm.save(folder/(name+'.png'));palm.save(folder/(hand+'.png'))
        for state in ('play','dance'):
            for i,key in enumerate(rig['anims'][state]):
                key['a'][hand]=(8 if i%2 else -8)*(1 if side=='l' else -1)
    rig['note']='64px variant with separate wrist children. Hands include held instruments. Wrist cutouts share one pixel of source art; conservative wrist motion.'
    (folder/'rig.json').write_text(json.dumps(rig,indent=2)+'\n')

report={}
board=Image.new('RGB',(1152,4*406),art.NIGHT[:3])
labels=ImageDraw.Draw(board)
for row,pet in enumerate(art.REGIONS):
    rig,parts,faces=art.extract(pet)
    folder=DEST/pet
    for name in ('tail',):
        parts[name]=clean_islands(parts[name]);parts[name].save(folder/(name+'.png'))
    split_hands(pet,rig,parts,folder)
    def render(angles=None,pivot=False,root=(96,100)):
        return art.render(rig,parts,angles or {},root=root,pivot=pivot,size=192)
    for col,(label,angles,pivot) in enumerate((('neutral',{},False),('dance',rig['anims']['dance'][0]['a'],False),('pivots',{},True))):
        im=render(angles,pivot)
        board.paste(im.resize((384,384),Image.Resampling.NEAREST),(col*384,row*406+22))
        labels.text((col*384+12,row*406+5),pet+' / '+label,fill='white')
    render().save(folder/'assembled.png')
    render(pivot=True).resize((576,576),Image.Resampling.NEAREST).save(folder/'pivot-preview.png')
    # 20 cells, exactly 64x64 each. Body parts first, followed by six heads.
    entries=list(parts.items())+[('head_'+name,im) for name,im in faces.items()]
    atlas=Image.new('RGBA',(256,320))
    manifest={'cell_size':[64,64],'sheet':'spritesheet-64.png','frames':{}}
    sheetpreview=Image.new('RGBA',(768,1080),art.NIGHT)
    draw=ImageDraw.Draw(sheetpreview)
    for i,(name,im) in enumerate(entries):
        x,y=(i%4)*64,(i//4)*64
        ox,oy=(64-im.width)//2,(64-im.height)//2
        atlas.paste(im,(x+ox,y+oy))
        p=next(p for p in rig['parts'] if p['name']==('head' if name.startswith('head_') else name))
        manifest['frames'][name]={'cell':[x,y,64,64],'content':[x+ox,y+oy,im.width,im.height], 'file':name+'.png','pivot_in_cell':[p['pivot'][0]+ox,p['pivot'][1]+oy]}
        draw.text((i%4*192+8,i//4*216+5),name,fill='white')
        tile=Image.new('RGBA',(64,64));tile.paste(im,(ox,oy))
        sheetpreview.alpha_composite(tile.resize((192,192),Image.Resampling.NEAREST),(i%4*192,i//4*216+24))
    atlas.save(folder/'spritesheet-64.png')
    sheetpreview.save(folder/'spritesheet-preview.png')
    (folder/'spritesheet-64.json').write_text(json.dumps(manifest,indent=2)+'\n')
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
        assert max(im.size)<=64
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
board.save(DEST/'rig-overview-64.png')
(DEST/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
