#!/usr/bin/env python3
"""Deterministic extraction/registration of approved source sheets. No firmware edits.
Names l/r mean viewer-left/right. Output <=31px per axis; shared palette per pet.
"""
from pathlib import Path
import json, math, hashlib
import numpy as np
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'assets/purplemonkey/rig'
SOURCE_BASE=BASE
MAX_SIZE=31
OUT=BASE/'review'
OUT.mkdir(exist_ok=True)
NIGHT=(22,18,46,255)
# Manually inspected regions enclosing whole parts, excluding assembled reference.
REGIONS={
'monkey':{'head':(0,30,480,440),'body':(480,95,770,495),'arm_l':(785,95,940,565),'arm_r':(975,95,1130,565),'leg_l':(15,450,240,850),'leg_r':(260,450,470,850),'tail':(475,500,850,845),'drum':(865,560,1195,800)},
'cat':{'head':(0,45,510,465),'body':(530,120,825,550),'arm_l':(885,180,1010,575),'arm_r':(1070,180,1200,575),'leg_l':(35,490,250,845),'leg_r':(330,490,535,845),'tail':(550,540,780,845),'xylophone':(780,595,1230,855)},
'dog':{'ear_l':(25,75,275,370),'head':(320,35,625,385),'ear_r':(640,75,915,370),'body':(335,375,630,735),'arm_l':(65,380,325,740),'arm_r':(650,380,905,740),'leg_l':(145,735,370,990),'leg_r':(520,735,740,990),'tail':(760,715,990,990)},
'llama':{'head':(35,35,315,420),'body':(320,130,780,475),'neck':(785,45,990,515),'tail':(85,430,315,615),'collar':(420,480,715,630),'leg_far_l':(110,620,270,985),'leg_far_r':(300,620,465,985),'leg_near_l':(535,620,690,985),'leg_near_r':(735,620,905,985)}}
EXPRS=['neutral','blink','happy','sing','surprised','sleepy']
def solid(im):
 a=np.array(im.convert('RGBA')); a[:,:,3]=np.where(a[:,:,3]>=128,255,0);a[a[:,:,3]==0,:3]=0
 return Image.fromarray(a)
def trim(im):
 b=im.getchannel('A').getbbox()
 if b is None: raise ValueError('Empty part')
 return im.crop(b),b

