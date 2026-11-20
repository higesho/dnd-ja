# usage: python -X utf8 validate.py 012 [013 ...]   (or "all")
import json,glob,re,sys,collections,os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
JP=re.compile(r'[぀-ヿ一-鿿]')
TOK=re.compile(r'<[A-Z_0-9]+>')
def load_out(n):
    d={}
    for f in sorted(glob.glob('out/%s.json'%n)+glob.glob('out/%s_*.json'%n)):
        try: d.update({str(k):v for k,v in json.load(open(f,encoding='utf-8')).items()})
        except Exception as e: print('JSON ERROR',f,e)
    return d
args=sys.argv[1:]
if args==['all']: args=sorted(os.path.basename(f)[:4] for f in glob.glob('batches/*.json'))
tot=0
for n in args:
    pre='e' if n.startswith('e') else 'b'; n=pre+n.lstrip('be').zfill(3)
    b=json.load(open('batches/%s.json'%n,encoding='utf-8')); o=load_out(n); errs=[]
    for e in b:
        k=str(e['id']); v=o.get(k)
        if v is None: errs.append('%s missing'%k); continue
        vs=[v] if isinstance(v,str) else ([v.get('m'),v.get('f')] if isinstance(v,dict) else [None])
        if e['kind']=='pc_reply' and not (isinstance(v,dict) and v.get('m') and v.get('f')): errs.append('%s pc_reply needs {"m","f"}'%k)
        for s in vs:
            if not isinstance(s,str): errs.append('%s bad value'%k); continue
            if collections.Counter(TOK.findall(s))!=collections.Counter(TOK.findall(e['en'])):
                errs.append('%s token mismatch: en=%s ja=%s'%(k,TOK.findall(e['en']),TOK.findall(s)))
            if s.strip()==e['en'].strip(): continue  # left as-is intentionally
            if not JP.search(s) and re.search(r'[a-z]{3,}',e['en']): errs.append('%s no Japanese? %r'%(k,s[:60]))
            if re.search(r'[A-Za-z]{4,} [a-z]{3,} [a-z]{3,}',s) and not re.search(r'[A-Za-z]{4,} [a-z]{3,} [a-z]{3,}',e['en'][:0]+'x'): errs.append('%s English left? %r'%(k,s[:60]))
    extra=set(o)-set(str(e['id']) for e in b)
    if extra: errs.append('extra ids: %s'%sorted(extra)[:10])
    print('%s: %d/%d done, %d issues'%(n,len(set(o)&set(str(e['id']) for e in b)),len(b),len(errs)))
    for x in errs[:40]: print('  ',x)
    tot+=len(errs)
print('TOTAL issues',tot)
