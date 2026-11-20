import sys,os,shutil
sys.path.insert(0,'../mods'); from tra import read
from tlk import read_tlk,write_tlk
R='L:/SteamLibrary/steamapps/common/_eet_jpwork'
def apply(M,F,tra):
    for k,v in tra.items():
        i=int(k)
        if i>=len(M): continue
        (m,ms)=v[0]; (f,fs)=v[1] if len(v)>1 else v[0]
        for T,s,snd in ((M,m,ms),(F,f,fs)):
            fl,so,vo,pi,_=T[i]
            if snd: so=snd.encode('latin1').ljust(8,b'\0')
            T[i]=(fl|1|4,so,vo,pi,s.encode('utf-8'))
fix=read(R+'/release/bgee_jpfix/bgee_jpfix/tra/japanese/strings.tra')
sod=read(R+'/release/sod_ja/sod_ja/tra/japanese/strings.tra')
G="L:/SteamLibrary/steamapps/common/Baldur's Gate Enhanced Edition/lang/ja_JP/"
lang,M=read_tlk(G+'dialog.tlk'); _,F=read_tlk(G+'dialogF.tlk'); M=list(M);F=list(F)
apply(M,F,fix)
d=R+'/pkg/bgee_jpfix/差し替え版（SoDなし）'
shutil.rmtree(d,ignore_errors=True); os.makedirs(d+'/lang/ja_JP'); os.makedirs(d+'/override')
write_tlk(d+'/lang/ja_JP/dialog.tlk',lang,M); write_tlk(d+'/lang/ja_JP/dialogF.tlk',lang,F)
shutil.copy('fontsrc/MEIRYO.ttf',d+'/override/MEIRYO.ttf'); shutil.copy('fontsrc/OFL.txt',d+'/OFL.txt')
lang,M=read_tlk('orig/bgee/dialog.tlk'); _,F=read_tlk('orig/bgee/dialogf.tlk'); M=list(M);F=list(F)
apply(M,F,fix); apply(M,F,sod)
d=R+'/pkg/sod_ja/差し替え版（DLC Merger 導入済み環境用）'
shutil.rmtree(d,ignore_errors=True); os.makedirs(d+'/lang/ja_JP'); os.makedirs(d+'/override')
write_tlk(d+'/lang/ja_JP/dialog.tlk',lang,M); write_tlk(d+'/lang/ja_JP/dialogF.tlk',lang,F)
shutil.copy('fontsrc/MEIRYO.ttf',d+'/override/MEIRYO.ttf'); shutil.copy('fontsrc/OFL.txt',d+'/OFL.txt')
print('ok')
