import struct,pickle,collections,os,re
import sys
GAME=sys.argv[1] if len(sys.argv)>1 else "L:/EET_work/BGEE"
OUT=sys.argv[2] if len(sys.argv)>2 else "index.pkl"
d=open(GAME+'/chitin.key','rb').read()
sig,ver,nb,nr,bo,ro=struct.unpack_from('<4s4sIIII',d,0)
bifs=[]
for i in range(nb):
    ln,no,nl,loc=struct.unpack_from('<IIHH',d,bo+i*12)
    bifs.append(d[no:no+nl].rstrip(b'\0').decode().replace(chr(92),'/'))
res={}
for i in range(nr):
    name,typ,loc=struct.unpack_from('<8sHI',d,ro+i*14)
    res[(name.rstrip(b'\0').decode('latin1').upper(),typ)]=loc
cache={}
def getbif(bi):
    if bi not in cache:
        p=GAME+'/'+bifs[bi]
        if not os.path.exists(p):
            alt=[q for q in (GAME+'/sod-dlc/'+os.path.basename(p),) if os.path.exists(q)]
            p=alt[0] if alt else p
        dd=open(p,'rb').read()
        s,v,nf,nt,off=struct.unpack_from('<4s4sIII',dd,0)
        ent={}
        for i in range(nf):
            l,o,sz,t=struct.unpack_from('<IIIH',dd,off+i*16); ent[l&0x3fff]=(o,sz)
        cache[bi]=(dd,ent)
    return cache[bi]
EXT={1005:'ITM',1006:'SPL',1007:'BCS',1009:'CRE',1011:'DLG',1014:'STO'}
OV={}
if os.path.isdir(GAME+'/override'):
    for f in os.listdir(GAME+'/override'):
        b,_,x=f.rpartition('.')
        for t,e in EXT.items():
            if x.upper()==e: OV[(b.upper(),t)]=GAME+'/override/'+f
for k in OV:
    if k not in res: res[k]=None
def rd(name,typ):
    if (name,typ) in OV: return open(OV[(name,typ)],'rb').read()
    loc=res[(name,typ)]; dd,ent=getbif(loc>>20); o,s=ent[loc&0x3fff]; return dd[o:o+s]
ctx=collections.defaultdict(list); dlgs={}; creD={}; acts=collections.defaultdict(list)
missing=0
JR=re.compile(r'(AddJournalEntry|EraseJournalEntry)\s*\(\s*(\d+)',re.I)
DS=re.compile(r'(Display\w*|FloatMessage\w*)\s*\([^)]*?(\d{4,})',re.I)
for (n,t) in list(res):
    if t not in (1005,1006,1007,1009,1011,1014): continue
    try: dd=rd(n,t)
    except Exception: missing+=1; continue
    sig=dd[:8]
    if sig==b'DLG V1.0':
        ns,os_,nt,ot=struct.unpack_from('<IIII',dd,8)
        aoff,acnt=struct.unpack_from('<II',dd,0x28)
        actions=[]
        for i in range(acnt):
            o,l=struct.unpack_from('<II',dd,aoff+i*8); actions.append(dd[o:o+l].decode('latin1'))
        states=[];trans=[]
        for i in range(nt):
            fl,txt,jr,tr,ac=struct.unpack_from('<IIIII',dd,ot+i*32)
            nd=dd[ot+i*32+20:ot+i*32+28].rstrip(b'\0').decode('latin1').upper(); nst=struct.unpack_from('<I',dd,ot+i*32+28)[0]
            trans.append((fl,txt,jr,nd,nst,ac))
        for i in range(ns):
            sr,ft,ntr,trg=struct.unpack_from('<IIIi',dd,os_+i*16); states.append((sr,ft,ntr))
        dlgs[n]=(states,trans,actions)
        for a in actions:
            for m in JR.finditer(a): acts[int(m.group(2))].append(('journal',n))
            for m in DS.finditer(a): acts[int(m.group(2))].append(('floating_text',n))
    elif sig in (b'ITM V1  ',b'ITM V1.1'):
        for off,k in ((8,'item_name_unid'),(0xc,'item_name'),(0x50,'item_desc_unid'),(0x54,'item_desc')):
            ctx[struct.unpack_from('<I',dd,off)[0]].append((k,n))
    elif sig==b'SPL V1  ':
        for off,k in ((8,'spell_name'),(0x50,'spell_desc')):
            ctx[struct.unpack_from('<I',dd,off)[0]].append((k,n))
    elif sig[:4]==b'CRE ':
        ln,sn=struct.unpack_from('<II',dd,8)
        ctx[ln].append(('creature_name',n)); ctx[sn].append(('creature_tooltip',n))
        dl=dd[0x2cc:0x2d4].rstrip(b'\0').decode('latin1').upper()
        if dl: creD.setdefault(dl,ln)
        for j in range(100):
            s=struct.unpack_from('<I',dd,0xa4+4*j)[0]
            if s!=0xffffffff: ctx[s].append(('creature_soundset_line',n))
    elif sig==b'STORV1.0':
        ctx[struct.unpack_from('<I',dd,0xc)[0]].append(('store_name',n))
    elif dd[:2]==b'SC':
        txt=dd.decode('latin1').replace('\r','')
        for m in re.finditer(r'AC\n(\d+)OB\n[^\n]*OB\nOB\n[^\n]*OB\nOB\n[^\n]*OB\n(-?\d+) ',txt):
            aid=int(m.group(1)); a=int(m.group(2))
            if a>100: acts[a].append(('journal' if aid==173 else 'script_text',n,aid))
for n,(states,trans,actions) in dlgs.items():
    for si,(sr,ft,ntr) in enumerate(states):
        ctx[sr].append(('npc_line',n,si))
        for k in range(ft,ft+ntr):
            if k>=len(trans): break
            fl,txt,jr,nd,nst,ac=trans[k]
            if fl&1: ctx[txt].append(('pc_reply',n,si,sr))
            if fl&16: ctx[jr].append(('journal',n,si))
for k,v in acts.items(): ctx[k].extend(v)
pickle.dump((dict(ctx),{k:(v[0],[t[:5] for t in v[1]]) for k,v in dlgs.items()},creD),open(OUT,'wb'))
print('dlgs',len(dlgs),'ctx',len(ctx),'cre',len(creD),'missing',missing)
