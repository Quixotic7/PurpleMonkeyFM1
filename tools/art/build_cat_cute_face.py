#!/usr/bin/env python3
"""Register the cute face-only asymmetric cat revision to the existing modular rig."""
import json,math
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
import extract_pet_rigs as art
ROOT=art.ROOT;BASE=ROOT/'assets/purplemonkey/rig-64-cat-v3';OLD=ROOT/'assets/purplemonkey/rig-64-modular/cat';D=BASE/'cat'

def build():
 D.mkdir(parents=True,exist_ok=True)
 src=Image.open(BASE/'source/cat-face-source.png').convert('RGBA');cells=[]
 bounds=json.loads((BASE/'source/crop-grid.json').read_text()) if (BASE/'source/crop-grid.json').exists() else {'x':[round(i*src.width/6) for i in range(7)],'y':[round(i*src.height/4) for i in range(5)]}
 for i in range(24):
  x,y=i%6,i//6;box=bounds['boxes'][i] if 'boxes' in bounds else (bounds['x'][x],bounds['y'][y],bounds['x'][x+1],bounds['y'][y+1]);im=art.solid(src.crop(box))
  if i in (13,14,15,16,17,18,19):
   a=np.array(im);rgb=a[:,:,:3].astype(int);white=(rgb.min(axis=2)>155)&((rgb.max(axis=2)-rgb.min(axis=2))<55);a[white,3]=0;im=Image.fromarray(a)
  bb=im.getchannel('A').getbbox();assert bb;cells.append(im.crop(bb))
 rig=json.loads((OLD/'rig.json').read_text());meta=json.loads((OLD/'face-variants.json').read_text())
 parts={p['name']:Image.open(OLD/p['file']).convert('RGBA') for p in rig['parts']}
 def fit(im,size):
  s=min(size[0]/im.width,size[1]/im.height);return art.solid(im.resize((max(1,round(im.width*s)),max(1,round(im.height*s))),Image.Resampling.LANCZOS))
 head=fit(cells[0],(54,46));w,h=head.size;hp=[w//2,round((h-1)*.88)];parts['head_shape']=head
 p=next(p for p in rig['parts'] if p['name']=='head_shape');p['pivot']=hp
 meta['head_shape']={'file':'head_shape.png','canvas':[w,h],'pivot':hp}
 # Body, tail, limbs and instrument retain the prior approved cartoon pixels.
 variants={}
 groups=[('eyes',[5,10,11,12],['open','blink','happy','surprised'],.73,.32,.5,.45),('nose',[13,14,15],['neutral','scrunch','tilt'],.24,.16,.51,.65),('mouth',[16,17,18,19],['smile','sing','happy','sleepy'],.46,.23,.51,.80)]
 for name,indices,states,wr,hr,cx,cy in groups:
  raw=[cells[i] for i in indices];s=min(w*wr/max(im.width for im in raw),h*hr/max(im.height for im in raw));layer=meta['layers'][name];layer.update(canvas=[w,h],pivot=hp,registration={})
  for state,im in zip(states,raw):
   small=art.solid(im.resize((max(1,round(im.width*s)),max(1,round(im.height*s))),Image.Resampling.LANCZOS));xy=[round(w*cx-small.width/2),round(h*cy-small.height/2)];cv=Image.new('RGBA',(w,h));cv.paste(small,xy);variants[name+'_'+state]=cv;layer['registration'][state]={'source_cell':indices[states.index(state)],'offset':xy,'scale':s}
  p=next(p for p in rig['parts'] if p['name']==name);p.update(pivot=hp,at=hp);parts[name]=variants[name+'_'+layer['default']]
 # Left and right ears are separately drawn, never mirrored.
 rawears=[cells[i] for i in (1,2,3,4,6,7,8,9)];es=min(26/max(im.width for im in rawears),30/max(im.height for im in rawears))
 for side,indices in [('l',[1,3,6,8]),('r',[2,4,7,9])]:
  name='ear_'+side;pv=[32,36];layer=meta['layers'][name];layer['pivot']=pv;layer['registration']={}
  for state,i in zip(('relaxed','perk','droop','flick'),indices):
   im=cells[i];sm=art.solid(im.resize((max(1,round(im.width*es)),max(1,round(im.height*es))),Image.Resampling.LANCZOS));xy=[32-round((sm.width-1)*.5),36-round((sm.height-1)*.86)];cv=Image.new('RGBA',(64,64));cv.paste(sm,xy);variants[name+'_'+state]=cv;layer['registration'][state]={'source_cell':i,'offset':xy,'scale':es}
  attach=[round(w*(.22 if side=='l' else .78)),round(h*(.23 if side=='l' else .19))]
  p=next(p for p in rig['parts'] if p['name']==name);p.update(pivot=pv,at=attach);parts[name]=variants[name+'_relaxed']
 # Keep body/limb/prop pixels byte-identical. Quantize ONLY new face art to the
 # original shared palette, which already includes warm white, black, pink and amber.
 preserved=[p['name'] for p in rig['parts'] if p['name'] not in ('head_shape','eyes','nose','mouth','ear_l','ear_r')]
 rgb=np.concatenate([np.array(parts[n])[np.array(parts[n])[:,:,3]>0,:3] for n in preserved])
 colors=sorted(set(map(tuple,rgb.tolist())))
 # Fill remaining slots with face colors selected from the generated revision.
 facepix=np.concatenate([np.array(im)[np.array(im)[:,:,3]>0,:3] for im in [head]+list(variants.values())])
 extra=Image.fromarray(facepix.reshape(1,-1,3)).quantize(max(1,31-len(colors)),method=Image.Quantize.MEDIANCUT).getpalette()
 for j in range(0,len(extra),3):
  c=tuple(extra[j:j+3])
  if len(c)==3 and c not in colors and len(colors)<31:colors.append(c)
 pal=Image.new('P',(1,1));flat=[v for c in colors for v in c];pal.putpalette(flat+[0]*(768-len(flat)))
 def quant(im):
  q=im.convert('RGB').quantize(palette=pal,dither=Image.Dither.NONE).convert('RGBA');q.putalpha(im.getchannel('A'));return q
 parts={n:im if n in preserved else quant(im) for n,im in parts.items()};variants={n:quant(im) for n,im in variants.items()}
 for p in rig['parts']:parts[p['name']].save(D/p['file'])
 for n,im in variants.items():im.save(D/(n+'.png'))
 rig['note']='Cute storybook face-only cat v3. Body, tail, limbs and instrument from original modular cat. Independent asymmetric facial layers.'
 (D/'rig.json').write_text(json.dumps(rig,indent=2)+'\n');(D/'face-variants.json').write_text(json.dumps(meta,indent=2)+'\n')
 entries=[(p['name'],parts[p['name']],p['pivot'],p['file']) for p in rig['parts'] if p['name'] not in meta['layers']]
 for name,layer in meta['layers'].items():
  entries.extend((Path(file).stem,variants[Path(file).stem],layer['pivot'],file) for file in layer['frames'].values())
 atlas=Image.new('RGBA',(384,384));preview=Image.new('RGBA',(960,1104),art.NIGHT);draw=ImageDraw.Draw(preview);manifest={'cell_size':[64,64],'sheet':'spritesheet-64.png','frames':{}}
 for i,(name,im,pv,file) in enumerate(entries):
  x,y=i%6*64,i//6*64;ox,oy=(64-im.width)//2,(64-im.height)//2;atlas.paste(im,(x+ox,y+oy));tile=Image.new('RGBA',(64,64));tile.paste(im,(ox,oy));preview.alpha_composite(tile.resize((160,160),Image.Resampling.NEAREST),(i%6*160,i//6*184+24));draw.text((i%6*160+3,i//6*184+5),name,fill='white');manifest['frames'][name]={'cell':[x,y,64,64],'content':[x+ox,y+oy,im.width,im.height],'file':file,'pivot_in_cell':[pv[0]+ox,pv[1]+oy]}
 atlas.save(D/'spritesheet-64.png');preview.save(D/'spritesheet-preview.png');(D/'spritesheet-64.json').write_text(json.dumps(manifest,indent=2)+'\n')
 def render(ps=parts,a={}):return art.render(rig,ps,a,root=(128,136),size=256)
 render().save(D/'assembled.png');frames=[];faceboard=Image.new('RGBA',(768,560),art.NIGHT);dr=ImageDraw.Draw(faceboard)
 for i,(name,states) in enumerate(meta['expressions'].items()):
  ps=dict(parts)
  for layer,state in states.items():ps[layer]=variants[layer+'_'+state]
  im=render(ps);frames.append(im.resize((512,512),Image.Resampling.NEAREST).convert('RGB'));faceboard.paste(im,(i%3*256,i//3*280+24));dr.text((i%3*256+8,i//3*280+5),name,fill='white')
 faceboard.save(D/'faces-preview.png');frames[0].save(D/'expressions-preview.gif',save_all=True,append_images=frames[1:],duration=[700,150,500,500,500,500],loop=0)
 # Enlarged assembled head previews for reviewing likeness.
 headboard=Image.new('RGBA',(768,560),art.NIGHT);hd=ImageDraw.Draw(headboard)
 for i,frame in enumerate(frames):
  headboard.paste(frame.crop((172,110,340,270)).resize((256,244),Image.Resampling.NEAREST),(i%3*256,i//3*280+30));hd.text((i%3*256+8,i//3*280+8),list(meta['expressions'])[i],fill='white')
 headboard.save(D/'head-review.png')
 colors=set()
 for im in list(parts.values())+list(variants.values()):
  assert max(im.size)<=64;a=np.array(im);assert set(np.unique(a[:,:,3]))<={0,255};colors.update(map(tuple,a[a[:,:,3]>0,:3]))
 assert len(colors)<=31
 for p in rig['parts']:
  w0,h0=parts[p['name']].size;assert 0<=p['pivot'][0]<w0 and 0<=p['pivot'][1]<h0
  if p['parent']:
   w0,h0=parts[p['parent']].size;assert 0<=p['at'][0]<w0 and 0<=p['at'][1]<h0
 for f in manifest['frames'].values():
  x,y,w0,h0=f['content'];assert atlas.crop((x,y,x+w0,y+h0)).tobytes()==Image.open(D/f['file']).tobytes()
 (BASE/'validation.json').write_text(json.dumps({'parts':len(parts),'sprites':len(entries),'max_cell':64,'colors':len(colors),'independently_drawn_ears':True},indent=2)+'\n')
 for n in preserved:
  p=next(p for p in rig['parts'] if p['name']==n);assert parts[n].tobytes()==Image.open(OLD/p['file']).convert('RGBA').tobytes()
 print('Validated face-only cute cat:',len(parts),'parts,',len(entries),'sprites')
if __name__=='__main__':build()
