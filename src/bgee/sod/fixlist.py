import json,glob,collections,pickle,re
from tlk import read_tlk
G="L:/SteamLibrary/steamapps/common/Baldur's Gate Enhanced Edition/lang/"
_,EN=read_tlk(G+'en_US/dialog.tlk'); _,JA=read_tlk(G+'ja_JP/dialog.tlk')
D_raw=lambda k:JA[k][4].decode('utf-8','replace')
D=lambda e:e[4].decode('utf-8','replace').replace('\r','').replace('\n',' / ')
sys_path=None
import sys; sys.path.insert(0,'../mods'); from tra import read
final=read('../release/bgee_jpfix/bgee_jpfix/tra/japanese/strings.tra')
fb={}
for f in glob.glob('fixbatches/f*.json'):
    for e in json.load(open(f,encoding='utf-8')): fb[e['id']]=e
fixed=set()
for f in glob.glob('fixout/f*.json'):
    for k,v in json.load(open(f,encoding='utf-8')).items():
        if v!='KEEP': fixed.add(int(k))
JP=re.compile(r'[\u3040-\u30ff\u4e00-\u9fff]')
cat=collections.defaultdict(list)
for k in sorted(int(x) for x in final):
    old=D(JA[k]); new=final[str(k)][0][0].replace('\n',' / ')
    if k in fixed and k in fb:
        iss=fb[k]['issues']
        c=('表示の不具合（文字の差し込み記号）' if 'wrong_token' in iss else '開発用ラベルの残り' if 'debug_label_in_text' in iss else
           '呪文説明の数値・内容' if 'spell_stats_differ' in iss else 'アイテム説明の数値・内容' if any(x.startswith('item_stats') for x in iss) else
           '未訳' if 'untranslated' in iss else '別の文の訳・大きな省略・誤訳')
    elif not JP.search(old): c='未訳'
    elif k in fixed: c='別の文の訳・大きな省略・誤訳'
    else:
        import textfix
        c='文章が途中で欠ける不具合（半角スペース）' if textfix.risky(D_raw(k)) else '半角カナの全角化'
    cat[c].append((k,D(EN[k]),old,new))
order=['表示の不具合（文字の差し込み記号）','開発用ラベルの残り','別の文の訳・大きな省略・誤訳','アイテム説明の数値・内容','呪文説明の数値・内容','未訳','文章が途中で欠ける不具合（半角スペース）','半角カナの全角化']
L=['BGEE 公式日本語訳 修正一覧（v1.0）','='*40,'','各行：文章番号 / 英語原文 / 修正前 / 修正後','']
for c in order:
    v=cat.get(c,[]); L.append('■ %s（%d件）'%(c,len(v))); L.append('')
    for k,e,o,n in v:
        L.append('#%d'%k); L.append('  英語　：'+e); L.append('  修正前：'+(o if o.strip() else '（空）')); L.append('  修正後：'+n); L.append('')
open('../pkg/bgee_jpfix/修正一覧.txt','w',encoding='utf-8-sig',newline='\r\n').write('\n'.join(L))
print({c:len(cat[c]) for c in order}, sum(len(v) for v in cat.values()))
