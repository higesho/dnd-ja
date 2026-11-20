# usage: python -X utf8 validate.py j00 [j01 ...] | all
import json,sys,os,re,glob,collections
from tra import read
JP=re.compile(r'[぀-ヿ一-鿿]')
TOK=re.compile(r'<[A-Z_0-9]+>|%[A-Za-z_0-9#]+%')
jobs={j['job']:j for j in json.load(open('jobs.json',encoding='utf-8'))}
def replies(srcs):
    ids=set()
    for s in srcs:
        if not s.endswith('.d') or not os.path.exists(s): continue
        t=open(s,encoding='cp1252',errors='replace').read()
        t=re.sub(r'~[^~]*~','~~',t)  # drop condition text
        for m in re.finditer(r'(?:\bREPLY\s*@|\+\+\s*@|\+\s*~~\s*\+\s*@)\s*(\w+)',t): ids.add(m.group(1))
    return ids
args=sys.argv[1:]
if args==['all']: args=sorted(jobs)
total=0
for jn in args:
    errs=[]; done=0; want=0
    for f in jobs[jn]['files']:
        en=read(os.path.join(f['mod'],'english',f['rel']))
        keys=f.get('ids') or list(en.keys())
        want+=len(keys)
        out=os.path.join('japanese',f['mod'],f['rel']) if 'ids' not in f else os.path.join('japanese_parts',f['mod'],f['rel']+'.%d'%f['part'])
        if not os.path.exists(out): errs.append('%s: missing file'%out); continue
        try:
            raw=open(out,'rb').read(); raw.decode('utf-8'); ja=read(out)
        except Exception as e: errs.append('%s: %s'%(out,str(e)[:120])); continue
        rp=replies(f['src'])
        for k in keys:
            if k not in ja: errs.append('%s @%s missing'%(f['rel'],k)); continue
            done+=1
            e=en[k][0]; v=ja[k]
            for s,snd in v:
                if collections.Counter(TOK.findall(s))!=collections.Counter(TOK.findall(e[0])):
                    errs.append('%s @%s token mismatch en=%s ja=%s'%(f['rel'],k,TOK.findall(e[0]),TOK.findall(s)))
                if e[1] and snd!=e[1]: errs.append('%s @%s sound ref %r -> %r'%(f['rel'],k,e[1],snd))
                if re.search(r'[a-z]{3,}\s+[a-z]{3,}',e[0]) and not JP.search(s): errs.append('%s @%s no Japanese: %r'%(f['rel'],k,s[:50]))
            if k in rp and len(v)<2: errs.append('%s @%s is a player reply: needs ~male~ ~female~'%(f['rel'],k))
        extra=set(ja)-set(keys)
        if extra: errs.append('%s extra keys %s'%(f['rel'],sorted(extra)[:8]))
    print('%s: %d/%d done, %d issues'%(jn,done,want,len(errs)))
    for x in errs[:40]: print('   ',x)
    total+=len(errs)
print('TOTAL issues',total)
