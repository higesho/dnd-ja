import struct,sys
def read_tlk(path):
    d=open(path,'rb').read()
    sig,ver,lang,n,off=struct.unpack_from('<4s4sHII',d,0)
    ents=[]
    for i in range(n):
        flags,snd,vol,pitch,o,l=struct.unpack_from('<H8sIIII',d,18+i*26)
        ents.append((flags,snd,vol,pitch,d[off+o:off+o+l]))
    return lang,ents
def write_tlk(path,lang,ents):
    hdr=struct.pack('<4s4sHII',b'TLK ',b'V1  ',lang,len(ents),18+26*len(ents))
    tab=bytearray();blob=bytearray()
    for flags,snd,vol,pitch,s in ents:
        tab+=struct.pack('<H8sIIII',flags,snd,vol,pitch,len(blob),len(s)); blob+=s
    open(path,'wb').write(hdr+tab+blob)
