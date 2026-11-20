import re,sys,collections
from tlk import read_tlk
_,ja=read_tlk('orig/ja_JP/dialog.tlk')
CJK=re.compile(r'[　-ヿ一-鿿＀-￯]')
def risky(s,lim=30):
    """space-delimited token that contains a newline and has a long CJK run after/before the newline"""
    if ' ' not in s: return []
    out=[]
    for tok in s.split(' '):
        if '\n' not in tok: continue
        parts=tok.split('\n')
        # a part (other than the first) that is long and CJK
        for p in parts[1:]+parts[:1]:
            if len(p)>lim and CJK.search(p): out.append(tok[:60]); break
    return out
if __name__=='__main__':
    n=0;ex=[]
    for i,e in enumerate(ja):
        s=e[4].decode('utf-8','replace')
        r=risky(s)
        if r:
            n+=1
            if len(ex)<40: ex.append((i,r[0]))
    print(n)
    for x in ex: print(x)
