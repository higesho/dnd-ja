import re,pickle,unicodedata,sys
from tlk import read_tlk
_,en=read_tlk('../lang/en_US/dialog.tlk'); _,ja=read_tlk('orig/ja_JP/dialog.tlk')
ctx,_,_=pickle.load(open('index.pkl','rb'))
refs=pickle.load(open('refs2.pkl','rb'))
unt=set(pickle.load(open('unt.pkl','rb')))
CJK=re.compile(r'[぀-ヿ一-鿿]')
def nums(s):
    s=unicodedata.normalize('NFKC',s)
    return set(re.findall(r'\d+',s))
# referenced-ness of translated entries: need to know if ctx covers them (index covers all), refs2 only covers untranslated -> rebuild quickly for all
out=[]
for i,(e,j) in enumerate(zip(en,ja)):
    es=e[4].decode('utf-8','replace'); js=j[4].decode('utf-8','replace')
    if i in unt or not CJK.search(js) or len(es)<25: continue
    r=len(js)/max(1,len(es))
    ne=nums(es); nj=nums(js)
    flag=[]
    if r<0.18: flag.append('short%.2f'%r)
    if r>1.6: flag.append('long%.2f'%r)
    miss=ne-nj
    if miss and len(ne)>=2 and len(miss)>=2: flag.append('nums-%s'%','.join(sorted(miss)[:4]))
    tag=re.match(r'\[[A-Z0-9 _]+\]',es)
    if tag and not js.startswith(tag.group(0)[:3]): flag.append('tag')
    if flag: out.append((i,flag,es,js))
pickle.dump(out,open('mismatch.pkl','wb'))
print(len(out))
isref=lambda i:i in ctx
print('referenced',sum(1 for o in out if isref(o[0])))
for i,f,es,js in out[:int(sys.argv[1]) if len(sys.argv)>1 else 30]:
    if isref(i): print(i,f,'|',es[:70].replace('\n','/'),'|',js[:45].replace('\n','/'))
