# Build fixed ja_JP dialog.tlk / dialogf.tlk for IWD:EE v2.7
# base = official ja_JP TLKs, + out/*.json translations, + mechanical fixes (fixes.py)
import json,glob,os,re,sys,collections
from tlk import read_tlk,write_tlk
from fixes import fix_all
from restyle import restyle
OUT=sys.argv[1] if len(sys.argv)>1 else 'build'
VER=os.environ.get('JPVER','1.1')
_,en=read_tlk('../lang/en_US/dialog.tlk')
lang,ja=read_tlk('orig/ja_JP/dialog.tlk')
_,jf=read_tlk('orig/ja_JP/dialogf.tlk')
assert len(en)==len(ja)==len(jf)
M=[list(x) for x in ja]; F=[list(x) for x in jf]
tr={}
for f in sorted(glob.glob('out/b*.json')):
    for k,v in json.load(open(f,encoding='utf-8')).items(): tr[int(k)]=v
canon=[l.rstrip('\r\n').split('\t') for l in open('canon.tsv',encoding='utf-8') if '\t' in l]
def canonize(t):
    for a,b in canon: t=t.replace(a,b)
    return t
n=0
for i,v in tr.items():
    if isinstance(v,dict): m,f=v['m'],v['f']
    else: m=f=v
    M[i][4]=canonize(m).encode('utf-8'); F[i][4]=canonize(f).encode('utf-8'); n+=1
# journal / save timestamp formats (ASCII spaces broke the journal wrap)
FIX={15980:'<GAMEDAY>日目　<HOUR>時（<YEAR>年<DAYANDMONTH>）',
     15981:'<MONTHNAME><DAY>日', 37823:'<MONTHNAME><DAY>日',
     16041:'<GAMEDAY>日目　<HOUR>時'}
cred=('Icewind Dale: Enhanced Edition 日本語版\n'
      '公式日本語訳（Beamdog / コミュニティ翻訳）\n\n'
      '日本語化修正MOD v%s\n'
      '・未訳部分（ハート・オブ・ウィンター、トライアル・オブ・ザ・ルアマスター、EE追加の会話・ヘルプ・音声セット等）の補完翻訳\n'
      '・古いままだったアイテム／呪文説明の更新、取り違えられていた訳の修正\n'
      '・フォント（字間の不具合）と文章が欠ける改行不具合の修正\n'
      ) % VER
note=('この日本語化修正MODはゲームバージョン v2.7.3.0 で動作確認しています。\n\n'
      '導入：lang フォルダと override フォルダを\nゲームのインストールフォルダに上書きコピーし、\n'
      'オプション → 言語 で Japanese を選んで\nゲームを再起動してください。\n\n'
      '元に戻すには：Steam で「ファイルの整合性を確認」を実行し、\n'
      'override フォルダの MEIRYO.ttf と L_ja_JP.LUA を削除してください。')
FIX[40822]=cred; FIX[40823]=note
for T in (M,F):
    for i,s in FIX.items(): T[i][4]=s.encode('utf-8')
    for i,e in enumerate(T):
        s=e[4].decode('utf-8','replace')
        t=fix_all(restyle(en[i][4].decode('utf-8','replace'),fix_all(s)) if i not in FIX else s)
        if t!=s: e[4]=t.encode('utf-8')
        if e[4]: e[0]|=1
        if re.search(rb'<[A-Z_0-9]+>',e[4]): e[0]|=4   # token-substitution flag
os.makedirs(OUT,exist_ok=True)
write_tlk(OUT+'/dialog.tlk',lang,[tuple(e) for e in M])
write_tlk(OUT+'/dialogf.tlk',lang,[tuple(e) for e in F])
print('applied',n,'translations ->',OUT,len(M))
