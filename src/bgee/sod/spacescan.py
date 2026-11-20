import re,sys
sys.path.insert(0,'../mods'); from tra import read
from tlk import read_tlk
CJK=re.compile(r'[　-ヿ一-鿿＀-￯]')
def risky(s,lim=30):
    if ' ' not in s: return False
    for tok in s.split(' '):
        if '\n' not in tok: continue
        for p in tok.split('\n'):
            if len(p)>lim and CJK.search(p): return True
    return False
G="L:/SteamLibrary/steamapps/common/Baldur's Gate Enhanced Edition/lang/ja_JP/"
_,J=read_tlk(G+'dialog.tlk')
off=[i for i in range(len(J)) if risky(J[i][4].decode('utf-8','replace'))]
print('official BGEE risky:',len(off))
for mod in ('bgee_jpfix','sod_ja'):
    t=read('../release/%s/%s/tra/japanese/strings.tra'%(mod,mod))
    r=[k for k,v in t.items() if any(risky(x[0]) for x in v)]
    sp=[k for k,v in t.items() if any(' ' in x[0] and CJK.search(x[0]) for x in v)]
    print(mod,'risky',len(r),'any space+CJK',len(sp))
    for k in r[:3]: print('  ',k,repr(t[k][0][0][:120]))
# pronoun token values
_,E=read_tlk("L:/SteamLibrary/steamapps/common/Baldur's Gate Enhanced Edition/lang/en_US/dialog.tlk")
for w in ['he','she','him','her','his','hers','sir','madam','lord','lady','man','woman','boy','girl','brother','sister']:
    hits=[i for i in range(len(E)) if E[i][4].decode('latin1').strip()==w]
    print(w,[(i,J[i][4].decode('utf-8','replace')) for i in hits[:3]])
