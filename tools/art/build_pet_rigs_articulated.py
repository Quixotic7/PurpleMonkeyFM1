#!/usr/bin/env python3
"""Extract generated round-ended limb kit and assemble articulated 64px rigs."""
import json, math, importlib.util
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageOps
import extract_pet_rigs as art
BASE=art.ROOT/'assets/purplemonkey/rig-64-articulated'
OLD=art.ROOT/'assets/purplemonkey/rig-64'
PETS=['monkey','cat','dog','llama']
EXPRS=art.EXPRS

def crop_solid(im):
    im=art.solid(im);box=im.getchannel('A').getbbox()
    assert box
    return im.crop(box)

def build():
    source=Image.open(BASE/'source/limb-kit.png').convert('RGBA')
    board=Image.new('RGB',(1536,4*540),art.NIGHT[:3]);bd=ImageDraw.Draw(board)
    reports={}
    for row,pet in enumerate(PETS):
        d=BASE/pet;d.mkdir(parents=True,exist_ok=True)
        oldrig=json.loads((OLD/pet/'rig.json').read_text())
        old={p['name']:p for p in oldrig['parts']}
        parts={};rig={'parts':[],'order':[],'feet':0,'anims':{}}
        def add(name,im,parent=None,at=None,pivot=None):
            parts[name]=im
            rig['parts'].append({'name':name,'file':name+'.png','parent':parent,'at':at,'pivot':pivot or [im.width//2,im.height//2]})
        retained=['body','head','tail']+(['ear_l','ear_r'] if pet=='dog' else [])+(['neck','collar'] if pet=='llama' else [])+(['drum'] if pet=='monkey' else ['xylophone'] if pet=='cat' else [])
        # Retain original art and local attachment geometry, topologically ordered later.
        for n in retained:
            p=old[n];add(n,Image.open(OLD/pet/p['file']).convert('RGBA'),p['parent'],p['at'],p['pivot'])
        cells=[]
        for col in range(6):
            box=(round(col*source.width/6),round(row*source.height/4),round((col+1)*source.width/6),round((row+1)*source.height/4))
            cells.append(crop_solid(source.crop(box)))
        # One scale for the row; complete limb segments are never independently stretched.
        segment_indices=[0,1,3,4]
        scale=min(62/max(max(im.size) for im in cells), 29/(sum(cells[i].height for i in segment_indices)/4))
        cells=[art.solid(im.resize((max(1,round(im.width*scale)),max(1,round(im.height*scale))),Image.Resampling.LANCZOS)) for im in cells]
        def point(im,x,y):return [round((im.width-1)*x),round((im.height-1)*y)]
        limb_groups=[]
        def chain(prefix,suffix,images,attach,mirror=False,hand=False):
            names=([f'upper_arm_{suffix}',f'forearm_{suffix}',f'hand_{suffix}'] if hand else [f'upper_leg_{suffix}',f'lower_leg_{suffix}',f'foot_{suffix}'])
            images=[ImageOps.mirror(im) if mirror else im.copy() for im in images]
            pa='body';at=attach
            for i,(name,im) in enumerate(zip(names,images)):
                # Segment pivots sit well inside the circular caps, providing overlap.
                pv=point(im,.5,.22 if i<2 else (.20 if not hand else .15))
                if i==2 and not hand:pv=point(im,.72 if not mirror else .28,.22)
                if i==2 and hand and pet=='dog':pv=point(im,.25 if mirror else .75,.70)
                add(name,im,pa,at,pv)
                pa=name;at=point(im,.5,.78)
            limb_groups.append(names)
            return names
        legorders=[];armorders=[]
        if pet!='llama':
            for side in ('l','r'):
                armorders.append(chain('arm',side,cells[:3],old['arm_'+side]['at'],side=='r',True))
                legorders.append(chain('leg',side,cells[3:],old['leg_'+side]['at'],side=='r'))
            rig['order']=['tail']+sum(legorders,[])+armorders[0]+['body']
            if pet=='dog':rig['order']+=['ear_l','head','ear_r']
            else:rig['order']+=['head','drum' if pet=='monkey' else 'xylophone']
            rig['order']+=armorders[1]
        else:
            for suffix in ('far_l','far_r','near_l','near_r'):
                # Viewer-left is the hind pair; right is the front pair.
                ims=cells[3:] if suffix.endswith('l') else cells[:3]
                legorders.append(chain('leg',suffix,ims,old['leg_'+suffix]['at']))
            rig['order']=['tail']+sum(legorders[:2],[])+['body']+sum(legorders[2:],[])+['neck','head','collar']
        # Topological ordering for the preview solver and runtime.
        todo=rig['parts'];seq=[];seen=set()
        while todo:
            ready=[p for p in todo if p['parent'] is None or p['parent'] in seen]
            assert ready
            seq+=ready;seen.update(p['name'] for p in ready);todo=[p for p in todo if p not in ready]
        rig['parts']=seq
        faces={e:Image.open(OLD/pet/('head_'+e+'.png')).convert('RGBA') for e in EXPRS}
        ims=list(parts.values())+list(faces.values())
        pixels=np.concatenate([np.array(im)[np.array(im)[:,:,3]>0,:3] for im in ims])
        pal=Image.fromarray(pixels.reshape(1,-1,3)).quantize(31,method=Image.Quantize.MEDIANCUT)
        def quant(im):
            q=im.convert('RGB').quantize(palette=pal,dither=Image.Dither.NONE).convert('RGBA');q.putalpha(im.getchannel('A'));return q
        parts={n:quant(im) for n,im in parts.items()};faces={n:quant(im) for n,im in faces.items()}
        parts['head']=faces['neutral'].copy()
        def key(a,ms=500):return {'ms':ms,'root':[0,0],'a':a}
        bent={};opposite={}
        for names in limb_groups:
            sign=1 if names[0].endswith('_l') else -1
            isarm=names[0].startswith('upper_arm')
            bent.update({names[0]:sign*(-12 if isarm else 8),names[1]:sign*(45 if isarm else -28),names[2]:sign*(-12 if isarm else 15)})
            opposite.update({names[0]:-sign*8,names[1]:sign*(15 if isarm else 12),names[2]:-sign*6})
        rig['anims']={'idle':[key({'head':-2},1200),key({'head':2},1200)],'play':[key(bent,350),key(opposite,350)],'dance':[key(bent),key(opposite)]}
        poses=art.solve(rig,{},(0,0))
        rig['feet']=math.ceil(max(poses[n][1]-next(p['pivot'][1] for p in seq if p['name']==n)+im.height for n,im in parts.items()))
        rig['note']='Articulated 64px option. Round-ended generated limbs; forearms and shins overlap at elbows/knees. Requires part capacity >=17. l/r mean viewer sides.'
        (d/'rig.json').write_text(json.dumps(rig,indent=2)+'\n')
        hp=next(p['pivot'] for p in seq if p['name']=='head')
        (d/'expressions.json').write_text(json.dumps({'part':'head','canvas':list(parts['head'].size),'pivot':hp,'frames':{e:'head_'+e+'.png' for e in EXPRS}},indent=2)+'\n')
        for n,im in parts.items():im.save(d/(n+'.png'))
        for n,im in faces.items():im.save(d/('head_'+n+'.png'))
        entries=list(parts.items())+[('head_'+e,im) for e,im in faces.items()]
        atlas=Image.new('RGBA',(384,256));preview=Image.new('RGBA',(960,736),art.NIGHT);dr=ImageDraw.Draw(preview)
        manifest={'cell_size':[64,64],'sheet':'spritesheet-64.png','frames':{}}
        for i,(n,im) in enumerate(entries):
            x,y=i%6*64,i//6*64;ox,oy=(64-im.width)//2,(64-im.height)//2
            atlas.paste(im,(x+ox,y+oy));p=next(p for p in seq if p['name']==('head' if n.startswith('head_') else n))
            manifest['frames'][n]={'cell':[x,y,64,64],'content':[x+ox,y+oy,im.width,im.height],'file':n+'.png','pivot_in_cell':[p['pivot'][0]+ox,p['pivot'][1]+oy]}
            tile=Image.new('RGBA',(64,64));tile.paste(im,(ox,oy));preview.alpha_composite(tile.resize((160,160),Image.Resampling.NEAREST),(i%6*160,i//6*184+24));dr.text((i%6*160+4,i//6*184+5),n,fill='white')
        atlas.save(d/'spritesheet-64.png');preview.save(d/'spritesheet-preview.png');(d/'spritesheet-64.json').write_text(json.dumps(manifest,indent=2)+'\n')
        def render(angles={},pivot=False,ps=None):return art.render(rig,ps or parts,angles,root=(128,136),pivot=pivot,size=256)
        for col,(label,angles,pivot) in enumerate([('neutral',{},False),('elbows + knees',bent,False),('pivots',bent,True)]):
            im=render(angles,pivot).resize((512,512),Image.Resampling.NEAREST);board.paste(im,(col*512,row*540+28));bd.text((col*512+10,row*540+8),pet+' / '+label,fill='white')
        render().save(d/'assembled.png');render(bent,True).resize((512,512),Image.Resampling.NEAREST).save(d/'pivot-preview.png')
        frames=[]
        for a,b in [({},bent),(bent,opposite),(opposite,{})]:
            for j in range(12):
                angles={n:a.get(n,0)*(1-j/12)+b.get(n,0)*j/12 for n in parts}
                frames.append(render(angles).resize((512,512),Image.Resampling.NEAREST).convert('RGB'))
        frames[0].save(d/'motion-preview.gif',save_all=True,append_images=frames[1:],duration=65,loop=0)
        fs=[]
        for e in EXPRS:
            ps=dict(parts);ps['head']=faces[e];fs.append(render(ps=ps).resize((512,512),Image.Resampling.NEAREST).convert('RGB'))
        fs[0].save(d/'expressions-preview.gif',save_all=True,append_images=fs[1:],duration=[700,150,500,500,500,500],loop=0)
        colors=set()
        for im in list(parts.values())+list(faces.values()):
            assert max(im.size)<=64
            arr=np.array(im);assert set(np.unique(arr[:,:,3]))<={0,255};colors.update(map(tuple,arr[arr[:,:,3]>0,:3]))
        assert len(colors)<=31
        assert len(entries)<=24
        for p in seq:
            w,h=parts[p['name']].size;assert 0<=p['pivot'][0]<w and 0<=p['pivot'][1]<h
            if p['parent']:
                w,h=parts[p['parent']].size;assert 0<=p['at'][0]<w and 0<=p['at'][1]<h
        reports[pet]={'parts':len(parts),'expressions':6,'colors':len(colors),'cell_size':64,'body_pixel_bytes':sum(im.width*im.height for im in parts.values())}
    board.save(BASE/'rig-overview.png');(BASE/'validation.json').write_text(json.dumps(reports,indent=2)+'\n');print(json.dumps(reports,indent=2))
if __name__=='__main__':build()
