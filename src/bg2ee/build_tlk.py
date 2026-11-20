# Build 2.6-compatible ja_JP dialog.tlk / dialogF.tlk
# base = old patch TLK (2.3) + new entries from en_US, then apply out/*.json translations
import json,glob,os,re,sys
from tlk import read_tlk,write_tlk
G="L:/SteamLibrary/steamapps/common/Baldur's Gate II Enhanced Edition/"
OUT=sys.argv[1] if len(sys.argv)>1 else 'build'
_,en=read_tlk(G+'lang/en_US/dialog.tlk')
_,ja=read_tlk(G+'BG2EE日本語化パッチ_2.3.67.3_0.91/lang/ja_JP/dialog.tlk')
_,jf=read_tlk(G+'BG2EE日本語化パッチ_2.3.67.3_0.91/lang/ja_JP/dialogF.tlk')
M=[];F=[]
for i,(fl,snd,vol,pit,s) in enumerate(en):
    hdr=(fl,snd,vol,pit)
    if i<len(ja):
        M.append(list(hdr)+[ja[i][4]]); F.append(list(hdr)+[jf[i][4]])
    else:
        M.append(list(hdr)+[s]); F.append(list(hdr)+[s])
tr={}
for f in sorted(glob.glob('out/b*.json')):
    for k,v in json.load(open(f,encoding='utf-8')).items(): tr[int(k)]=v
n=0
for i,v in tr.items():
    if isinstance(v,dict): m,f=v['m'],v['f']
    else: m=f=v
    M[i][4]=m.encode('utf-8'); F[i][4]=f.encode('utf-8'); n+=1
cred=ja[102450][4].decode('utf-8')+'\n\nv2.7対応・未訳部分（ブラック・ピットII、EE追加会話など）の補完翻訳\nClaude（Anthropic）'
note='この日本語化はゲームバージョン2.7（v2.7.3.0で動作確認）に対応しています。\n\n導入：langフォルダとoverrideフォルダをゲームのインストールフォルダにコピーし、オプション > 言語 から日本語を選択してゲームを再起動してください。'
for T in (M,F):
    T[103592][4]=cred.encode('utf-8'); T[103598][4]=note.encode('utf-8')
    for e in T:
        e[0]=(e[0]|1) if e[4] else e[0]
        if b'<' in e[4]: e[0]|=4   # token-substitution flag
os.makedirs(OUT,exist_ok=True)
write_tlk(OUT+'/dialog.tlk',0,[tuple(e) for e in M])
write_tlk(OUT+'/dialogF.tlk',0,[tuple(e) for e in F])
print('applied',n,'entries ->',OUT,len(M))
