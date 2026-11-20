# Post-process Japanese strings for the EE engine:
# 1) expand gendered <PRO_*> tokens into male/female text (dialog.tlk / dialogF.tlk)
# 2) half-width spaces in Japanese text break line wrapping (text gets cut) -> full-width
import re
CJK=re.compile(r'[　-ヿ一-鿿＀-￯]')
TOK={ 'HESHE':('彼','彼女'),'HIMHER':('彼','彼女'),'HISHER':('彼の','彼女の'),'SIRMAAM':('旦那','お嬢さん'),
      'LADYLORD':('閣下','閣下'),'MANWOMAN':('男','女'),'GIRLBOY':('坊や','お嬢ちゃん'),'BROTHERSISTER':('兄弟','姉妹'),
      'SONDAUGHTER':('息子','娘'),'MALEFEMALE':('男性','女性')}
RX=re.compile(r'<(?:PRO_)?(%s)>(の)?'%'|'.join(TOK))
def expand(m,f):
    if not RX.search(m) and not RX.search(f): return m,f
    def sub(s,g):
        def r(mt):
            k=mt.group(1); v=TOK[k][g]
            if mt.group(2):  # trailing の
                return v if v.endswith('の') else v+'の'
            return v
        return RX.sub(r,s)
    return sub(m,0),sub(f,1)
def risky(s,lim=30):
    if ' ' not in s: return False
    for tok in s.split(' '):
        if '\n' not in tok: continue
        for p in tok.split('\n'):
            if len(p)>lim and CJK.search(p): return True
    return False
def spaces(s):
    if ' ' not in s or not CJK.search(s): return s
    s=re.sub(r'(?<=[　-ヿ一-鿿＀-￯\n]) +| +(?=[　-ヿ一-鿿＀-￯\n])',lambda m:'　'*len(m.group()),s)
    if risky(s): s=s.replace(' ','　')
    return s
def fix(m,f):
    m,f=expand(m,f); return spaces(m),spaces(f)
