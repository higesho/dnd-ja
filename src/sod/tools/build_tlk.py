# Build Japanese TLKs: BGEE+SoD (build/bgee) and EET (build/eet)
import json,glob,re,os,collections
from tlk import read_tlk,write_tlk
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
N=lambda s:re.sub(r'\s+',' ',s).strip()
D=lambda e:e[4].decode('utf-8','replace')
def load(prefix):
    t={}
    for f in sorted(glob.glob('out/%s[0-9][0-9][0-9]*.json'%prefix)):
        for k,v in json.load(open(f,encoding='utf-8')).items():
            m,fm=(v,v) if isinstance(v,str) else (v['m'],v['f'])
            t[int(k)]=(m,fm)
    return t
tb=load('b'); te=load('e')
for k,v in json.load(open('dups.json')).items():
    if int(k) in tb:
        for d in v: tb[d]=tb[int(k)]
print('translations: sod',len(tb),'eet',len(te))
def setent(T,i,s):
    fl,snd,vol,pit,_=T[i]; b=s.encode('utf-8')
    fl|=1
    if '<' in s: fl|=4
    T[i]=(fl,snd,vol,pit,b)
# --- BGEE+SoD
lang,M=read_tlk('orig/bgee/dialog.tlk'); _,F=read_tlk('orig/bgee/dialogf.tlk')
M=list(M);F=list(F)
_,BEN=read_tlk('L:/EET_work/BGEE/lang/en_US/dialog.tlk')
for i,(m,f) in tb.items(): setent(M,i,m); setent(F,i,f)
os.makedirs('build/bgee',exist_ok=True)
write_tlk('build/bgee/dialog.tlk',lang,M); write_tlk('build/bgee/dialogF.tlk',lang,F)
# text -> (m,f) from BGEE (SoD translations + official ja)
tmap={}
for i,e in enumerate(BEN):
    t=N(D(e))
    if not t or t in tmap: continue
    if i in tb: tmap[t]=tb[i]
    elif JP.search(D(M[i])): tmap[t]=(D(M[i]),D(F[i]) if JP.search(D(F[i])) else D(M[i]))
# --- EET
lang,EM=read_tlk('orig/eet/dialog.tlk'); _,EF=read_tlk('orig/eet/dialogF.tlk'); EM=list(EM); EF=list(EF)
c=collections.Counter()
for i in range(len(EM)):
    s=D(EM[i])
    if JP.search(s) or not re.search(r'[A-Za-z]{2,}',s): continue
    if i in te: v=te[i]; c['eet']+=1
    elif N(s) in tmap: v=tmap[N(s)]; c['text']+=1
    else: c['left']+=1; continue
    setent(EM,i,v[0])
    if i<len(EF): setent(EF,i,v[1])
os.makedirs('build/eet',exist_ok=True)
write_tlk('build/eet/dialog.tlk',lang,EM); write_tlk('build/eet/dialogF.tlk',lang,EF)
print('EET',c)
