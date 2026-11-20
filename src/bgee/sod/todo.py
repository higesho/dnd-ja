import pickle,re,collections,json
from tlk import read_tlk
B="L:/EET_work/BGEE/lang/"
_,en=read_tlk(B+'en_US/dialog.tlk'); _,ja=read_tlk(B+'ja_JP/dialog.tlk')
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
E=lambda i:en[i][4].decode('utf-8','replace'); J=lambda i:ja[i][4].decode('utf-8','replace')
def trivial(s):
    t=re.sub(r'<[^>]*>','',s).strip()
    return (not re.search(r'[A-Za-z]{2,}',t)) or t in ('<NO TEXT>','placeholder') or 'DO NOT USE' in s or t.startswith('***') or re.fullmatch(r"\[[^\]]*\]",t)
todo=[i for i in range(len(en)) if E(i).strip() and not JP.search(J(i)) and not trivial(E(i))]
pickle.dump(todo,open('todo.pkl','wb'))
ctx,dlgs,creD=pickle.load(open('index.pkl','rb'))
c=collections.Counter(); ch=collections.Counter()
for i in todo:
    k=ctx.get(i,[('misc',)])
    kinds=[x[0] for x in k]
    kk='pc_reply' if 'pc_reply' in kinds else ('npc_line' if 'npc_line' in kinds else kinds[0])
    c[kk]+=1; ch[kk]+=len(E(i))
print(len(todo),sum(len(E(i)) for i in todo)); print(c); print(ch)
print('range',todo[0],todo[-1], 'below 30000:',sum(1 for i in todo if i<30000))
ex=[i for i in todo if i<30000][:15]
for i in ex: print(i,repr(E(i)[:80]),'| ja:',repr(J(i)[:40]))
