from pathlib import Path
import struct,sys
for f in sys.argv[1:]:
    b=Path(f).read_bytes();o=52;s={}
    while o+8<len(b):
        n=struct.unpack_from('<I',b,o+4)[0];s[b[o:o+4]]=b[o+8:o+8+n];o+=8+n
    m=s[b'BUS0'];print(f,'frame',struct.unpack_from('<Q',s[b'META'],16)[0])
    for i in range(128):
        a,x,t=struct.unpack_from('<HHH',m,0x60400+i*8)
        if a&0x300==0x200:continue
        if i<20 or a&255<32:
            print(i,*(hex(v) for v in (a,x,t)),'XY',x&511,a&255,'tile',hex(t&1023))
