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
    if re.search(r'[a-z]{2,}', re.sub(r'%[-0-9.]*[lFN]?[A-Za-z]', '', t)):
        errs.append('%s: 英語の小文字が残っている: %s' % (i, t[:40]))
    if 'note' in it and '選択肢（' in it['note']:
        if len(t.strip()) > 25:
            errs.append('%s: 選択肢が長い（%d 字 > 25 字）: %s' % (i, len(t.strip()), t))
    if t != '-' and re.search(r'%[-0-9.]*[a-zA-Z]', it['en']):
        a1 = re.findall(r'%[-0-9.]*[lFN]?[A-Za-z]', it['en']); a2 = re.findall(r'%[-0-9.]*[lFN]?[A-Za-z]', t)
        if sorted(x[-1] for x in a1) != sorted(x[-1] for x in a2):
            errs.append('%s: 書式（%%d・%%s など）が原文と合わない: %s / %s' % (i, it['en'], t))
    for e, j in (en['glossary'].items() if t != '-' else []):
        hit = re.search(r'(?<![A-Za-z])' + re.escape(e) + r'(?![A-Za-z])', it['en']) if e[:1].isupper() else               re.search(r'(?<![a-z])' + re.escape(e.lower()) + r'(?![a-z])', it['en'].lower())
        if hit and j not in t:
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
