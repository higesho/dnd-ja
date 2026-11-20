import re,collections,pickle,json,glob
from tlk import read_tlk
G="L:/SteamLibrary/steamapps/common/Baldur's Gate Enhanced Edition/lang/"
_,EN=read_tlk(G+'en_US/dialog.tlk'); _,JA=read_tlk(G+'ja_JP/dialog.tlk'); _,JF=read_tlk(G+'ja_JP/dialogF.tlk')
D=lambda e:e[4].decode('utf-8','replace')
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
ctx,_,_=pickle.load(open('index.pkl','rb'))
N=lambda s:re.sub(r'\s+',' ',s).strip()
# 1) same JA used for different EN (possible misplaced line)
byja=collections.defaultdict(set)
for i in range(len(EN)):
    j=N(D(JA[i])); e=N(D(EN[i]))
    if JP.search(j) and len(j)>8: byja[j].add(e)
mis=[]
for i in range(len(EN)):
    j=N(D(JA[i])); e=N(D(EN[i]))
    if j in byja and len(byja[j])>1 and len(e)>25:
        # EN variants differ substantially?
        es=sorted(byja[j],key=len)
        if len(set(x.lower().strip('.!? ')[:20] for x in es))>1: mis.append(i)
# 2) length ratio outliers in dialog
lr=[]
for i in range(len(EN)):
    e=D(EN[i]); j=D(JA[i])
    if not JP.search(j) or len(e)<80: continue
    r=len(j)/len(e)
    if r<0.22 or r>1.6: lr.append((r,i))
# 3) dialogF diverges oddly (F differs from M and F lacks Japanese or very different length)
fd=[]
for i in range(len(EN)):
    j=D(JA[i]); f=D(JF[i])
    if j!=f and JP.search(j) and (not JP.search(f) or abs(len(f)-len(j))>max(20,len(j)*0.6)): fd.append(i)
print('same-JA-different-EN',len(mis),' length outliers',len(lr),' female divergent',len(fd))
pickle.dump({'misplaced':mis,'lenratio':[i for r,i in lr],'female':fd},open('bgee_audit4.pkl','wb'))
import random; random.seed(4)
for i in random.sample(mis,8): print('MIS #%d EN:%s | JA:%s'%(i,D(EN[i])[:80],D(JA[i])[:60]))
for r,i in random.sample(lr,8): print('LEN %.2f #%d EN:%s | JA:%s'%(r,i,D(EN[i])[:80].replace('\n','/'),D(JA[i])[:60].replace('\n','/')))
for i in fd[:5]: print('FEM #%d M:%s | F:%s'%(i,D(JA[i])[:60],D(JF[i])[:60]))
