# Compare IWD2EE (jp_work) terms with IWDEE terms; list EN terms whose Japanese differs.
import re,collections,os
I2=r"L:/Games/GOG/TocaRaceDriver3/Icewind Dale 2/jp_work/translation/"
def load_tsv(p):
    d={}
    for l in open(p,encoding='utf-8-sig'):
        if l.startswith('#') or '\t' not in l: continue
        a,b=l.rstrip('\r\n').split('\t',1); d.setdefault(a.strip(),b.strip())
    return d
iwd2={}
iwd2.update(load_tsv(I2+'名前集.tsv')); iwd2.update(load_tsv(I2+'用語集.tsv'))
ee={}
src={}
for l in open('glossary_auto.txt',encoding='utf-8'):
    m=re.match(r'(.+?) → (.+?)  \[(\w+)\]$',l.rstrip())
    if m:
        ee[m.group(1)]=m.group(2).split(' / ')[0]; src[m.group(1)]='official-'+m.group(3)
for l in open('glossary_new.txt',encoding='utf-8'):
    if l.startswith('#') or '→' not in l: continue
    a,b=[x.strip() for x in l.split('→',1)]
    for aa,bb in zip(a.split(' / '),b.split(' / ')):
        aa=re.sub(r'\s*\(.*?\)','',aa).strip(); bb=re.sub(r'（.*?）','',bb).strip()
        if aa and aa not in ee: ee[aa]=bb; src[aa]='new'
for l in open('glossary.md',encoding='utf-8'):
    for m in re.finditer(r"([A-Z][A-Za-z' \-ûâ]+?) → ([^\s/（(]+)",l):
        a,b=m.group(1).strip(),m.group(2).strip()
        if a not in ee: ee[a]=b; src[a]='glossary'
    for m in re.finditer(r"([A-Z][A-Za-z\-]+(?: [A-Z][A-Za-z\-]+)*) ([\u30a0-\u30ff・ー]+)",l):
        a,b=m.group(1),m.group(2)
        if a not in ee: ee[a]=b; src[a]='glossary'
ja=[l.rstrip('\n').split('\t',1)[1] for l in open(I2+'ja.tsv',encoding='utf-8') if '\t' in l]
alltext='\n'.join(ja)
low={k.lower():k for k in ee}
rows=[]
for k,v in iwd2.items():
    kk=low.get(k.lower())
    if not kk: continue
    e=ee[kk]
    if e==v or not v or not e: continue
    n=alltext.count(v)
    rows.append((n,k,v,e,src[kk]))
rows.sort(reverse=True)
with open('iwd2_diff.tsv','w',encoding='utf-8') as f:
    for r in rows: f.write('%d\t%s\t%s\t%s\t%s\n'%r)
print(len(rows))
for r in rows[:80]: print('%5d  %-28s %-18s → %-18s %s'%r)
