# Altered version based on zlib contrib/blast by Mark Adler (zlib license).
# Original: Copyright (C) 2003, 2012, 2013 Mark Adler
# This software is provided 'as-is', without any express or implied warranty.
# In no event will the authors be held liable for any damages arising from the use of this software.
# Permission is granted to anyone to use this software for any purpose, including commercial
# applications, and to alter it and redistribute it freely, subject to the following restrictions:
# 1. The origin of this software must not be misrepresented; you must not claim that you wrote
#    the original software.
# 2. Altered source versions must be plainly marked as such, and must not be misrepresented as
#    being the original software.
# 3. This notice may not be removed or altered from any source distribution.
# PKWARE DCL explode (port of zlib contrib/blast.c)
MAXBITS=13
class _H:
    def __init__(s, rep):
        length=[]
        for b in rep:
            ln=(b>>4)+1; l=b&15
            length += [l]*ln
        s.count=[0]*(MAXBITS+1); s.symbol=[0]*len(length)
        for l in length: s.count[l]+=1
        offs=[0]*(MAXBITS+1)
        for l in range(1,MAXBITS): offs[l+1]=offs[l]+s.count[l]
        for sym,l in enumerate(length):
            if l: s.symbol[offs[l]]=sym; offs[l]+=1
LITLEN=bytes([11,124,8,7,28,7,188,13,76,4,10,8,12,10,12,10,8,23,8,9,7,6,7,8,7,6,55,8,23,24,12,11,7,9,11,12,6,7,22,5,7,24,6,11,9,6,7,22,7,11,38,7,9,8,25,11,8,11,9,12,8,12,5,38,5,38,5,11,7,5,6,21,6,10,53,8,7,24,10,27,44,253,253,253,252,252,252,13,12,45,12,45,12,61,12,45,44,173])
LENLEN=bytes([2,35,36,53,38,23])
DISTLEN=bytes([2,20,53,230,247,151,248])
BASE=[3,2,4,5,6,7,8,9,10,12,16,24,40,72,136,264]
EXTRA=[0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8]
_lit=_H(LITLEN); _len=_H(LENLEN); _dist=_H(DISTLEN)
def explode(src):
    pos=0; bitbuf=0; bitcnt=0
    out=bytearray()
    def bits(need):
        nonlocal pos,bitbuf,bitcnt
        val=bitbuf
        while bitcnt<need:
            val |= src[pos]<<bitcnt; pos+=1; bitcnt+=8
        bitbuf=val>>need; bitcnt-=need
        return val & ((1<<need)-1)
    def decode(h):
        nonlocal pos,bitbuf,bitcnt
        code=first=index=0
        bb=bitbuf; left=bitcnt; ln=1
        while True:
            while left:
                code |= (bb&1)^1; bb>>=1
                count=h.count[ln]
                if code < first+count:
                    bitbuf=bb; bitcnt=(bitcnt-ln)&7
                    return h.symbol[index+(code-first)]
                index+=count; first+=count; first<<=1; code<<=1
                left-=1; ln+=1
            left=(MAXBITS+1)-ln
            if left==0: raise ValueError('ran out of codes')
            bb=src[pos]; pos+=1
            if left>8: left=8
    lit=bits(8); dict_=bits(8)
    if lit>1 or not 4<=dict_<=6: raise ValueError('bad header %d %d'%(lit,dict_))
    while True:
        if bits(1):
            sym=decode(_len); ln=BASE[sym]+bits(EXTRA[sym])
            if ln==519: break
            sym=2 if ln==2 else dict_
            dist=(decode(_dist)<<sym)+bits(sym)+1
            if dist>len(out): raise ValueError('dist too far')
            for _ in range(ln): out.append(out[-dist])
        else:
            out.append(decode(_lit) if lit else bits(8))
    return bytes(out)
