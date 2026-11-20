import sys,capstone
from xref import *
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
va=int(sys.argv[1],16); n=int(sys.argv[2],0)
o=va2off(va)
for i in md.disasm(d[o:o+n],va):
    s=i.op_str
    import re
    m=re.search(r'rip \+ (0x[0-9a-f]+)',s)
    extra=''
    if m:
        t=i.address+i.size+int(m.group(1),16)
        to=va2off(t)
        if to: 
            raw=d[to:to+24]
            extra=' ; '+repr(raw.split(b'\0')[0][:30]) if raw[:1].isascii() and raw[:1]!=b'\0' else ' ; %x'%t
    print('%x: %s %s%s'%(i.address,i.mnemonic,s,extra))
