import struct,pickle,re,collections,os
import numpy as np
from biff import load_key
import index as I
unt=set(pickle.load(open('unt.pkl','rb')))
ctx,dlgs,creD=pickle.load(open('index.pkl','rb'))
bifs,res=load_key()
refs=collections.defaultdict(set)   # strref -> set of 'TYPE:NAME'
unt_arr=np.array(sorted(unt),dtype=np.uint32)
TEXT_SIGS=(b'SC\n',b'2DA',b' 2DA')
for (n,t) in res:
    try: d=I.rd(n,t)
    except Exception: continue
    head=d[:8]
    if t in (0x3e8,0x4,0x3eb,0x3ec,0x3e9,0x404,0x1,0x6,0x3ef,0x3fd,0x40a,0x405):  # skip media/tiles
        if head[:3] not in TEXT_SIGS: continue
    if head[:3]==b'SC\n' or b'2DA' in head or t in (0x408,0x409,0x3f8,0x3fb,0x3f9):
        # text resource
        for m in re.finditer(rb'(?<![0-9])([0-9]{2,6})(?![0-9])',d):
            v=int(m.group(1))
            if v in unt: refs[v].add('%x:%s'%(t,n))
        continue
    if len(d)<8: continue
    a=np.frombuffer(d,dtype=np.uint8)
    for k in range(4):
        L=(len(d)-k)//4
        if L<=0: continue
        v=np.frombuffer(d[k:k+4*L],dtype='<u4')
        hit=np.intersect1d(v,unt_arr)
        for h in hit: refs[int(h)].add('%x:%s'%(t,n))
pickle.dump(dict(refs),open('refs.pkl','wb'))
allref=set(refs)|set(i for i in unt if i in ctx)
print('unt',len(unt),'ctx',len([i for i in unt if i in ctx]),'binary/text refs',len(refs),'union',len(allref))
c=collections.Counter()
for i in refs:
    for r in refs[i]: c[r.split(':')[0]]+=1
print(c.most_common())
pickle.dump(sorted(allref),open('unt_ref.pkl','wb'))
