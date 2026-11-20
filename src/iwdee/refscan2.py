import struct,pickle,re,collections
import numpy as np
from biff import load_key
import index as I
from tlk import read_tlk
unt=set(pickle.load(open('unt.pkl','rb')))
ctx,dlgs,creD=pickle.load(open('index.pkl','rb'))
bifs,res=load_key()
refs=collections.defaultdict(set)
ACT=re.compile(r'OB\n(-?\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+)"')
STR_OPC={139,180,206,253,254,267,290,319,324,330,332,335}
def scan_eff(d,off,cnt,n,size=48):
    for k in range(cnt):
        o=off+k*size
        if o+12>len(d): break
        opc=struct.unpack_from('<H',d,o)[0]
        p1,p2=struct.unpack_from('<ii',d,o+4)
        for v in (p1,p2):
            if opc in STR_OPC and v in unt: refs[v].add('eff%d:%s'%(opc,n))
for (n,t) in res:
    try: d=I.rd(n,t)
    except Exception: continue
    if t==0x3ef or t==0x3fb:   # BCS / BS
        s=d.decode('latin1')
        for m in ACT.finditer(s):
            for g in (1,4,5):
                v=int(m.group(g))
                if v in unt and v>=100: refs[v].add('bcs:'+n)
    elif t in (0x3f4,0x408,0x409,0x400):  # 2DA MENU LUA INI
        for m in re.finditer(rb'(?<![0-9.])([0-9]{3,6})(?![0-9])',d):
            v=int(m.group(1))
            if v in unt and v>=1000: refs[v].add('txt%x:%s'%(t,n))
    elif t in (0x3f2,0x3f7,0x3f5):  # ARE WMP CHU GAM aligned scan
        L=len(d)//4
        v=np.frombuffer(d[:4*L],dtype='<u4')
        for h in set(v.tolist())&unt:
            if h>=1000: refs[h].add('bin%x:%s'%(t,n))
    elif d[:4] in (b'ITM ',b'SPL '):
        if d[:4]==b'ITM ':
            eo,=struct.unpack_from('<I',d,0x6a); hc,=struct.unpack_from('<H',d,0x68) if False else (0,)
        # all effects in file: global + extended headers
        eo,=struct.unpack_from('<I',d,0x6a)
        hdr_off,=struct.unpack_from('<I',d,0x64); nh,=struct.unpack_from('<H',d,0x68)
        ge,=struct.unpack_from('<H',d,0x70)
        tot=(len(d)-eo)//48
        scan_eff(d,eo,tot,n)
    elif d[:4]==b'CRE ':
        eo,ne=struct.unpack_from('<II',d,0x2c4)
        ver=d[33]
        scan_eff(d,eo,ne,n,264 if d[0x33]==1 else 48)
    elif d[:4]==b'EFF ':
        scan_eff(d,8,1,n,264)
sel=set(i for i in unt if i in ctx)|set(refs)
pickle.dump(dict(refs),open('refs2.pkl','wb'))
pickle.dump(sorted(sel),open('unt_ref.pkl','wb'))
_,en=read_tlk('../lang/en_US/dialog.tlk')
print(len(sel),sum(len(en[i][4]) for i in sel))
c=collections.Counter()
for i in set(refs)-set(ctx):
    for r in refs[i]: c[re.sub(r'\d+:.*|:.*','',r)]+=1
print(c)
