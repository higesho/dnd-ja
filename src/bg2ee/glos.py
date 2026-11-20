import pickle,collections,re,sys
from tlk import *
G="L:/SteamLibrary/steamapps/common/Baldur's Gate II Enhanced Edition/"
_,en=read_tlk(G+'lang/en_US/dialog.tlk')
_,ja=read_tlk(G+'BG2EE日本語化パッチ_2.3.67.3_0.91/lang/ja_JP/dialog.tlk')
unt=set(pickle.load(open('unt.pkl','rb'))+pickle.load(open('part.pkl','rb')))
KAT=re.compile(r'[\u30a0-\u30ff\u4e00-\u9fff][\u30a0-\u30ff\u4e00-\u9fff・＝=]*')
E=[en[i][4].decode('utf-8','replace') for i in range(len(ja))]
J=[ja[i][4].decode('utf-8','replace') for i in range(len(ja))]
glob=collections.Counter()
for j in J: glob.update(set(KAT.findall(j)))
def look(t):
    rx=re.compile(r'\b'+re.escape(t)+r'\b')
    hits=[i for i in range(len(ja)) if i not in unt and rx.search(E[i])]
    c=collections.Counter()
    for i in hits: c.update(set(KAT.findall(J[i])))
    n=len(hits)
    sc=sorted(((v/n)*(v/ (glob[k]))**0.5,k,v) for k,v in c.items() if v>=max(1,n*0.3) and len(k)>=2)
    return n,[(k,v) for s,k,v in sorted(sc,reverse=True)[:3]]
for t in sys.argv[1:]:
    print(t,look(t))
