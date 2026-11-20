# usage: python -X utf8 validate.py NN [NN ...]   (or "all")
import json,glob,re,sys,collections
JP=re.compile(r'[぀-ヿ一-鿿]')
TOK=re.compile(r'<[A-Z_0-9]+>')
def load_out(n):
    d={}
    for f in sorted(glob.glob('out/b%s*.json'%n)):
        try: d.update({str(k):v for k,v in json.load(open(f,encoding='utf-8')).items()})
        except Exception as e: print('JSON ERROR',f,e)
    return d
args=sys.argv[1:]
if args==['all']: args=sorted(f[9:11] for f in glob.glob('batches/b*.json'))
tot=0
for n in args:
    b=json.load(open('batches/b%s.json'%n,encoding='utf-8')); o=load_out(n); errs=[]
    for e in b:
        k=str(e['id']); v=o.get(k)
        if v is None: errs.append('%s missing'%k); continue
        vs=[v] if isinstance(v,str) else ([v.get('m'),v.get('f')] if isinstance(v,dict) else [None])
        if e['kind']=='pc_reply' and not (isinstance(v,dict) and v.get('m') and v.get('f')): errs.append('%s pc_reply needs {"m","f"}'%k)
        for s in vs:
            if not isinstance(s,str): errs.append('%s bad value'%k); continue
            OPT={'<PRO_HESHE>','<PRO_HISHER>','<PRO_HIMHER>','<PRO_GIRLBOY>','<PRO_MANWOMAN>','<HESHE>','<HISHER>','<HIMHER>','<GIRLBOY>','<MANWOMAN>'}
            ce=collections.Counter(t for t in TOK.findall(e['en']) if t not in OPT); cj=collections.Counter(t for t in TOK.findall(s) if t not in OPT)
            if any(t in OPT for t in TOK.findall(s)): errs.append('%s uses a pronoun token (render as 彼女は/彼を in Japanese) - rewrite without it'%k)
            if ce!=cj:
                errs.append('%s token mismatch: en=%s ja=%s'%(k,TOK.findall(e['en']),TOK.findall(s)))
            if not JP.search(s) and re.search(r'[a-z]{3,}',e['en']) and not re.search(r'NO TRANS|do not translate',e['en'],re.I): errs.append('%s no Japanese? %r'%(k,s[:60]))
    extra=set(o)-set(str(e['id']) for e in b)
    if extra: errs.append('extra ids: %s'%sorted(extra)[:10])
    print('b%s: %d/%d done, %d issues'%(n,len(set(o)&set(str(e['id']) for e in b)),len(b),len(errs)))
    for x in errs[:40]: print('  ',x)
    tot+=len(errs)
print('TOTAL issues',tot)
