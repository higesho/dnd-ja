# usage: python -X utf8 find.py "English phrase" [max]
# Shows existing translated entries (EN / JA) containing the phrase, for terminology lookup.
import sys,re,pickle
from tlk import read_tlk
G="L:/SteamLibrary/steamapps/common/Baldur's Gate II Enhanced Edition/"
_,en=read_tlk(G+'lang/en_US/dialog.tlk')
_,ja=read_tlk(G+'BG2EE日本語化パッチ_2.3.67.3_0.91/lang/ja_JP/dialog.tlk')
q=sys.argv[1]; mx=int(sys.argv[2]) if len(sys.argv)>2 else 6
rx=re.compile(re.escape(q),re.I); n=0
for i in range(len(ja)):
    e=en[i][4].decode('utf-8','replace')
    if rx.search(e):
        j=ja[i][4].decode('utf-8','replace')
        if not re.search(r'[\u3040-\u9fff]',j): continue
        if len(e)>300:
            m=rx.search(e); e='…'+e[max(0,m.start()-120):m.end()+120]+'…'
        print('#%d EN: %s\n     JA: %s'%(i,e.replace('\n','/'),j[:400].replace('\n','/')))
        n+=1
        if n>=mx: break
