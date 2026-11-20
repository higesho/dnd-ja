import struct,pickle,collections,os
from biff import *
bifs,res=load_key()
# cache bif file contents lazily
cache={}
def getbif(bi):
    if bi not in cache:
        p=GAME+'/'+bifs[bi].replace(chr(92),'/')
        d=open(p,'rb').read()
        sig,ver,nf,nt,off=struct.unpack_from('<4s4sIII',d,0)
        ent={}
        for i in range(nf):
            l,o,s,t=struct.unpack_from('<IIIH',d,off+i*16)
            ent[l&0x3fff]=(o,s)
        cache[bi]=(d,ent)
    return cache[bi]
def rd(name,typ):
    loc=res[(name,typ)]; d,ent=getbif(loc>>20); o,s=ent[loc&0x3fff]; return d[o:o+s]
byt=collections.defaultdict(list)
for (n,t) in res: byt[t].append(n)
ctx=collections.defaultdict(list)  # strref -> list of (kind, info)
dlgs={}
creD={}  # dlg resref -> cre name strref
for t in byt:
    for n in byt[t]:
        try: d=rd(n,t)
        except Exception: continue
        sig=d[:8]
        if sig==b'DLG V1.0':
            ns,os_,nt,ot=struct.unpack_from('<IIII',d,8)
            states=[];trans=[]
            for i in range(nt):
                fl,txt,jr,tr,ac=struct.unpack_from('<IIIII',d,ot+i*32)
                nd=d[ot+i*32+20:ot+i*32+28].rstrip(b'\0').decode('latin1').upper(); nst=struct.unpack_from('<I',d,ot+i*32+28)[0]
                trans.append((fl,txt,jr,nd,nst))
            for i in range(ns):
                sr,ft,ntr,trg=struct.unpack_from('<IIIi',d,os_+i*16)
                states.append((sr,ft,ntr))
            dlgs[n]=(states,trans)
        elif sig in (b'ITM V1  ',b'ITM V1.1'):
            for off,k in ((8,'item_name_unid'),(0xc,'item_name'),(0x50,'item_desc_unid'),(0x54,'item_desc')):
                ctx[struct.unpack_from('<I',d,off)[0]].append((k,n))
        elif sig==b'SPL V1  ':
            for off,k in ((8,'spell_name'),(0x50,'spell_desc')):
                ctx[struct.unpack_from('<I',d,off)[0]].append((k,n))
        elif sig[:4]==b'CRE ':
            ln,sn=struct.unpack_from('<II',d,8)
            ctx[ln].append(('creature_name',n)); ctx[sn].append(('creature_tooltip',n))
            try:
                dl=d[0x2cc:0x2d4].rstrip(b'\0').decode('latin1').upper()
                if dl: creD.setdefault(dl,ln)
            except: pass
            for j in range(100):
                s=struct.unpack_from('<I',d,0xa4+4*j)[0]
                if s!=0xffffffff: ctx[s].append(('creature_soundset_line',n))
        elif sig==b'STOR':
            ctx[struct.unpack_from('<I',d,0xc)[0]].append(('store_name',n))
        elif sig[:4]==b'AREA':
            pass
for n,(states,trans) in dlgs.items():
    for si,(sr,ft,ntr) in enumerate(states):
        ctx[sr].append(('npc_line',n,si))
        for k in range(ft,ft+ntr):
            if k>=len(trans): break
            fl,txt,jr,nd,nst=trans[k]
            if fl&1: ctx[txt].append(('pc_reply',n,si,sr))
            if fl&16: ctx[jr].append(('journal',n,si))
pickle.dump((dict(ctx),dlgs,creD),open('index.pkl','wb'))
print(len(dlgs),len(ctx),len(creD))
