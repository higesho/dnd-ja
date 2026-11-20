import json,glob,re,collections,sys
canon=[l.rstrip('\r\n').split('\t') for l in open('canon.tsv',encoding='utf-8') if '\t' in l]
def canonize(t):
    for a,b in canon: t=t.replace(a,b)
    return t
en={}
for f in glob.glob('batches/b*.json'):
    for e in json.load(open(f,encoding='utf-8')): en[str(e['id'])]=e['en']
tr={}
for f in glob.glob('out/b*.json'):
    for k,v in json.load(open(f,encoding='utf-8')).items():
        tr[k]=canonize(v if isinstance(v,str) else v['m']+'\n'+v['f'])
gl=[]
for l in open('glossary_new.txt',encoding='utf-8'):
    if l.startswith('#') or '→' not in l: continue
    a,b=l.split('→',1); a=a.strip(); b=b.strip()
    if '/' in a or '(' in a or '/' in b or '（' in b: continue
    if len(a)<4 or not re.search(r'[゠-ヿ一-鿿]',b): continue
    gl.append((a,b))
rep=[]
for a,b in gl:
    rx=re.compile(r'\b'+re.escape(a)+r'\b')
    hits=[k for k,s in en.items() if rx.search(s) and k in tr]
    if len(hits)<2: continue
    miss=[k for k in hits if b.split(' +')[0] not in tr[k]]
    if miss: rep.append((len(miss),len(hits),a,b,miss[:4]))
rep.sort(reverse=True)
for r in rep[:int(sys.argv[1]) if len(sys.argv)>1 else 60]:
    print('%d/%d %s → %s  e.g. %s'%(r[0],r[1],r[2],r[3],','.join(r[4])))
