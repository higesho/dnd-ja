import pickle,json,re,os,collections
from tlk import read_tlk
G="L:/SteamLibrary/steamapps/common/Baldur's Gate II Enhanced Edition/"
_,en=read_tlk(G+'lang/en_US/dialog.tlk')
_,ja=read_tlk(G+'BG2EE日本語化パッチ_2.3.67.3_0.91/lang/ja_JP/dialog.tlk')
ctx,dlgs,creD=pickle.load(open('index.pkl','rb'))
unt=sorted(set(pickle.load(open('unt.pkl','rb'))+pickle.load(open('part.pkl','rb'))))
E=lambda i:en[i][4].decode('utf-8','replace')
JA=lambda i:ja[i][4].decode('utf-8','replace') if i<len(ja) else ''
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
EXCL=set([73481,73482,73483,73980,83359,83363,99385,99386,103037,103038,103039,103040,103044,103045,
 103214,103215,103216,103217,103218,103219,103580,103581,103582,103583,103587,103588,103589,103590,103591,103593,103594,103595,103596,103597,103592,103598]+list(range(102827,102832)))
def trivial(s):
    t=re.sub(r'<[^>]*>','',s)
    return not re.search(r'[A-Za-z]{2,}',t) or s.strip() in ('<NO TEXT>','placeholder','—NOTEXT—','**REMOVED**') or s.startswith('***Placeholder') or 'DO NOT USE' in s
todo=[i for i in unt if i not in EXCL and not trivial(E(i))]
SPK={'RASAADJ':'RASAAD','RASAA25J':'RASAAD','HEXXATJ':'HEXXAT','HEXXA25J':'HEXXAT','NEERAJ':'NEERA','NEERA25J':'NEERA','DORNJ':'DORN','DORN25J':'DORN','WILSONJ':'WILSON','VICONIJ':'VICONIA','BVICONI':'VICONIA','TBXANJ':'XAN'}
NM={'RASAAD':'Rasaad/ラサード','HEXXAT':'Hexxat/ヘクサート','NEERA':'Neera/ニーラ','DORN':'Dorn/ドルン','WILSON':'Wilson/ウィルソン','VICONIA':'Viconia/ヴィコニア','XAN':'Xan/ザン'}
def speaker(dlg):
    if dlg in SPK: return NM[SPK[dlg]]
    s=None
    for cand in (dlg, dlg[1:], dlg[:-1], dlg[1:].replace('25',''), dlg[:-1].replace('25',''), dlg.replace('25','')):
        if cand in creD: s=creD[cand]; break
    if s is None or s>=len(en): return None
    e=E(s); j=JA(s)
    return e+('/'+j if JP.search(j) else '')
items=[]
for i in todo:
    c=ctx.get(i,[])
    kinds=sorted(set(x[0] for x in c))
    ent={'id':i,'en':E(i)}
    dl=[x for x in c if x[0] in ('npc_line','pc_reply','journal')]
    if dl:
        x=dl[0]; ent['dlg']=x[1]; ent['state']=x[2]
        sp=speaker(x[1])
        if x[0]=='pc_reply':
            ent['kind']='pc_reply'
            npc=x[3]
            if npc<len(en):
                ent['reply_to_en']=E(npc)[:300]
                if JP.search(JA(npc)): ent['reply_to_ja']=JA(npc)[:300]
            if sp: ent['talking_to']=sp
        else:
            ent['kind']=x[0]
            if sp and x[0]=='npc_line': ent['speaker']=sp
        key=(0,x[1],x[2])
    else:
        ent['kind']=kinds[0] if kinds else 'misc'
        if kinds and 'creature_soundset_line' in kinds:
            cr=[x[1] for x in c if x[0]=='creature_soundset_line'][0]; ent['creature']=cr
        key=(1,ent['kind'],i)
    items.append((key,ent))
items.sort(key=lambda t:t[0])
# pack: keep dlg groups together, ~15000 en chars per batch
batches=[];cur=[];size=0;LIM=15000
for key,ent in items:
    l=len(ent['en'])
    if cur and size+l>LIM:
        batches.append(cur);cur=[];size=0
    cur.append(ent);size+=l
if cur: batches.append(cur)
os.makedirs('batches',exist_ok=True); os.makedirs('out',exist_ok=True)
for k,b in enumerate(batches):
    json.dump(b,open('batches/b%02d.json'%k,'w',encoding='utf-8'),ensure_ascii=False,indent=1)
print(len(todo),len(batches),[len(b) for b in batches])
print(collections.Counter(e['kind'] for _,e in items))
