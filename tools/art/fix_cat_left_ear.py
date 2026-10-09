#!/usr/bin/env python3
from pathlib import Path
import json,shutil
import numpy as np
from PIL import Image,ImageDraw
import extract_pet_rigs as art
ROOT=art.ROOT;OLD=ROOT/'assets/purplemonkey/rig-64-cat-v3/cat';BASE=ROOT/'assets/purplemonkey/rig-64-cat-v4';D=BASE/'cat'
def main():
 shutil.copytree(OLD,D,dirs_exist_ok=True)
 rig=json.loads((D/'rig.json').read_text());meta=json.loads((D/'face-variants.json').read_text());atlasmeta=json.loads((D/'spritesheet-64.json').read_text())
 source=Image.open(BASE/'source/left-ear-source.png').convert('RGBA');raw=[]
 for i in range(4):
  box=(i%2*source.width//2,i//2*source.height//2,(i%2+1)*source.width//2,(i//2+1)*source.height//2)
  im=art.solid(source.crop(box));raw.append(im.crop(im.getchannel('A').getbbox()))
 colors=set()
 for f in atlasmeta['frames'].values():
  a=np.array(Image.open(OLD/f['file']).convert('RGBA'));colors.update(map(tuple,a[a[:,:,3]>0,:3].tolist()))
 colors=sorted(colors);pal=Image.new('P',(1,1));flat=[v for c in colors for v in c];pal.putpalette(flat+[0]*(768-len(flat)))
 scale=min(24/max(im.width for im in raw),28/max(im.height for im in raw))
 for state,im in zip(('relaxed','perk','droop','flick'),raw):
  sz=(round(im.width*scale),round(im.height*scale));sm=art.solid(im.resize(sz,Image.Resampling.LANCZOS));cv=Image.new('RGBA',(64,64));xy=[32-round((sz[0]-1)*.5),36-round((sz[1]-1)*.85)];cv.paste(sm,xy)
  q=cv.convert('RGB').quantize(palette=pal,dither=Image.Dither.NONE).convert('RGBA');q.putalpha(cv.getchannel('A'));q.save(D/('ear_l_'+state+'.png'))
  meta['layers']['ear_l']['registration'][state]={'offset':xy,'scale':scale,'source_quadrant':state,'root_in_scaled_image':[32-xy[0],36-xy[1]]}
 p=next(p for p in rig['parts'] if p['name']=='ear_l');p['at'][1]+=2
 rig['note']='v4: viewer-left ear redrawn with rounded black root and simple pink inner triangle. Attachment lowered 2px; all other sprites unchanged from v3.'
 (D/'rig.json').write_text(json.dumps(rig,indent=2)+'\n');(D/'face-variants.json').write_text(json.dumps(meta,indent=2)+'\n')
 atlas=Image.new('RGBA',(384,384));preview=Image.new('RGBA',(960,1104),art.NIGHT);dr=ImageDraw.Draw(preview)
 for i,(n,f) in enumerate(atlasmeta['frames'].items()):
  im=Image.open(D/f['file']).convert('RGBA');x,y,w,h=f['content'];atlas.paste(im,(x,y));tile=Image.new('RGBA',(64,64));cx,cy,_,_=f['cell'];tile.paste(im,(x-cx,y-cy));preview.alpha_composite(tile.resize((160,160),Image.Resampling.NEAREST),(i%6*160,i//6*184+24));dr.text((i%6*160+3,i//6*184+5),n,fill='white')
  if not n.startswith('ear_l_'):assert im.tobytes()==Image.open(OLD/f['file']).convert('RGBA').tobytes()
  a=np.array(im);assert max(im.size)<=64 and set(np.unique(a[:,:,3]))<={0,255}
 atlas.save(D/'spritesheet-64.png');preview.save(D/'spritesheet-preview.png')
 parts={p['name']:Image.open(D/p['file']).convert('RGBA') for p in rig['parts']}
 def render(ps):return art.render(rig,ps,root=(128,136),size=256)
 render(parts).save(D/'assembled.png');frames=[];board=Image.new('RGBA',(768,560),art.NIGHT);bd=ImageDraw.Draw(board);heads=board.copy();hd=ImageDraw.Draw(heads)
 for i,(name,states) in enumerate(meta['expressions'].items()):
  ps=dict(parts)
  for layer,state in states.items():ps[layer]=Image.open(D/meta['layers'][layer]['frames'][state]).convert('RGBA')
  im=render(ps);board.paste(im,(i%3*256,i//3*280+24));bd.text((i%3*256+8,i//3*280+5),name,fill='white');big=im.resize((512,512),Image.Resampling.NEAREST);frames.append(big.convert('RGB'));heads.paste(big.crop((172,110,340,270)).resize((256,244),Image.Resampling.NEAREST),(i%3*256,i//3*280+30));hd.text((i%3*256+8,i//3*280+8),name,fill='white')
 board.save(D/'faces-preview.png');heads.save(D/'head-review.png');frames[0].save(D/'expressions-preview.gif',save_all=True,append_images=frames[1:],duration=[700,150,500,500,500,500],loop=0)
 (BASE/'validation.json').write_text(json.dumps({'updated_sprites':4,'other_sprite_pixels_unchanged':True,'max_cell':64,'palette_colors':len(colors),'attachment_change':'ear_l.at.y + 2'},indent=2)+'\n')
 print('Validated: four left-ear sprites updated; all other sprite pixels preserved.')
if __name__=='__main__':main()
