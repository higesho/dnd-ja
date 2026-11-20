import re,collections,pickle,json,glob
from tlk import read_tlk
G="L:/SteamLibrary/steamapps/common/Baldur's Gate Enhanced Edition/lang/"
_,EN=read_tlk(G+'en_US/dialog.tlk'); _,JA=read_tlk(G+'ja_JP/dialog.tlk')
D=lambda e:e[4].decode('utf-8','replace')
R=pickle.load(open('bgee_audit.pkl','rb')); ctx,_,_=pickle.load(open('index.pkl','rb'))
todo=set(pickle.load(open('todo.pkl','rb')))
batched=set()
for f in glob.glob('batches/b*.json'):
    for e in json.load(open(f,encoding='utf-8')): batched.add(e['id'])
dups=json.load(open('dups.json'))
for k,v in dups.items(): batched.update(v)
u=R['untranslated']; print('untranslated in our SoD-project batches:',sum(1 for i in u if i in batched),'/',len(u))
kind=collections.Counter((ctx.get(i,[('unused',)])[0][0]) for i in u); print(' kinds',kind.most_common())
TOK=re.compile(r'<[A-Za-z_0-9]+>')
bad=[];miss=[]
for i in R['token_mismatch']:
    e=collections.Counter(TOK.findall(D(EN[i]))); j=collections.Counter(TOK.findall(D(JA[i])))
    if j-e: bad.append((i,dict(j-e)))
    else: miss.append((i,dict(e-j)))
print('token: ja has extra/wrong tokens',len(bad),' ja dropped tokens',len(miss))
print(collections.Counter(t for _,d in bad for t in d).most_common(12))
print(collections.Counter(t for _,d in miss for t in d).most_common(8))
for i,d in bad[:6]: print('  #%d %s | EN:%s | JA:%s'%(i,d,D(EN[i])[:90],D(JA[i])[:90]))
# stat diffs
def fields(s):
    s=s.translate(str.maketrans('０１２３４５６７８９','0123456789'))
    return s
print('--- stat mismatch samples')
import random; random.seed(7)
for i in random.sample(R['stat_number_mismatch'],10):
    e=D(EN[i]); j=fields(D(JA[i]))
    ne=collections.Counter(re.findall(r'\d+',e)); nj=collections.Counter(re.findall(r'\d+',j))
    print('  #%d en-only %s ja-only %s  %s'%(i,dict(ne-nj),dict(nj-ne),e.split('\n')[0][:40]))
