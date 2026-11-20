import re,collections,pickle
from tlk import read_tlk
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
N=lambda s:re.sub(r'\s+',' ',s).strip()
_,eet=read_tlk("L:/EET_work/EET/lang/ja_JP/dialog.tlk")
_,ben=read_tlk("L:/EET_work/BGEE/lang/en_US/dialog.tlk")
_,bja=read_tlk("L:/EET_work/BGEE/lang/ja_JP/dialog.tlk")
D=lambda e:e[4].decode('utf-8','replace')
todo=set(pickle.load(open('todo.pkl','rb')))
m={}
for i,e in enumerate(ben):
    t=N(D(e))
    if t: m.setdefault(t,i)
lab=re.compile(r"^\[[^\]]*\]$|^<NO TEXT>$|DO NOT USE|^\*\*\*")
c=collections.Counter(); un=[]
for i,e in enumerate(eet):
    s=D(e)
    if not re.search(r'[A-Za-z]{3,}',s) or JP.search(s): continue
    t=N(s)
    if lab.search(t): c['label']+=1; continue
    if t in m:
        j=m[t]; c['todo' if j in todo else ('bgeeja' if JP.search(D(bja[j])) else 'bgee-other')]+=1
    else: c['unmatched']+=1; un.append(i)
print(c)
pickle.dump(un,open('eet_unmatched.pkl','wb'))
import random; random.seed(5)
for i in sorted(random.sample(un,40)): print(i,repr(D(eet[i])[:150]))
print('---- bgee-other')
k=0
for i,e in enumerate(eet):
    s=D(e)
    if not re.search(r'[A-Za-z]{3,}',s) or JP.search(s): continue
    t=N(s)
    if lab.search(t) or t not in m: continue
    j=m[t]
    if j not in todo and not JP.search(D(bja[j])):
        k+=1
        if k%40==0: print(i,j,repr(t[:90]),'| bja:',repr(D(bja[j])[:30]))
