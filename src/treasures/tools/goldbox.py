import sys,struct,glob,os,re
def dax(path):
    b=open(path,'rb').read(); hs=struct.unpack_from('<H',b)[0]
    out={}
    for i in range(hs//9):
        id_,off,raw,comp=struct.unpack_from('<BIHH',b,2+i*9)
        src=b[2+hs+off:2+hs+off+comp]; o=bytearray(); p=0
        while p<len(src) and len(o)<raw:
            n=src[p]; p+=1
            if n<0x80: o+=src[p:p+n+1]; p+=n+1
            else: o+=bytes([src[p]])*(0x100-n); p+=1
        out[id_]=bytes(o[:raw])
    return out
def unpack6(data,n):
    bits=int.from_bytes(data,'big'); tot=len(data)*8; s=''
    for i in range(n):
        v=(bits>>(tot-6*(i+1)))&0x3f
        s+=chr(v+0x40 if v<=0x1f else v)
    return s
def strings(blk):
    res=[];p=0
    while p<len(blk)-2:
        if blk[p]==0x80:
            n=blk[p+1]; nb=(n*6+7)//8
            if 3<=n<=255 and p+2+nb<=len(blk):
                s=unpack6(blk[p+2:p+2+nb],n)
                if re.fullmatch(r"[A-Z0-9 ,.'!?\-:;\"()/&]+",s) and ' ' in s:
                    res.append(s); p+=2+nb; continue
        p+=1
    return res
