import sys,re,collections; sys.path.insert(0,'tools'); import res
d=res.load(); e=res.entries(d); n=res.names(d); s=res.sources(d)
out=open('orig/reslist.txt','w',encoding='utf8')
strs=0
for i,(o,f) in enumerate(e):
    if not o: continue
    at,b=res.get(d,o)
    nm=n.get(i,'?'); src=s.get(nm,'')
    istext = at==0x10 and not src and b.endswith(b'\0') and all(c>=32 or c in (10,13,9) for c in b[:-1])
    if istext: strs+=1
    out.write('%4d %02x %7d %-40s %-30s %s\n'%(i,at,len(b),nm,src[:30],('TXT '+repr(b[:70])) if istext else repr(b[:24])))
print(strs)