def extract(pet):
 d=BASE/pet; d.mkdir(parents=True,exist_ok=True)
 source=SOURCE_BASE/pet/('parts-source-alpha-v2.png' if pet in ('dog','llama') else 'parts-source-v1.png')
 sheet=solid(Image.open(source)); raw={}; audit={}
 hires=d/'extracted-source';hires.mkdir(exist_ok=True)
 for name,box in REGIONS[pet].items():
  im,bb=trim(sheet.crop(box));raw[name]=im
  im.save(hires/(name+'.png'))
  audit[name]={'source_box':list(box),'trim_box':list(bb),'source_size':list(im.size)}
 # One common body-part scale, with a 1px pad all round.
 scale=(MAX_SIZE-2)/max(max(im.size) for im in raw.values())
 sprites={}
 for name,im in raw.items():
  sz=tuple(max(1,round(v*scale)) for v in im.size)
  small=solid(im.resize(sz,Image.Resampling.LANCZOS));pad=Image.new('RGBA',(sz[0]+2,sz[1]+2));pad.paste(small,(1,1));sprites[name]=pad
 # Face head shapes share a common scale and neck/chin anchor. Align translation only.
 exprsheet=solid(Image.open(SOURCE_BASE/pet/'expressions-source-v1.png'))
 cells=[]; boxes=[]
 for i,name in enumerate(EXPRS):
  box=(i%3*512,i//3*512,(i%3+1)*512,(i//3+1)*512)
  im,bb=trim(exprsheet.crop(box));cells.append(im);boxes.append(bb)
 # Register common bottom-centre neck/chin to fixed anchor. No per-frame scale changes.
 maxw=max(i.width for i in cells);maxh=max(i.height for i in cells)
 head=sprites['head'];available=(head.width-2,head.height-2)
 fs=min(available[0]/maxw,available[1]/maxh)
 frames={}; alignment=[]
 for name,im,bb in zip(EXPRS,cells,boxes):
  sz=(max(1,round(im.width*fs)),max(1,round(im.height*fs)))
  sm=solid(im.resize(sz,Image.Resampling.LANCZOS));cv=Image.new('RGBA',head.size)
  xy=((head.width-sz[0])//2,head.height-1-sz[1]);cv.paste(sm,xy);frames[name]=cv
  alignment.append({'name':name,'cell_bbox':list(bb),'scale':fs,'offset':list(xy),'canvas':list(head.size),'registration':'bottom-centre neck/chin; common scale, translation only'})
 sprites['head']=frames['neutral'].copy()
 # One shared 31-color palette including all facial frames, binary alpha.
 ims=list(sprites.values())+list(frames.values());pixels=[]
 for im in ims:
  a=np.array(im);pixels.extend(a[a[:,:,3]>0,:3].tolist())
 rgb=np.array(pixels,dtype=np.uint8).reshape(1,-1,3)
 palim=Image.fromarray(rgb).quantize(colors=31,method=Image.Quantize.MEDIANCUT)
 palette=palim.getpalette()[:93]
 def quant(im):
  rgb=im.convert('RGB').quantize(palette=palim,dither=Image.Dither.NONE).convert('RGBA');rgb.putalpha(im.getchannel('A'));return rgb
 sprites={n:quant(im) for n,im in sprites.items()};frames={n:quant(im) for n,im in frames.items()}
 for n,im in sprites.items():im.save(d/(n+'.png'))
 for n,im in frames.items():im.save(d/('head_'+n+'.png'))
 def point(name,x,y):
  w,h=sprites[name].size;return [max(0,min(w-1,round(x*(w-1)))),max(0,min(h-1,round(y*(h-1))))]
 parts=[]
 def add(n,parent=None,at=None,pv=(.5,.1)):
  parts.append({'name':n,'file':n+'.png','pivot':point(n,*pv),'parent':parent,'at':point(parent,*at) if parent else None})
 add('body',pv=(.5,.6))
 if pet!='llama':
  add('tail','body',(.83,.78),pv=({'monkey':(.18,.12),'cat':(.82,.85),'dog':(.15,.84)}[pet]))
  add('leg_l','body',(.30,.85),pv=(.5,.1));add('leg_r','body',(.70,.85),pv=(.5,.1))
  add('arm_l','body',(.18,.25),pv=(.65,.09) if pet=='dog' else (.5,.09))
  add('head','body',(.5,.12),pv=(.5,.88))
  if pet=='dog':
   add('ear_l','head',(.12,.33),pv=(.8,.16));add('ear_r','head',(.88,.33),pv=(.2,.16))
  add('arm_r','body',(.82,.25),pv=(.35,.09) if pet=='dog' else (.5,.09))
  instrument={'monkey':'drum','cat':'xylophone'}.get(pet)
  if instrument:add(instrument,'body',(.5,.68),pv=(.5,.25))
  order=['tail','leg_l','leg_r','arm_l','body']
  if pet=='dog':order+=['ear_l','head','ear_r','arm_r']
  else:order+=['head',instrument,'arm_r']
 else:
  add('tail','body',(.10,.28),pv=(.78,.75))
  for n,x in [('leg_far_l',.22),('leg_far_r',.77),('leg_near_l',.12),('leg_near_r',.88)]:add(n,'body',(x,.78),pv=(.5,.12))
  add('neck','body',(.80,.30),pv=(.5,.88));add('head','neck',(.5,.12),pv=(.4,.88));add('collar','neck',(.5,.80),pv=(.5,.25))
  order=['tail','leg_far_l','leg_far_r','body','leg_near_l','leg_near_r','neck','head','collar']
 # Conservative motion for pivot validation, not a finished performance choreography.
 def key(ms,a,root=(0,0)):return {'ms':ms,'root':list(root),'a':a}
 if pet!='llama':
  play=[key(200,{'arm_l':35,'arm_r':-10,'head':-3}),key(200,{'arm_l':10,'arm_r':-35,'head':3})]
  dance=[key(250,{'body':-5,'arm_l':45,'arm_r':-30,'head':4,'tail':12},(-1,-2)),key(250,{'body':5,'arm_l':30,'arm_r':-45,'head':-4,'tail':-12},(1,0))]
 else:
  play=[key(250,{'neck':-4,'head':5,'tail':8}),key(250,{'neck':4,'head':-5,'tail':-8})]
  dance=[key(250,{'neck':-7,'head':7,'leg_near_l':10,'leg_far_r':-8},(-1,-2)),key(250,{'neck':7,'head':-7,'leg_near_r':-10,'leg_far_l':8},(1,0))]
 rig={'note':'Extracted art; l/r denote viewer-left/right. Angles relative to neutral. Conservative QA motions. Expression metadata is separate; existing generator does not swap heads.','feet':0,'parts':parts,'order':order,'anims':{'idle':[key(1200,{'head':-2,'tail':-3}),key(1200,{'head':2,'tail':3},(0,1))],'play':play,'dance':dance}}
 # Ground offset from root pivot, computed from neutral hierarchy.
 poses=solve(rig,{},(0,0));maxy=max(poses[n][1]-next(p['pivot'][1] for p in parts if p['name']==n)+sprites[n].height for n in sprites)
 rig['feet']=math.ceil(maxy)
 (d/'rig.json').write_text(json.dumps(rig,indent=2)+'\n')
 hp=next(p['pivot'] for p in parts if p['name']=='head')
 expr={'part':'head','canvas':list(head.size),'pivot':hp,'palette':'palette.json','frames':{n:'head_'+n+'.png' for n in EXPRS},'registration':alignment,'integration':'Texture swap metadata only; existing firmware/gen_pm_rig.py do not consume this file.'}
 (d/'expressions.json').write_text(json.dumps(expr,indent=2)+'\n')
 (d/'palette.json').write_text(json.dumps([palette[i:i+3] for i in range(0,93,3)]))
 (d/'extraction.json').write_text(json.dumps({'source':source.name,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'common_scale':scale,'parts':audit},indent=2))
 return rig,sprites,frames

def solve(rig,angles,root):
 poses={}
 for p in rig['parts']:
  n=p['name'];pa=p['parent']
  if pa is None:poses[n]=(root[0],root[1],angles.get(n,0));continue
  parent=next(q for q in rig['parts'] if q['name']==pa);x,y,a=poses[pa];r=math.radians(a)
  dx=p['at'][0]-parent['pivot'][0];dy=p['at'][1]-parent['pivot'][1]
  poses[n]=(x+dx*math.cos(r)-dy*math.sin(r),y+dx*math.sin(r)+dy*math.cos(r),a+angles.get(n,0))
 return poses

def render(rig,sprites,angles={},root=(64,65),pivot=False,size=128):
 cv=Image.new('RGBA',(size,size),NIGHT);poses=solve(rig,angles,root)
 yy,xx=np.mgrid[0:size,0:size]
 for n in rig['order']:
  p=next(p for p in rig['parts'] if p['name']==n);x,y,a=poses[n];r=math.radians(a);c=math.cos(r);s=math.sin(r)
  u=np.floor((xx-x)*c+(yy-y)*s+p['pivot'][0]+.5).astype(int);v=np.floor(-(xx-x)*s+(yy-y)*c+p['pivot'][1]+.5).astype(int)
  arr=np.array(sprites[n]);valid=(u>=0)&(v>=0)&(u<arr.shape[1])&(v<arr.shape[0]);layer=np.zeros((size,size,4),dtype=np.uint8);layer[valid]=arr[v[valid],u[valid]];cv=Image.alpha_composite(cv,Image.fromarray(layer))
 if pivot:
  dr=ImageDraw.Draw(cv)
  for p in rig['parts']:
   x,y,_=poses[p['name']]
   if p['parent']:
    px,py,_=poses[p['parent']];dr.line((x,y,px,py),fill=(80,220,200),width=1)
   dr.ellipse((x-1,y-1,x+1,y+1),fill=(255,90,100))
 return cv

def main():
 results={p:extract(p) for p in REGIONS}
 board=Image.new('RGB',(4*384,4*410),(22,18,46));draw=ImageDraw.Draw(board)
 for row,(pet,(rig,sprites,frames)) in enumerate(results.items()):
  for col,(label,angle,piv) in enumerate([('neutral',{},False),('play',rig['anims']['play'][0]['a'],False),('dance',rig['anims']['dance'][0]['a'],False),('pivots',{},True)]):
   im=render(rig,sprites,angle,pivot=piv);board.paste(im.resize((384,384),Image.Resampling.NEAREST),(col*384,row*410+26));draw.text((col*384+15,row*410+8),pet+' / '+label,fill='white')
  motion=[]
  for state in ('idle','play','dance'):
   keys=rig['anims'][state]
   for j,key in enumerate(keys):
    nxt=keys[(j+1)%len(keys)]
    for step in range(8):
     t=step/8
     angles={n:key['a'].get(n,0)*(1-t)+nxt['a'].get(n,0)*t for n in sprites}
     root=tuple(base+key['root'][i]*(1-t)+nxt['root'][i]*t for i,base in enumerate((64,65)))
     motion.append(render(rig,sprites,angles,root).resize((384,384),Image.Resampling.NEAREST).convert('RGB'))
  motion[0].save(BASE/pet/'motion-preview.gif',save_all=True,append_images=motion[1:],duration=80,loop=0)
  render(rig,sprites).save(BASE/pet/'assembled.png')
  render(rig,sprites,pivot=True).resize((512,512),Image.Resampling.NEAREST).save(BASE/pet/'pivot-preview.png')
  seq=[]
  for expr in EXPRS:
   ss=dict(sprites);ss['head']=frames[expr];seq.append(render(rig,ss).resize((384,384),Image.Resampling.NEAREST).convert('RGB'))
  seq[0].save(BASE/pet/'expressions-preview.gif',save_all=True,append_images=seq[1:],duration=[750,150,600,600,600,600],loop=0)
  sheet=Image.new('RGBA',(3*128,2*148),NIGHT);dr=ImageDraw.Draw(sheet)
  for i,(name,im) in enumerate(frames.items()):
   dr.text((i%3*128+8,i//3*148+5),name,fill='white');big=im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST);sheet.alpha_composite(big,(i%3*128+(128-big.width)//2,i//3*148+22))
  sheet.save(BASE/pet/'faces-preview.png')
 board.save(OUT/'rig-overview.png')
 report={}
 for pet,(rig,sprites,frames) in results.items():
  allims=list(sprites.values())+list(frames.values());colors=set()
  for im in allims:
   assert max(im.size)<=31
   a=np.array(im);assert set(np.unique(a[:,:,3]))<={0,255};colors.update(map(tuple,a[a[:,:,3]>0,:3].tolist()))
  assert len(colors)<=31
  for p in rig['parts']:
   w,h=sprites[p['name']].size;assert 0<=p['pivot'][0]<w and 0<=p['pivot'][1]<h
   if p['parent']:
    w,h=sprites[p['parent']].size;assert 0<=p['at'][0]<w and 0<=p['at'][1]<h
  assert len(set(im.size for im in frames.values()))==1
  report[pet]={'parts':len(sprites),'expression_frames':len(frames),'max_dimension':max(max(im.size) for im in allims),'colors':len(colors),'pixel_bytes_parts':sum(im.width*im.height for im in sprites.values()),'pixel_bytes_extra_faces':sum(im.width*im.height for n,im in frames.items() if n!='neutral'),'feet':rig['feet']}
 (OUT/'validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':main()
