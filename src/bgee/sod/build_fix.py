import re,collections,pickle,json,glob,os
from tlk import read_tlk
G="L:/SteamLibrary/steamapps/common/Baldur's Gate Enhanced Edition/lang/"
_,EN=read_tlk(G+'en_US/dialog.tlk'); _,JA=read_tlk(G+'ja_JP/dialog.tlk'); _,JF=read_tlk(G+'ja_JP/dialogF.tlk')
D=lambda e:e[4].decode('utf-8','replace')
ctx,_,_=pickle.load(open('index.pkl','rb'))
A=pickle.load(open('bgee_audit.pkl','rb')); S=pickle.load(open('bgee_stats.pkl','rb')); M=pickle.load(open('bgee_audit4.pkl','rb'))
batched=set()
for f in glob.glob('batches/b*.json'):
    for e in json.load(open(f,encoding='utf-8')): batched.add(e['id'])
TOK=re.compile(r'<[A-Za-z_0-9]+>')
issues=collections.defaultdict(list)
for i in A['token_mismatch']:
    e=collections.Counter(TOK.findall(D(EN[i]))); j=collections.Counter(TOK.findall(D(JA[i])))
    if j-e: issues[i].append('wrong_token')
for i in A.get('bracket_tag',[]): issues[i].append('debug_label_in_text')
for i in A.get('truncated',[]): issues[i].append('truncated_or_wrong')
for i in S.get('item',[]): issues[i].append('item_stats_differ')
for i in S.get('item_nostat',[]): issues[i].append('item_stats_missing')
for i in S.get('spell',[]): issues[i].append('spell_stats_differ')
for i in M['misplaced']: issues[i].append('maybe_wrong_line')
for i in M['lenratio']: issues[i].append('length_outlier')
for i in A.get('untranslated',[]):
    if i not in batched: issues[i].append('untranslated')
items=[]
for i in sorted(issues):
    if i in batched: continue
    k=[x[0] for x in ctx.get(i,[])]
    ent={'id':i,'issues':issues[i],'kind':k[0] if k else 'misc','en':D(EN[i]),'ja':D(JA[i])}
    if D(JF[i])!=D(JA[i]): ent['ja_female']=D(JF[i])
    items.append(ent)
os.makedirs('fixbatches',exist_ok=True); os.makedirs('fixout',exist_ok=True)
n=5; per=(len(items)+n-1)//n
for k in range(n):
    json.dump(items[k*per:(k+1)*per],open('fixbatches/f%02d.json'%k,'w',encoding='utf-8'),ensure_ascii=False,indent=1)
print(len(items),collections.Counter(x for e in items for x in e['issues']))
