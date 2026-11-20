import struct,os,zlib
GAME=r"L:/SteamLibrary/steamapps/common/Icewind Dale Enhanced Edition"
TYPES={0x3f2:'MOS',0x3eb:'BAM',0x3ef:'BMP',0x3fd:'TOH',0x408:'MENU',0x409:'LUA',0x40a:'TTF',0x40b:'PNG',0x3f3:'ITM',0x3ee:'SPL',0x3ed:'CRE',0x3f8:'2DA',0x3f9:'IDS',0x3f4:'DLG',0x3f1:'ARE',0x3f6:'BCS',0x3fe:'GLSL',0x404:'PVRZ',0x3f0:'WED',0x400:'INI',0x3fc:'WMP',0x3ec:'TIS',0x3f5:'CHU',0x403:'MENU?',0x3f7:'STO',0x3fa:'EFF',0x3fb:'BS',0x401:'SRC',0x402:'FNT',0x3ea:'WBM',0x3e8:'WAV',0x3e9:'PLT',0x3ff:'GUI'}
def load_key():
    d=open(GAME+'/chitin.key','rb').read()
    sig,ver,nb,nr,bo,ro=struct.unpack_from('<4s4sIIII',d,0)
    bifs=[]
    for i in range(nb):
        ln,no,nl,loc=struct.unpack_from('<IIHH',d,bo+i*12)
        bifs.append(d[no:no+nl].rstrip(b'\0').decode())
    res={}
    for i in range(nr):
        name,typ,loc=struct.unpack_from('<8sHI',d,ro+i*14)
        name=name.rstrip(b'\0').decode('latin1').upper()
        res[(name,typ)]=loc
    return bifs,res
_bc={}
def read_res(name,typ):
    bifs,res=load_key()
    loc=res[(name.upper(),typ)]
    bi=loc>>20; idx=loc&0x3fff
    p=GAME+'/'+bifs[bi].replace(chr(92),'/')
    d=open(p,'rb').read()
    sig,ver,nf,nt,off=struct.unpack_from('<4s4sIII',d,0)
    for i in range(nf):
        l,o,s,t=struct.unpack_from('<IIIH',d,off+i*16)
        if l&0x3fff==idx: return d[o:o+s]
    raise KeyError
