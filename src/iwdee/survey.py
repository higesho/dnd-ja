import sys,re
from tlk import read_tlk
G='../lang/'
_,en=read_tlk(G+'en_US/dialog.tlk')
_,ja=read_tlk(G+'ja_JP/dialog.tlk')
_,jf=read_tlk(G+'ja_JP/dialogf.tlk')
print(len(en),len(ja),len(jf))
def isjp(b):
    try: s=b.decode('utf-8')
    except: return None
    return any(ord(c)>0x3000 for c in s)
same=0;ascii_=0;bad=0;ex=[]
for i,(e,j) in enumerate(zip(en,ja)):
    es=e[4];js=j[4]
    if not es.strip(): continue
    r=isjp(js)
    if r is None: bad+=1;continue
    if not r and re.search(rb'[a-z]{3,} [a-z]{3,}',js):
        ascii_+=1
        if len(ex)<40: ex.append((i,js[:100]))
print('bad utf8',bad,'ascii-untranslated',ascii_)
for x in ex: print(x)
# flags diff
fd=sum(1 for e,j in zip(en,ja) if (e[0]&4)!=(j[0]&4) and b'<' in j[4])
print('flag4 mismatch w/ tokens',fd)
nf=sum(1 for j in ja if b'<' in j[4] and re.search(rb'<[A-Z_]+>',j[4]) and not j[0]&4)
print('tokens without flag4',nf)
import pickle,collections
ctx,dlgs,creD=pickle.load(open('index.pkl','rb'))
unt=[i for i,(e,j) in enumerate(zip(en,ja)) if e[4].strip() and not isjp(j[4]) and re.search(rb'[A-Za-z]{3,}',j[4])]
print('unt broad',len(unt))
ref=[i for i in unt if i in ctx]
print('referenced',len(ref),collections.Counter(ctx[i][0][0] for i in ref))
dl=collections.Counter(ctx[i][0][1] for i in ref if ctx[i][0][0] in('npc_line','pc_reply'))
print(dl.most_common(60))
# ranges
import itertools
def rng(L):
    out=[]
    for k,g in itertools.groupby(enumerate(L),lambda t:t[1]-t[0]):
        g=list(g); out.append((g[0][1],g[-1][1]))
    return out
R=rng(unt); print(len(R)); print([r for r in R if r[1]-r[0]>20])
pickle.dump(unt,open('unt.pkl','wb'))
