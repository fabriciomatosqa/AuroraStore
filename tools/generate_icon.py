#!/usr/bin/env python3
"""Render Aurora crystal A emblem as an opaque 512x512 RGB PS4 icon."""
from PIL import Image, ImageDraw, ImageFilter
import os
S=512
base=Image.new("RGBA",(S,S),(0,0,0,255))
outer=[(256,42),(486,455),(26,455)]
halo=Image.new("RGBA",(S,S),(0,0,0,0)); h=ImageDraw.Draw(halo)
for w,c in [(44,(0,55,255,35)),(28,(0,130,255,55)),(14,(0,230,255,80)),(6,(130,70,255,110))]:
 h.line(outer+[outer[0]],fill=c,width=w,joint="curve")
base=Image.alpha_composite(base,halo.filter(ImageFilter.GaussianBlur(17)))
d=ImageDraw.Draw(base)
d.polygon([(256,54),(256,214),(45,444)],fill=(20,93,245,255))
d.polygon([(256,54),(467,444),(256,214)],fill=(105,70,225,255))
d.polygon([(45,444),(256,214),(256,390)],fill=(0,90,220,255))
d.polygon([(256,214),(467,444),(256,390)],fill=(0,185,190,255))
d.polygon([(256,212),(326,335),(382,389),(256,364),(130,389),(186,335)],fill=(0,38,150,255))
d.polygon([(256,300),(186,335),(130,389),(256,364)],fill=(0,115,225,255))
d.polygon([(256,300),(326,335),(382,389),(256,364)],fill=(0,205,205,255))
d.polygon([(256,212),(256,300),(186,335)],fill=(12,54,190,255))
d.polygon([(256,212),(326,335),(256,300)],fill=(8,95,205,255))
d.polygon([(256,322),(349,394),(256,377),(163,394)],fill=(0,0,0,255))
edge=Image.new("RGBA",(S,S),(0,0,0,0)); e=ImageDraw.Draw(edge)
e.line(outer+[outer[0]],fill=(95,245,255,255),width=2,joint="curve")
base=Image.alpha_composite(base,edge.filter(ImageFilter.GaussianBlur(2)))
d=ImageDraw.Draw(base)
d.line([(256,54),(256,214),(45,444)],fill=(115,150,255,210),width=2)
d.line([(256,54),(467,444)],fill=(120,245,255,220),width=2)
d.line([(45,444),(256,390),(467,444)],fill=(0,230,255,180),width=2)
os.makedirs("sce_sys",exist_ok=True)
icon=base.convert("RGB")
icon.save("sce_sys/icon0.png",format="PNG",optimize=True)
assert icon.size==(512,512) and icon.mode=="RGB"
