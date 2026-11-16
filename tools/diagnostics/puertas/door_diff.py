import struct
from pathlib import Path
def sections(path):
 b=Path(path).read_bytes();s={};o=52
 while o+8<len(b):
  n=struct.unpack_from('<I',b,o+4)[0];s[b[o:o+4]]=b[o+8:o+8+n];o+=8+n
 return s
a=sections('/tmp/door-before.state');b=sections('/tmp/legacy2-rain-work/door-near.state')
for bg in range(4):
 print('BG',bg)
 for y in range(27+bg*8,43+bg*8):
  dif=[]
  for x in range(51+bg*8,56+bg*8):
   at=0x48400+0xe000+bg*0x800+((y%32)*32+(x%32))*2
   va=struct.unpack_from('<H',a[b'BUS0'],at)[0];vb=struct.unpack_from('<H',b[b'BUS0'],at)[0]
   if va!=vb:dif.append((x,y,hex(va),hex(vb)))
  if dif:print(dif)
