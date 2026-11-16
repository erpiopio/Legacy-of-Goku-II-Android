from pathlib import Path
import struct,sys
b=Path(sys.argv[1]).read_bytes();o=52;s={}
while o+8<len(b):
    n=struct.unpack_from('<I',b,o+4)[0];s[b[o:o+4]]=b[o+8:o+8+n];o+=8+n
m=s[b'BUS0'];iw=m[0x40000:0x48000];io=s[b'IO_0']
rom=Path('/home/plex-rober/legacy-goku2-thor/roms/legacy_of_goku_ii_europe_alfp.gba').read_bytes()
u=lambda b,o:struct.unpack_from('<I',b,o)[0]
level=u(iw,0xf80);print('camera',struct.unpack_from('<iiii',iw,0xf3c),'room',struct.unpack_from('<II',iw,0xf84),'level',hex(level))
for bg in range(4):
    r=u(rom,level-0x8000000+0x14+bg*4)
    print('BG',bg,'scroll',struct.unpack_from('<HH',io,0x10+4*bg),'resource',hex(r),'ctor',hex(u(rom,r-0x8000000)),'dims',list(rom[r-0x8000000+0x14:r-0x8000000+0x16]))
