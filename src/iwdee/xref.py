import pefile,struct,sys,capstone
pe=pefile.PE('../icewind.exe',fast_load=True)
base=pe.OPTIONAL_HEADER.ImageBase
d=open('../icewind.exe','rb').read()
def off2va(o):
    for s in pe.sections:
        if s.PointerToRawData<=o<s.PointerToRawData+s.SizeOfRawData: return base+s.VirtualAddress+o-s.PointerToRawData
def va2off(v):
    for s in pe.sections:
        if s.VirtualAddress<=v-base<s.VirtualAddress+s.Misc_VirtualSize: return v-base-s.VirtualAddress+s.PointerToRawData
text=[s for s in pe.sections if s.Name.startswith(b'.text')][0]
ts=text.PointerToRawData; te=ts+text.SizeOfRawData
def xrefs(tva):
    out=[]
    import re
    for o in range(ts,te-4):
        rel=struct.unpack_from('<i',d,o)[0]
        if off2va_text(o)+4+rel==tva: out.append(o)
    return out
tbase=base+text.VirtualAddress-ts
def off2va_text(o): return tbase+o
if __name__=='__main__':
    import numpy as np
    arr=np.frombuffer(d[ts:te-3],dtype=np.uint8)
    rel=np.frombuffer(d[ts:te],dtype='<i4',count=None) if False else None
    for a in sys.argv[1:]:
        tva=off2va(int(a,16))
        # vectorized
        b=np.frombuffer(d,dtype=np.uint8)
        idx=np.arange(ts,te-4)
        vals=(b[idx].astype(np.int64)|(b[idx+1].astype(np.int64)<<8)|(b[idx+2].astype(np.int64)<<16)|(b[idx+3].astype(np.int64)<<24))
        vals=np.where(vals>=2**31,vals-2**32,vals)
        hit=idx[(tbase+idx+4+vals)==tva]
        print(a,hex(tva),[hex(tbase+h) for h in hit])
