import json,glob,re,sys,collections,os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
JP=re.compile(r'[぀-ヿ一-鿿]'); TOK=re.compile(r'<[A-Z_0-9]+>'); PRON=re.compile(r'<PRO_[A-Z]+>|<(SIR|MAN|GIRL|BOY|LADY|LORD)[A-Z]*>')
HW=re.compile(r'[\uff61-\uff9f]')
tot=0
for n in sys.argv[1:]:
    n=n.lstrip('f').zfill(2)
    b=json.load(open('fixbatches/f%s.json'%n,encoding='utf-8')); o={}
    for f in sorted(glob.glob('fixout/f%s*.json'%n)): o.update(json.load(open(f,encoding='utf-8')))
    errs=[]
    for e in b:
        k=str(e['id']); v=o.get(k)
        if v is None: errs.append(k+' missing'); continue
        if v=='KEEP': continue
        vs=[v] if isinstance(v,str) else [v.get('m'),v.get('f')]
        for s in vs:
            if not isinstance(s,str) or not s: errs.append(k+' bad value'); continue
            en=collections.Counter(t for t in TOK.findall(e['en']) if not PRON.fullmatch(t)); ja=collections.Counter(t for t in TOK.findall(s) if not PRON.fullmatch(t))
            if en!=ja: errs.append('%s token mismatch en=%s ja=%s'%(k,dict(en),dict(ja)))
            if set(TOK.findall(s))-set(TOK.findall(e['en'])): errs.append('%s token not in English: %s'%(k,set(TOK.findall(s))-set(TOK.findall(e['en']))))
            if HW.search(s): errs.append(k+' half-width kana')
            if not JP.search(s) and re.search(r'[a-z]{3,}',e['en']) and s.strip()!=e['en'].strip(): errs.append(k+' no Japanese')
    print('f%s: %d/%d, %d issues'%(n,len(set(o)&set(str(e['id']) for e in b)),len(b),len(errs)))
    for x in errs[:30]: print('  ',x)
    tot+=len(errs)
print('TOTAL issues',tot)
