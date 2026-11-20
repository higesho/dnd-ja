# 訳の塊を検査する:  python -X utf8 tools/check_chunk.py 03
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
k = sys.argv[1].zfill(2)
en = json.load(open(os.path.join(ROOT, 'text', 'chunks', 'c%s.en.json' % k), encoding='utf-8'))
ja = json.load(open(os.path.join(ROOT, 'text', 'chunks', 'c%s.ja.json' % k), encoding='utf-8'))
bad = 0
for it in en['items']:
    i, e = it['id'], it['en']
    if i not in ja:
        print('なし', i, repr(e)); bad += 1; continue
    j = ja[i]
    if not isinstance(j, str):
        print('文字列でない', i); bad += 1; continue
    if j == '' or j == '-':
        continue
    if '\n' in j or '\r' in j:
        print('改行がある', i, repr(j)); bad += 1
    if re.search('[a-z]', j):
        print('小文字の英字がある（ゲームでは大文字になる。英字を残すのは名前や略号だけ）', i, repr(j)); bad += 1
    if it.get('kind') == 'menu':
        if len(j) > 5:
            print('メニュー項目が長い（2〜4 字に）', i, repr(e), '->', repr(j)); bad += 1
        if re.match('[A-Z]', j):
            print('メニュー項目にキーの英字を書かない', i, repr(j)); bad += 1
    if len(j) > 255:
        print('長すぎる（255 字まで）', i); bad += 1
extra = set(ja) - {it['id'] for it in en['items']}
if extra:
    print('余分な id', sorted(extra)[:10]); bad += 1
print('OK' if not bad else '問題 %d 件' % bad)
