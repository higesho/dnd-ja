import re,collections,pickle
from tlk import read_tlk
G="L:/SteamLibrary/steamapps/common/Baldur's Gate Enhanced Edition/lang/"
_,EN=read_tlk(G+'en_US/dialog.tlk'); _,JA=read_tlk(G+'ja_JP/dialog.tlk')
D=lambda e:e[4].decode('utf-8','replace')
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
ctx,_,_=pickle.load(open('index.pkl','rb'))
Z=str.maketrans('０１２３４５６７８９','0123456789')
def stat(s,keys):
    for k in keys:
        p=s.find(k)
        if p>=0: return s[p:]
    return None
def nums(s): return collections.Counter(re.findall(r'\d+',s.translate(Z)))
out=collections.defaultdict(list)
for i in range(len(EN)):
    e=D(EN[i]); j=D(JA[i])
    if not JP.search(j): continue
    k=[x[0] for x in ctx.get(i,[])]
    if any(x.startswith('item_desc') for x in k):
        a=stat(e,['STATISTICS:']); b=stat(j,['情報：','情報:','能力値：'])
        if a and b and nums(a)!=nums(b): out['item'].append(i)
        elif a and not b: out['item_nostat'].append(i)
    elif 'spell_desc' in k:
        a=stat(e,['Level:']); b=stat(j,['レベル：','レベル:'])
        if a and b and nums(a)!=nums(b): out['spell'].append(i)
for k,v in out.items(): print(k,len(v))
pickle.dump(dict(out),open('bgee_stats.pkl','wb'))
import random; random.seed(1)
for k in out:
  for i in random.sample(out[k],min(5,len(out[k]))):
    e=D(EN[i]); j=D(JA[i])
    a=stat(e,['STATISTICS:','Level:']) or ''; b=stat(j,['情報：','レベル：']) or ''
    print('== %s #%d en-only %s ja-only %s'%(k,i,dict(nums(a)-nums(b)),dict(nums(b)-nums(a))))
    print('   EN:',a[:300].replace('\n','/')); print('   JA:',b[:300].replace('\n','/'))
