# Restyle IWDEE Japanese terminology to the IWD2EE (D&D 3rd edition) style.
# rules: iwd2_rules.tsv (IWDEE, new, EN-regex, flag) ; names: iwd2_map_names.tsv (mode, EN, IWDEE, new)
import re,os
KATA=r'ァ-ヺー'          # katakana letters + long mark (not the middle dot)
def _load_rules():
    R=[]
    for l in open(os.path.join(os.path.dirname(__file__),'iwd2_rules.tsv'),encoding='utf-8'):
        if l.startswith('#') or '\t' not in l: continue
        c=l.rstrip('\r\n').split('\t')+['','']
        a,b,en,flag=c[0],c[1],c[2],c[3]
        R.append((a,b,re.compile(en) if en else None,flag))
    return R
def _load_proper():
    P=set()
    d=os.path.join(os.path.dirname(__file__),'iwd2_diff.tsv')
    for l in open(d,encoding='utf-8'):
        c=l.rstrip('\r\n').split('\t')
        if len(c)>=5 and c[4] in ('official-area','glossary','new') and c[1][:1].isupper(): P.add(c[1])
    for w in ('Kuldahar','Tempus','Arundel','Kresselack','Hrothgar','Everard','Orrick','Conlan','Myrkul','Corellon','Tymora','Shadowdale','Sword Coast','Caer-Dineval','Sahuagin','Solonor','Shevarash','Thelandira','Quinn','Wolfwere'):
        P.add(w)
    for w in ('Book','Gold','Armor','Shield','Ring','Prayer','Player','Shout','Retribution','Seer'): P.discard(w)
    return P
PROPER=_load_proper()
def _load_names():
    p=os.path.join(os.path.dirname(__file__),'iwd2_map_names.tsv')
    T=[];F={}
    if not os.path.exists(p): return T,F
    for l in open(p,encoding='utf-8'):
        if l.startswith('#') or '\t' not in l: continue
        c=l.rstrip('\r\n').split('\t')
        if len(c)<4: continue
        mode,en,a,b=c[:4]
        if not a or not b or a==b: continue
        if mode=='term':
            T.append((a,b,None if en in PROPER else re.compile(r'(?<![A-Za-z])'+re.escape(en)+r'(?![A-Za-z])',re.I)))
        elif mode=='full':
            F.setdefault(a,(b,en))
    return T,F
RULES=_load_rules(); NAMES,FULL=_load_names()
ITEMS=[(a,b,rx,flag or 'rule') for a,b,rx,flag in RULES]+[(a,b,rx,'') for a,b,rx in NAMES]
ITEMS.sort(key=lambda t:-len(t[0]))
SCHOOL_PRE=re.compile(r'[（(／/、,：:\s]$')
def restyle(en,ja):
    if not ja: return ja
    s=ja.strip()
    if s in FULL:
        b,e=FULL[s]
        if en.strip().lower()==e.lower(): return ja.replace(s,b)
    # find non-overlapping matches, longest terms first
    taken=[]; reps=[]
    for a,b,rx,flag in ITEMS:
        if a not in ja: continue
        if rx is not None and not rx.search(en): continue
        for m in re.finditer(re.escape(a),ja):
            st,ed=m.start(),m.end()
            if any(st<y and x<ed for x,y in taken): continue
            # whole-term check for katakana terms: not inside a longer katakana word
            if flag!='loose' and re.match('['+KATA+']',a[0]) and st>0 and re.match('['+KATA+']',ja[st-1]): continue
            if flag=='' and re.match('['+KATA+']',a[-1]) and ed<len(ja) and re.match('['+KATA+']',ja[ed]): continue
            # names: not a part of a longer katakana compound joined with ・ (グレーター・イヴェイジョン)
            if flag in ('','rule') and re.match('['+KATA+']',a[0]) and st>1 and ja[st-1]=='・' and re.match('['+KATA+']',ja[st-2]) and not (flag=='' and re.search(r'(オブ|ザ|オブ・ザ)・$',ja[max(0,st-6):st])): continue
            if flag=='' and re.match('['+KATA+']',a[-1]) and ed+1<len(ja) and ja[ed]=='・' and re.match('['+KATA+']',ja[ed+1]) and not re.search(r'・(オブ|ザ)',ja[ed:ed+4]): continue
            if flag=='schoolx' and re.match('['+KATA+']',ja[ed:ed+1] or 'a'): continue
            if flag=='school' and not SCHOOL_PRE.search(ja[max(0,st-6):st] or ' '): continue
            taken.append((st,ed)); reps.append((st,ed,b))
    if not reps: return ja
    out=[];p=0
    for st,ed,b in sorted(reps):
        out.append(ja[p:st]); out.append(b); p=ed
    out.append(ja[p:])
    return ''.join(out)
