#!/usr/bin/env python3
"""Add registered, independently swappable facial layers to articulated pet rigs."""
import json, math
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageOps
import extract_pet_rigs as art
ROOT=art.ROOT
SRC=ROOT/'assets/purplemonkey/rig-64-articulated'
BASE=ROOT/'assets/purplemonkey/rig-64-modular'
PETS=['monkey','cat','dog','llama']
HEAD_SIZES={'monkey':(48,46),'cat':(50,44),'dog':(48,52),'llama':(34,43)}
NIGHT=art.NIGHT

def trim(im):
    im=art.solid(im);bb=im.getchannel('A').getbbox();assert bb
    return im.crop(bb)

def build():
    source=Image.open(BASE/'source/face-kit.png').convert('RGBA')
    board=Image.new('RGB',(1536,2160),NIGHT[:3]);bd=ImageDraw.Draw(board)
    report={}
    for idx,pet in enumerate(PETS):
        d=BASE/pet;d.mkdir(parents=True,exist_ok=True)
        rig=json.loads((SRC/pet/'rig.json').read_text())
        oldhead=next(p for p in rig['parts'] if p['name']=='head')
        rig['parts']=[p for p in rig['parts'] if p['name'] not in ('head','ear_l','ear_r')]
        rig['order']=[n for n in rig['order'] if n not in ('head','ear_l','ear_r')]
        parts={p['name']:Image.open(SRC/pet/p['file']).convert('RGBA') for p in rig['parts']}
        cells=[]
        for i in range(16):
            row=idx*2+i//8;col=i%8
            # Generated art uses uneven spacing: inspected gutters, not assumed grid cells.
            xs=[0,188,328,478,632,782,934,1088,1254]
            ys=([0,180,345,490,650,790,946,1070,1254] if col==7 else [0,194,325,508,624,816,912,1100,1254])
            cells.append(trim(source.crop((xs[col],ys[row],xs[col+1],ys[row+1]))))
        target=HEAD_SIZES[pet];s=min(target[0]/cells[0].width,target[1]/cells[0].height)
        head=art.solid(cells[0].resize((round(cells[0].width*s),round(cells[0].height*s)),Image.Resampling.LANCZOS))
        w,h=head.size;hp=[round((w-1)*.5),round((h-1)*.88)]
        parts['head_shape']=head
        rig['parts'].append({'name':'head_shape','file':'head_shape.png','parent':oldhead['parent'],'at':oldhead['at'],'pivot':hp})
        # Head is painted before face features; attachments use head-local coordinates.
        rig['order']+=['head_shape']
        variants={};meta={'head_shape':{'file':'head_shape.png','canvas':[w,h],'pivot':hp},'layers':{},'expressions':{}}
        def facial_layer(name,indices,names,width_ratio,height_ratio,cx,cy,default):
            raw=[cells[i] for i in indices]
            scale=min(w*width_ratio/max(im.width for im in raw),h*height_ratio/max(im.height for im in raw))
            frames={};reg={}
            for state,im in zip(names,raw):
                sz=(max(1,round(im.width*scale)),max(1,round(im.height*scale)))
                sm=art.solid(im.resize(sz,Image.Resampling.LANCZOS));canvas=Image.new('RGBA',(w,h))
                xy=(round(w*cx-sz[0]/2),round(h*cy-sz[1]/2));assert xy[0]>=0 and xy[1]>=0 and xy[0]+sz[0]<=w and xy[1]+sz[1]<=h
                canvas.paste(sm,xy);key=name+'_'+state;variants[key]=canvas;frames[state]=key+'.png';reg[state]={'offset':list(xy),'scale':scale}
            parts[name]=variants[name+'_'+default]
            rig['parts'].append({'name':name,'file':frames[default],'parent':'head_shape','at':hp,'pivot':hp})
            rig['order'].append(name)
            meta['layers'][name]={'canvas':[w,h],'pivot':hp,'frames':frames,'default':default,'registration':reg}
        facial_layer('eyes',[4,5,6,7],['open','blink','happy','surprised'],.70,.38,.5,.47,'open')
        facial_layer('nose',[12,13,14],['neutral','scrunch','tilt'],.24,.16,.5 if pet!='llama' else .60,.65,'neutral')
        facial_layer('mouth',[8,9,10,11],['smile','sing','happy','sleepy'],.44,.23,.5 if pet!='llama' else .56,.81,'smile')
        # Ear frames share a 64px canvas and fixed root pivot; opposite ear is mirrored.
        eraw=[cells[i] for i in (1,2,3,15)]
        dims={'monkey':(22,24),'cat':(23,28),'dog':(32,42),'llama':(18,31)}[pet]
        es=min(dims[0]/max(im.width for im in eraw),dims[1]/max(im.height for im in eraw))
        for side in ('l','r'):
            name='ear_'+side;frames={};reg={}
            for state,im in zip(('relaxed','perk','droop','flick'),eraw):
                sz=(max(1,round(im.width*es)),max(1,round(im.height*es)))
                sm=art.solid(im.resize(sz,Image.Resampling.LANCZOS))
                if side=='r':sm=ImageOps.mirror(sm)
                if pet=='monkey':anchor=(.78 if side=='l' else .22,.55)
                elif pet=='dog':anchor=(.65 if side=='l' else .35,.15)
                else:anchor=(.5,.88)
                local=[round((sz[0]-1)*anchor[0]),round((sz[1]-1)*anchor[1])]
                # Lower pivot gives upright ears room; dog's hanging ears need room below.
                ep=[32,16 if pet=='dog' else 36]
                xy=[ep[0]-local[0],ep[1]-local[1]]
                canvas=Image.new('RGBA',(64,64));assert min(xy)>=0 and xy[0]+sz[0]<=64 and xy[1]+sz[1]<=64
                canvas.paste(sm,xy);key=name+'_'+state;variants[key]=canvas;frames[state]=key+'.png';reg[state]={'offset':xy,'scale':es,'root_in_scaled_image':local}
            edge={'monkey':.12,'cat':.22,'dog':.06,'llama':.20}[pet]
            ax=edge if side=='l' else 1-edge
            ay={'monkey':.52,'cat':.29,'dog':.25,'llama':.18}[pet]
            attach=[round((w-1)*ax),round((h-1)*ay)]
            parts[name]=variants[name+'_relaxed']
            rig['parts'].append({'name':name,'file':frames['relaxed'],'parent':'head_shape','at':attach,'pivot':ep})
            rig['order'].insert(rig['order'].index('head_shape'),name)
            meta['layers'][name]={'canvas':[64,64],'pivot':ep,'frames':frames,'default':'relaxed','registration':reg}
        meta['expressions']={
          'neutral':{'eyes':'open','mouth':'smile','nose':'neutral','ear_l':'relaxed','ear_r':'relaxed'},
          'blink':{'eyes':'blink','mouth':'smile','nose':'neutral','ear_l':'relaxed','ear_r':'relaxed'},
          'happy':{'eyes':'happy','mouth':'happy','nose':'scrunch','ear_l':'perk','ear_r':'perk'},
          'sing':{'eyes':'open','mouth':'sing','nose':'neutral','ear_l':'flick','ear_r':'flick'},
          'surprised':{'eyes':'surprised','mouth':'sing','nose':'tilt','ear_l':'perk','ear_r':'perk'},
          'sleepy':{'eyes':'blink','mouth':'sleepy','nose':'neutral','ear_l':'droop','ear_r':'droop'}}
        # Quantize body and every facial variant together.
        allims=list(parts.values())+list(variants.values())
        pix=np.concatenate([np.array(im)[np.array(im)[:,:,3]>0,:3] for im in allims])
        palette=Image.fromarray(pix.reshape(1,-1,3)).quantize(31,method=Image.Quantize.MEDIANCUT)
        def quant(im):
            out=im.convert('RGB').quantize(palette=palette,dither=Image.Dither.NONE).convert('RGBA');out.putalpha(im.getchannel('A'));return out
        parts={n:quant(im) for n,im in parts.items()};variants={n:quant(im) for n,im in variants.items()}
        for p in rig['parts']:parts[p['name']].save(d/p['file'])
        for n,im in variants.items():im.save(d/(n+'.png'))
        for keys in rig['anims'].values():
            for key in keys:
                if 'head' in key['a']:key['a']['head_shape']=key['a'].pop('head')
        rig['note']='64px articulated limbs and modular facial layers. Requires >=22 part slots and per-part texture swapping. No flattened expression heads.'
        (d/'rig.json').write_text(json.dumps(rig,indent=2)+'\n');(d/'face-variants.json').write_text(json.dumps(meta,indent=2)+'\n')
        (d/'palette.json').write_text(json.dumps([palette.getpalette()[i:i+3] for i in range(0,93,3)])+'\n')
        entries=[(p['name'],parts[p['name']],p['pivot'],p['file']) for p in rig['parts'] if p['name'] not in meta['layers']]
        for name,layer in meta['layers'].items():
            entries +=[(Path(file).stem,variants[Path(file).stem],layer['pivot'],file) for file in layer['frames'].values()]
        rows=math.ceil(len(entries)/6);atlas=Image.new('RGBA',(384,64*rows));preview=Image.new('RGBA',(960,184*rows),NIGHT);pd=ImageDraw.Draw(preview)
        manifest={'cell_size':[64,64],'sheet':'spritesheet-64.png','frames':{}}
        for i,(name,im,pv,file) in enumerate(entries):
            x,y=i%6*64,i//6*64;ox,oy=(64-im.width)//2,(64-im.height)//2
            atlas.paste(im,(x+ox,y+oy));tile=Image.new('RGBA',(64,64));tile.paste(im,(ox,oy))
            preview.alpha_composite(tile.resize((160,160),Image.Resampling.NEAREST),(i%6*160,i//6*184+24));pd.text((i%6*160+3,i//6*184+5),name,fill='white')
            manifest['frames'][name]={'cell':[x,y,64,64],'content':[x+ox,y+oy,im.width,im.height],'file':file,'pivot_in_cell':[pv[0]+ox,pv[1]+oy]}
        atlas.save(d/'spritesheet-64.png');preview.save(d/'spritesheet-preview.png');(d/'spritesheet-64.json').write_text(json.dumps(manifest,indent=2)+'\n')
        def render(angles={},ps=None,pivot=False):return art.render(rig,ps or parts,angles,root=(128,136),size=256,pivot=pivot)
        bent=rig['anims']['play'][0]['a']
        for col,(label,a,pv) in enumerate([('neutral',{},False),('articulated',bent,False),('pivots',bent,True)]):
            board.paste(render(a,pivot=pv).resize((512,512),Image.Resampling.NEAREST),(col*512,idx*540+28));bd.text((col*512+10,idx*540+8),pet+' / '+label,fill='white')
        render().save(d/'assembled.png')
        faceboard=Image.new('RGBA',(768,560),NIGHT);fd=ImageDraw.Draw(faceboard);gif=[]
        for j,(exp,states) in enumerate(meta['expressions'].items()):
            ps=dict(parts)
            for layer,state in states.items():ps[layer]=variants[layer+'_'+state]
            im=render(ps=ps);faceboard.paste(im,(j%3*256,j//3*280+24));fd.text((j%3*256+8,j//3*280+5),exp,fill='white');gif.append(im.resize((512,512),Image.Resampling.NEAREST).convert('RGB'))
        faceboard.save(d/'faces-preview.png');gif[0].save(d/'expressions-preview.gif',save_all=True,append_images=gif[1:],duration=[700,150,500,500,500,500],loop=0)
        gif=[]
        for a,b in [({},bent),(bent,rig['anims']['play'][1]['a']),(rig['anims']['play'][1]['a'],{})]:
            for j in range(12):gif.append(render({n:a.get(n,0)*(1-j/12)+b.get(n,0)*j/12 for n in parts}).resize((512,512),Image.Resampling.NEAREST).convert('RGB'))
        gif[0].save(d/'motion-preview.gif',save_all=True,append_images=gif[1:],duration=65,loop=0)
        colors=set()
        for im in list(parts.values())+list(variants.values()):
            assert max(im.size)<=64;a=np.array(im);assert set(np.unique(a[:,:,3]))<={0,255};colors.update(map(tuple,a[a[:,:,3]>0,:3]))
        assert len(colors)<=31
        for p in rig['parts']:
            w0,h0=parts[p['name']].size;assert 0<=p['pivot'][0]<w0 and 0<=p['pivot'][1]<h0
            if p['parent']:
                w0,h0=parts[p['parent']].size;assert 0<=p['at'][0]<w0 and 0<=p['at'][1]<h0
        for f in manifest['frames'].values():
            x,y,w0,h0=f['content'];assert atlas.crop((x,y,x+w0,y+h0)).tobytes()==Image.open(d/f['file']).tobytes()
        report[pet]={'rig_parts':len(parts),'atlas_sprites':len(entries),'facial_layers':5,'ear_poses_per_side':4,'eye_variants':4,'mouth_variants':4,'nose_variants':3,'opaque_colors':len(colors),'body_and_default_face_pixel_bytes':sum(im.width*im.height for im in parts.values())}
    board.save(BASE/'rig-overview.png');(BASE/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':build()
