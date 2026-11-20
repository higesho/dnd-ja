# 訳した塊の確認: python -X utf8 tools/check_chunk.py NNN
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
    i = it['id']; e = it['en']
    if i not in ja:
        errs.append('%s: 訳がない' % i); continue
    t = ja[i]
    if not isinstance(t, str) or t == '':
        errs.append('%s: 空の訳（英語のまま残すなら "="）' % i); continue
    if t == '=': continue
    if '\n' in t or '\r' in t: errs.append('%s: 改行が入っている' % i)
    if t != '-':
        t2 = re.sub(r'###[A-Z_]+|Xbox|Dolby Pro Logic|Pro Logic|D&D|Dungeons & Dragons', '', t)
        if re.search(r'[A-Za-z]{2,}', t2): errs.append('%s: 英字が残っている: %s' % (i, t[:40]))
        if sorted(re.findall(r'###[A-Z_]+', t)) != sorted(re.findall(r'###[A-Z_]+', e)): errs.append('%s: ### の語が原文と違う' % i)
    if 'note' in it and '短く' in it['note'] and len(t) > max(len(e), 6) and t != '-':
        errs.append('%s: 長い（%d 字。英語は %d 字）: %s' % (i, len(t), len(e), t))
    for g, j in (en['glossary'].items() if t != '-' else []):
        hit = re.search(r'(?<![A-Za-z])' + re.escape(g) + r'(?![A-Za-z])', e) if g[:1].isupper() else \
              re.search(r'(?<![a-z])' + re.escape(g.lower()) + r'(?![a-z])', e.lower())
        if hit and j not in t and not any(ch in t for ch in j[:2]):
            errs.append('%s: 用語 %s → %s が使われていない' % (i, g, j))
for k in ja:
    if k not in ids: errs.append('%s: 余分な id' % k)
if errs:
    print('NG')
    for x in errs: print(' ', x)
    sys.exit(1)
print('OK', len(ids))
