import sys,re
sys.path.insert(0,'../mods'); from tra import read
from tlk import read_tlk
G='L:/EET_test/BGEE/lang/ja_JP/'
_,M=read_tlk(G+'dialog.tlk'); _,F=read_tlk(G+'dialogf.tlk')
_,EN=read_tlk('L:/EET_test/BGEE/lang/en_US/dialog.tlk')
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
D=lambda e:e[4].decode('utf-8','replace')
bad=0;n=0;snd=0;flag=0
for mod in ('bgee_jpfix','sod_ja'):
    t=read('../release/%s/%s/tra/japanese/strings.tra'%(mod,mod))
    for k,v in t.items():
        i=int(k); n+=1
        m=v[0][0]; f=v[1][0] if len(v)>1 else m
        if D(M[i])!=m or D(F[i])!=f: bad+=1
        s=v[0][1]
        if s and M[i][1].rstrip(b'\0').decode('latin1')!=s: snd+=1
        if '<' in m and not (M[i][0]&4): flag+=1
print('checked',n,'mismatch',bad,'sound lost',snd,'token flag missing',flag)
left=[i for i in range(len(EN)) if re.search(r'[a-z]{3,} [a-z]{3,}',D(EN[i])) and not JP.search(D(M[i]))]
print('English-only entries remaining in ja tlk:',len(left))
for i in left[:15]: print(' ',i,D(M[i])[:80].replace('\n','/'))
