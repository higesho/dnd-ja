import sys,re,collections; sys.path.insert(0,'tools'); import res
d=res.load(); e=res.entries(d)
cnt=collections.Counter(); sz=collections.Counter()
for i,(o,f) in enumerate(e):
  if not o: continue
  at,bb=res.get(d,o); cnt[at]+=1; sz[at]+=len(bb)
print(cnt, sz)
n=0;tot=0
for i,(o,f) in enumerate(e):
  if not o: continue
  at,bb=res.get(d,o)
  if at==0x10:
    n+=1; tot+=len(bb)
    if n<30 or any(c<32 and c!=0 for c in bb) or bb.count(0)>1: print(i,repr(bb))
print(n,tot)
