import sys,re,json,glob,collections
sys.path.insert(0,'../mods'); from tra import read
from tlk import read_tlk
from alltr import load,batches
N=lambda s:re.sub(r'\s+',' ',s).strip()
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
D=lambda e:e[4].decode('utf-8','replace')
m={}
def add(e,j):
    if e and j and JP.search(j[0]): m.setdefault(N(e),j)
# our translations
tb=load('b'); eb=batches('b'); te=load('e'); ee=batches('e')
for i,v in te.items(): add(ee[i]['en'],v)
for i,v in tb.items(): add(eb[i]['en'],v)
# official
for pe,pj,pf in [("L:/EET_work/BGEE/lang/en_US/dialog.tlk","orig/bgee/dialog.tlk","orig/bgee/dialogf.tlk"),
                 ("L:/SteamLibrary/steamapps/common/Baldur's Gate II Enhanced Edition/lang/en_US/dialog.tlk","L:/SteamLibrary/steamapps/common/Baldur's Gate II Enhanced Edition/lang/ja_JP/dialog.tlk","L:/SteamLibrary/steamapps/common/Baldur's Gate II Enhanced Edition/lang/ja_JP/dialogF.tlk")]:
    _,E=read_tlk(pe);_,J=read_tlk(pj);_,F=read_tlk(pf)
    for i in range(min(len(E),len(J))): add(D(E[i]),(D(J[i]),D(F[i])))
src='L:/EET_work/EET/EET/lang/en_US/'
res={};miss=[]
for f in sorted(glob.glob(src+'*.tra')):
    name=f.split('/')[-1]
    t=read(f)
    for k,v in t.items():
        e=v[0][0]; key=N(e.replace('###',' '))
        if key in m: res.setdefault(name,{})[k]=m[key]
        else: miss.append((name,k,e,v[0][1]))
print('matched',sum(len(x) for x in res.values()),'missing',len(miss))
print(collections.Counter(x[0] for x in miss))
json.dump({'matched':{n:{k:list(v) for k,v in d.items()} for n,d in res.items()},'missing':miss},open('eet_tra_map.json','w',encoding='utf-8'),ensure_ascii=False,indent=1)
