import pickle,json,re,os,collections,bisect
from tlk import read_tlk
B="L:/EET_work/BGEE/lang/"
_,en=read_tlk(B+'en_US/dialog.tlk'); _,ja=read_tlk(B+'ja_JP/dialog.tlk')
E=lambda i:en[i][4].decode('utf-8','replace'); J=lambda i:ja[i][4].decode('utf-8','replace')
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
ctx,dlgs,creD=pickle.load(open('index.pkl','rb'))
todo=pickle.load(open('todo.pkl','rb'))
def foreign(s):
    return 'translation notes' in s.lower() or len(re.findall(r'[áéíóöőúüűčřšžłąęńśźż]',s))>3
todo=[i for i in todo if not foreign(E(i)) and i!=30]
def speaker(dlg):
    for cand in (dlg, dlg[:-1], dlg[1:], dlg[:-2]):
        if cand in creD:
            s=creD[cand]
            if s<len(en):
                e=E(s); j=J(s); return e+('/'+j if JP.search(j) else '')
    for L in (7,6):
        ks=sorted(k for k in creD if len(dlg)>L and k.startswith(dlg[:L]))
        if ks and creD[ks[0]]<len(en):
            s=creD[ks[0]]; e=E(s); j=J(s); return e+('/'+j if JP.search(j) else '')
    return None
# dedupe: same English text -> translate once (prefer pc_reply occurrence)
def kind_of(i):
    c=ctx.get(i,[]); ks=[x[0] for x in c]
    for k in ('pc_reply','npc_line','journal'):
        if k in ks: return k
    return ks[0] if ks else 'misc'
groups=collections.defaultdict(list)
for i in todo: groups[E(i)].append(i)
rep={}
for t,ids in groups.items():
    ids.sort(key=lambda i:(kind_of(i)!='pc_reply',i)); rep[ids[0]]=ids[1:]
json.dump({str(k):v for k,v in rep.items() if v},open('dups.json','w'),indent=0)
dl_ids=sorted(i for i in ctx if any(x[0] in('npc_line','pc_reply') for x in ctx[i]))
def near(i):
    k=bisect.bisect_left(dl_ids,i); best=None
    for j in dl_ids[max(0,k-3):k+3]:
        if best is None or abs(j-i)<abs(best-i): best=j
    if best is None or abs(best-i)>40: return None
    x=[y for y in ctx[best] if y[0] in('npc_line','pc_reply')][0]; return x[1]
items=[]
for i in rep:
    c=ctx.get(i,[]); k=kind_of(i); ent={'id':i,'en':E(i),'kind':k}
    dl=[x for x in c if x[0]==k and k in('npc_line','pc_reply')]
    if dl:
        x=dl[0]; ent['dlg']=x[1]; ent['state']=x[2]; sp=speaker(x[1])
        if k=='pc_reply':
            if x[3]<len(en): ent['reply_to_en']=E(x[3])[:300]
            if sp: ent['talking_to']=sp
        elif sp: ent['speaker']=sp
        key=(0,x[1],x[2],i)
    else:
        if k in ('script_text','journal','floating_text'):
            src=[x[1] for x in c if x[0]==k]
            if src: ent['source']=src[0]
        if k=='creature_soundset_line': ent['creature']=[x[1] for x in c if x[0]==k][0]
        if k=='misc':
            nd=near(i)
            if nd:
                ent['near_dlg']=nd; sp=speaker(nd)
                if sp: ent['near_speaker']=sp
        key=(1,0,0,i) if k in ('misc','script_text','journal','floating_text') else (2,k,0,i)
    items.append((key,ent))
items.sort(key=lambda t:t[0])
batches=[];cur=[];size=0;LIM=24000
for key,ent in items:
    l=len(ent['en'])
    if cur and size+l>LIM: batches.append(cur);cur=[];size=0
    cur.append(ent);size+=l
if cur: batches.append(cur)
os.makedirs('batches',exist_ok=True); os.makedirs('out',exist_ok=True)
for k,b in enumerate(batches):
    json.dump(b,open('batches/b%03d.json'%k,'w',encoding='utf-8'),ensure_ascii=False,indent=1)
print(len(todo),'unique',len(rep),'batches',len(batches))
print(collections.Counter(e['kind'] for _,e in items))
