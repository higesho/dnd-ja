# Build Japanese-enabled copies of DSotSC / NTotSC into L:/EET_work/mods_jp/<mod>
import zipfile,os,shutil,glob,json,re
from tra import read
OUT='L:/EET_work/mods_jp'
MODS=[('dsotsc','L:/EET_work/dl/dsotsc/dsotsc.zip','DSotSC'),('ntotsc','L:/EET_work/dl/ntotsc/ntotsc.zip','ntotsc')]
jobs=json.load(open('jobs.json',encoding='utf-8'))
def q(s):
    return '~~~~~%s~~~~~'%s if '~' in s else '~%s~'%s
for name,zp,top in MODS:
    dst=os.path.join(OUT,name); shutil.rmtree(dst,ignore_errors=True); os.makedirs(dst)
    zipfile.ZipFile(zp).extractall(dst)
    lang=os.path.join(dst,top,'language'); en_dir=os.path.join(lang,'english'); ja_dir=os.path.join(lang,'japanese')
    for f in glob.glob(os.path.join(en_dir,'**','*.tra'),recursive=True):
        rel=os.path.relpath(f,en_dir).replace(os.sep,'/'); out=os.path.join(ja_dir,rel); os.makedirs(os.path.dirname(out),exist_ok=True)
        whole=os.path.join('japanese',name,rel)
        if rel=='worldmap.tra' or not any(ff['mod']==name and ff['rel']==rel and 'ids' in ff for j in jobs for ff in j['files']):
            src=whole if os.path.exists(whole) else None
            if src: shutil.copyfile(src,out)
            else: print('MISSING',name,rel); shutil.copyfile(f,out)
            continue
        en=read(f); ja={}
        for p in sorted(glob.glob(os.path.join('japanese_parts',name,rel+'.*'))): ja.update(read(p))
        lines=[]
        for k,v in en.items():
            vv=ja.get(k,v)
            if k not in ja: print('missing key',name,rel,k)
            lines.append('@%s = '%k+' '.join(q(s)+(' [%s]'%snd if snd else '') for s,snd in vv))
        open(out,'w',encoding='utf-8',newline='\n').write('\n'.join(lines)+'\n')
    # tp2: add japanese language + no-convert
    tp=os.path.join(dst,top,top+'.tp2'); t=open(tp,encoding='cp1252').read()
    m=re.search(r'LANGUAGE\s*\n\s*~English~\s*\n\s*~english~\s*\n((?:\s*~[^~]*~\s*\n)+)',t)
    block=m.group(0)
    jblock=block.replace('~English~','~Japanese~').replace('~english~','~japanese~').replace('/english/','/japanese/')
    t=t.replace(block,block+jblock,1)
    t=re.sub(r'//\s*(ACTION_DEFINE_ARRAY charsetsNoConvertArray BEGIN)[^\n]*END',r'\1 japanese END',t,count=1)
    assert 'charsetsNoConvertArray BEGIN japanese END' in t, 'noconvert patch failed'
    t=re.sub(r'(ACTION_DEFINE_ASSOCIATIVE_ARRAY charsetsTable BEGIN\s*
)',r'				"japanese" => "UTF-8"
',t,count=1)
    assert '"japanese" => "UTF-8"' in t
    open(tp,'w',encoding='cp1252',newline='').write(t)
    print('built',name)
