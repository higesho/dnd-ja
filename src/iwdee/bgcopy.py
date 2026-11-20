import pickle,re,collections
from tlk import read_tlk
B="../../Baldur's Gate Enhanced Edition/lang/"
B2="../../Baldur's Gate II Enhanced Edition/lang/"
_,en=read_tlk('../lang/en_US/dialog.tlk'); _,ja=read_tlk('orig/ja_JP/dialog.tlk')
srcs=[]
for p in (B,B2):
    try:
        _,be=read_tlk(p+'en_US/dialog.tlk'); _,bj=read_tlk(p+'ja_JP/dialog.tlk'); srcs.append((p,be,bj))
    except Exception as ex: print(p,ex)
ctx,_,_=pickle.load(open('index.pkl','rb'))
bad=[]
for i in range(len(en)):
    js=ja[i][4]; es=en[i][4]
    if not js.strip(): continue
    for p,be,bj in srcs:
        if i<len(bj) and bj[i][4]==js and i<len(be) and be[i][4].strip()!=es.strip():
            bad.append((i,p)); break
print(len(bad))
ref=[b for b in bad if b[0] in ctx]
print('in ctx',len(ref))
for i,p in bad[:60]:
    print(i,'R' if i in ctx else ' ',p[6:12],en[i][4][:50],'|',ja[i][4].decode()[:30])
pickle.dump([b[0] for b in bad],open('bgcopy.pkl','wb'))
