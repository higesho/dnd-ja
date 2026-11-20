import re,collections,pickle,json
from tlk import read_tlk
G="L:/SteamLibrary/steamapps/common/Baldur's Gate Enhanced Edition/lang/"
_,EN=read_tlk(G+'en_US/dialog.tlk'); _,JA=read_tlk(G+'ja_JP/dialog.tlk'); _,JF=read_tlk(G+'ja_JP/dialogF.tlk')
D=lambda e:e[4].decode('utf-8','replace')
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
TOK=re.compile(r'<[A-Z_0-9]+>')
HW=re.compile(r'[\uff61-\uff9f]')
ctx,dlgs,creD=pickle.load(open('index.pkl','rb'))
def used(i): return i in ctx
def trivial(s):
    t=re.sub(r'<[^>]*>','',s).strip()
    return (not re.search(r'[A-Za-z]{2,}',t)) or t in('<NO TEXT>','placeholder') or 'DO NOT USE' in s or t.startswith('***') or re.fullmatch(r"\[[^\]]*\]",t)
n=min(len(EN),len(JA)); print('entries en',len(EN),'ja',len(JA),'jaF',len(JF))
R=collections.defaultdict(list)
for i in range(n):
    e=D(EN[i]); j=D(JA[i]); f=D(JF[i]) if i<len(JF) else ''
    if not e.strip() or trivial(e): continue
    if not JP.search(j): R['untranslated'].append(i); continue
    if collections.Counter(TOK.findall(e))!=collections.Counter(TOK.findall(j)): R['token_mismatch'].append(i)
    if TOK.search(j) and not (JA[i][0]&4): R['token_flag_missing'].append(i)
    if re.search(r'[A-Za-z]{3,}[ ,.]+[a-z]{3,}[ ,.]+[a-z]{3,}',re.sub(r'<[^>]*>|\[[^\]]*\]','',j)): R['english_left'].append(i)
    if re.match(r'^\[[^\]]+\]',j) and not re.match(r'^\[[^\]]+\]',e): R['bracket_tag'].append(i)
    if len(e)>200 and len(j)<len(e)*0.15: R['truncated'].append(i)
    if HW.search(j): R['halfwidth_kana'].append(i)
    ne=sorted(re.findall(r'\d+',e)); nj=sorted(re.findall(r'\d+',j.translate(str.maketrans('０１２３４５６７８９','0123456789'))))
    if ('STATISTICS' in e or 'Weight' in e) and ne!=nj: R['stat_number_mismatch'].append(i)
    if f and not JP.search(f): R['female_untranslated'].append(i)
    elif f and collections.Counter(TOK.findall(e))!=collections.Counter(TOK.findall(f)) and f!=j: R['female_token_mismatch'].append(i)
for k,v in R.items():
    u=sum(1 for i in v if used(i))
    print('%-22s %5d  (used in game data: %d)'%(k,len(v),u))
pickle.dump(dict(R),open('bgee_audit.pkl','wb'))
import random; random.seed(2)
for k in R:
    print('==',k)
    for i in random.sample(R[k],min(4,len(R[k]))):
        print('  #%d EN: %s\n       JA: %s'%(i,D(EN[i])[:130].replace('\n','/'),D(JA[i])[:130].replace('\n','/')))
