import pickle,re,collections,difflib
from tlk import read_tlk
B="../../Baldur's Gate Enhanced Edition/lang/"
_,en=read_tlk('../lang/en_US/dialog.tlk'); _,ja=read_tlk('orig/ja_JP/dialog.tlk')
_,be=read_tlk(B+'en_US/dialog.tlk'); _,bj=read_tlk(B+'ja_JP/dialog.tlk')
ctx,_,_=pickle.load(open('index.pkl','rb'))
refs=pickle.load(open('refs2.pkl','rb'))
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
m=collections.defaultdict(list)
for k in range(min(len(be),len(bj))):
    s=bj[k][4]
    if JP.search(s.decode('utf-8','replace')): m[s].append(k)
norm=lambda b:re.sub(r'[^a-z0-9]','',b.decode('utf-8','replace').lower())
out=[]
for i in range(len(en)):
    js=ja[i][4]
    if js not in m: continue
    ei=norm(en[i][4])
    best=max(difflib.SequenceMatcher(None,ei,norm(be[k][4])).ratio() for k in m[js][:20])
    if best<0.5:
        out.append((i,best,m[js][0]))
print(len(out))
for i,b,k in out[:80]:
    print(i,'R' if i in ctx else ' ','%.2f'%b,en[i][4][:45],'|',ja[i][4].decode()[:25],'| BG:',be[k][4][:30])
pickle.dump([o[0] for o in out],open('bgcopy.pkl','wb'))
