import sys,glob,os,re
sys.path.insert(0,'../mods'); from tra import read
import textfix
def q(s):
    if '~' not in s: return '~'+s+'~'
    return '~~~~~'+s+'~~~~~'
def process(path):
    t=read(path); out=[]; ch=0
    for k,v in t.items():
        m,ms=v[0]; f,fs=(v[1] if len(v)>1 else v[0])
        nm,nf=textfix.fix(m,f)
        if (nm,nf)!=(m,f if len(v)>1 else m): ch+=1
        if nm==nf and len(v)==1: out.append('@%s = %s%s'%(k,q(nm),' [%s]'%ms if ms else ''))
        else: out.append('@%s = %s%s %s%s'%(k,q(nm),' [%s]'%ms if ms else '',q(nf),' [%s]'%fs if fs else ''))
    if ch:
        open(path,'w',encoding='utf-8',newline='\n').write('\n'.join(out)+'\n')
    return ch,len(t)
tot=0
pats=sys.argv[1:]
for p in pats:
    for f in glob.glob(p,recursive=True):
        if os.path.basename(f).lower() in ('setup.tra','prompts.tra') and 'ja_JP' in f: continue
        try: c,n=process(f); tot+=c
        except ValueError as e: print("skip",f,e); continue
        if c: print(f,c,'/',n)
print('changed',tot)
