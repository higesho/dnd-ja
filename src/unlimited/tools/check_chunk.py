# 訳した塊の確認: python -X utf8 tools/check_chunk.py NN
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
n = int(sys.argv[1])
en = json.load(open(os.path.join(ROOT, 'text', 'chunks', 'c%03d.en.json' % n), encoding='utf-8'))
p = os.path.join(ROOT, 'text', 'chunks', 'c%03d.ja.json' % n)
if not os.path.exists(p):
    print('NG: %s がない' % p); sys.exit(1)
try:
    ja = json.load(open(p, encoding='utf-8'))
except Exception as e:
    print('NG: JSON として読めない:', e); sys.exit(1)
errs = []
ids = [it['id'] for it in en['items']]
for it in en['items']:
    i = it['id']
    if i not in ja:
        errs.append('%s: 訳がない' % i); continue
    t = ja[i]
    if not isinstance(t, str) or t == '':
        errs.append('%s: 空の訳（英語のまま残すなら "="）' % i); continue
    if t == '=':
        continue
    if '\n' in t or '\r' in t:
        errs.append('%s: 改行が入っている' % i)
    if t.count('^') != it['en'].count('^'):
        errs.append('%s: ^ の数が原文と違う（原文 %d 個）' % (i, it['en'].count('^')))
    if '%' in re.sub(r'%[-0-9.*]*[a-zA-Z(%)]', '', t) and it['src'].startswith('EXE') is False:
        errs.append('%s: ％ は「パーセント」と書く' % i)
    if (it['src'].startswith('EXE') or it['src'].startswith('SCR')) and len(t) > max(len(it['en']), 2):
        errs.append('%s: 長すぎる（%d 字 > 英語 %d 字）: %s' % (i, len(t), len(it['en']), t))
    if it['src'].startswith('BTN') and len(t) > max(len(it['en']), 6):
        errs.append('%s: ボタンが長い（%d 字）: %s' % (i, len(t), t))
    if re.search(r'[a-z]{2,}', re.sub(r'%[-0-9.*]*(?:l?[a-zA-Z])', '', t)):
        errs.append('%s: 英語の小文字が残っている: %s' % (i, t[:40]))
    if 'note' in it and '選択肢' in it['note']:
        w = len(t)
        if w > len(it['en']):
            errs.append('%s: 選択肢が長い（%d 字 > 英語 %d 字）: %s' % (i, w, len(it['en']), t))
    for e, j in en['glossary'].items():
        if re.search(r'(?<![a-z])' + re.escape(e.lower()) + r'(?![a-z])', it['en'].lower()) and j not in t:
            # 複数形などのずれは許す。用語の訳が 1 字も出ていないときだけ知らせる
            if not any(ch in t for ch in j[:2]):
                errs.append('%s: 用語 %s → %s が使われていない' % (i, e, j))
for k in ja:
    if k not in ids:
        errs.append('%s: 余分な id' % k)
if errs:
    print('NG')
    for e in errs: print(' ', e)
    sys.exit(1)
print('OK', len(ids))
