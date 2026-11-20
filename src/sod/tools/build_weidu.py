# Build WeiDU mods: bgee_jpfix (BGEE base strrefs <34000) and sod_ja (SoD strrefs >=34000)
import json,glob,re,os,shutil,unicodedata,sys
from tlk import read_tlk
from alltr import load
import final, textfix
R='L:/SteamLibrary/steamapps/common/_eet_jpwork/release'
D=lambda e:e[4].decode('utf-8','replace')
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
_,JM=read_tlk('orig/bgee/dialog.tlk'); _,JFm=read_tlk('orig/bgee/dialogf.tlk')
tb=final.get('b')
for k,v in json.load(open('dups.json')).items():
    if int(k) in tb:
        for d in v: tb[d]=tb[int(k)]
fx={}
for f in glob.glob('fixout/f*.json'):
    for k,v in json.load(open(f,encoding='utf-8')).items():
        if v!='KEEP':
            m,fe=(v,v) if isinstance(v,str) else (v['m'],v['f'])
            if f.endswith('f01.json') and k in ('10042','10045','11852'):
                m=m.replace('矢弾','飛び道具').replace('刺突','貫通'); fe=fe.replace('矢弾','飛び道具').replace('刺突','貫通')
            fx[int(k)]=(m,fe)
HW=re.compile(r'[\uff61-\uff9f]+')
def fullw(s): return HW.sub(lambda m:unicodedata.normalize('NFKC',m.group()),s)
base={}; sod={}
for i,v in tb.items(): (base if i<34000 else sod)[i]=v
for i,v in fx.items(): base[i]=v
# half-width kana in remaining official strings
for i in range(len(JM)):
    if i in base or i in sod: continue
    m=D(JM[i]); f=D(JFm[i])
    if HW.search(m) or HW.search(f):
        (base if i<34000 else sod)[i]=(fullw(m),fullw(f))
# official strings whose half-width spaces make the text get cut off
for i in range(len(JM)):
    if i in base or i in sod: continue
    m=D(JM[i]); f=D(JFm[i])
    if textfix.risky(m) or textfix.risky(f):
        (base if i<34000 else sod)[i]=(m,f)
for d in (base,sod):
    for i in d: d[i]=textfix.fix(fullw(d[i][0]),fullw(d[i][1]))
def q(s):
    if '~' not in s: return '~'+s+'~'
    assert '~~~~~' not in s; return '~~~~~'+s+'~~~~~'
def snd(i):
    s=JM[i][1].rstrip(b'\0').decode('latin1'); return ' [%s]'%s if s else ''
def sndf(i):
    s=JFm[i][1].rstrip(b'\0').decode('latin1'); return ' [%s]'%s if s else ''
def write_mod(name,strings,title,minstr,readme):
    md=R+'/'+name+'/'+name; os.makedirs(md+'/tra/japanese',exist_ok=True); os.makedirs(md+'/lib',exist_ok=True)
    with open(md+'/tra/japanese/strings.tra','w',encoding='utf-8',newline='\n') as f:
        for i in sorted(strings):
            m,fe=strings[i]
            if m==fe and snd(i)==sndf(i): f.write('@%d = %s%s\n'%(i,q(m),snd(i)))
            else: f.write('@%d = %s%s %s%s\n'%(i,q(m),snd(i),q(fe),sndf(i)))
    with open(md+'/lib/strings.tpa','w',encoding='utf-8',newline='\n') as f:
        for i in sorted(strings): f.write('STRING_SET %d @%d\n'%(i,i))
    with open(md+'/tra/japanese/setup.tra','w',encoding='utf-8',newline='\n') as f:
        f.write('@1 = ~%s~\n'%title)
        f.write('@2 = ~This mod is for Baldur\'s Gate: Enhanced Edition only.~\n')
        f.write('@3 = ~The game language must be Japanese (ja_JP). Set it in the game options first.~\n')
        f.write('@4 = ~Siege of Dragonspear is installed as an unmerged DLC. Install DLC Merger first.~\n')
        f.write('@5 = ~Siege of Dragonspear (merged with DLC Merger) is required.~\n')
    tp2='''BACKUP ~weidu_external/backup/%(n)s~
AUTHOR ~higesho~
VERSION ~v1.0~
README ~%(n)s/readme-%(n)s.txt~

LANGUAGE ~Japanese~ ~japanese~ ~%(n)s/tra/japanese/setup.tra~ ~%(n)s/tra/japanese/strings.tra~

BEGIN @1
REQUIRE_PREDICATE GAME_IS ~bgee~ @2
OUTER_SPRINT game_lang ~~
ACTION_IF FILE_EXISTS ~weidu.conf~ BEGIN
  COPY - ~weidu.conf~ ~weidu.conf~
    REPLACE_EVALUATE ~lang_dir[ %%TAB%%]*=[ %%TAB%%]*\([a-zA-Z_]+\)~ BEGIN SPRINT game_lang ~%%MATCH1%%~ END ~%%MATCH0%%~
END
ACTION_IF NOT (~%%game_lang%%~ STR_EQ ~ja_JP~) BEGIN FAIL @3 END
ACTION_IF (FILE_EXISTS ~dlc/sod-dlc.zip~ OR FILE_EXISTS ~sod-dlc.zip~) BEGIN FAIL @4 END
%(extra)s
INCLUDE ~%(n)s/lib/strings.tpa~
%(font)s
'''%{'n':name,'font':('COPY ~%s/fonts/MEIRYO.ttf~ ~override~'%name) if name=='bgee_jpfix' else '','extra':('ACTION_IF NEXT_STRREF < %d BEGIN FAIL @5 END'%minstr) if minstr else ''}
    open(R+'/'+name+'/'+name+'/'+name+'.tp2','w',encoding='utf-8',newline='\n').write(tp2)
    open(R+'/'+name+'/'+name+'/readme-'+name+'.txt','w',encoding='utf-8-sig',newline='\r\n').write(readme)
    shutil.copy('L:/EET_work/EET/setup-EET.exe',R+'/'+name+'/setup-'+name+'.exe')
    if name=='bgee_jpfix':
        os.makedirs(md+'/fonts',exist_ok=True); shutil.copy('fontsrc/MEIRYO.ttf',md+'/fonts/MEIRYO.ttf'); shutil.copy('fontsrc/OFL.txt',md+'/fonts/OFL.txt')
    print(name,len(strings))
if __name__=='__main__':
    write_mod('bgee_jpfix',base,'BGEE 公式日本語訳の修正（未訳・誤訳・データ不一致の修正）',0,open('readme_bgee_jpfix.txt',encoding='utf-8').read() if os.path.exists('readme_bgee_jpfix.txt') else '')
    write_mod('sod_ja',sod,'Siege of Dragonspear 日本語化',71000,open('readme_sod_ja.txt',encoding='utf-8').read() if os.path.exists('readme_sod_ja.txt') else '')
