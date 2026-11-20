# Verify L_ja_JP.LUA structure/format.  usage: python -X utf8 verify_lua.py [file]
import re, sys
from collections import Counter
from luacmp import load

F = sys.argv[1] if len(sys.argv) > 1 else 'L_ja_JP.LUA'
BS = chr(92)
src = open(F, 'rb').read()
print('BOM', src[:3] == b'\xef\xbb\xbf', 'CR', b'\r' in src, 'size', len(src))
t = src.decode('utf-8')
en = load('orig/L_EN_US.LUA'); ja = load(F)
keys = re.findall(r'^\s*([A-Z0-9_]+)\s*=', t, re.M)
c = Counter(keys)
print('dups', [k for k, n in c.items() if n > 1], 'count', len(keys),
      'missing', [k for k in en if k not in c], 'extra', [k for k in c if k not in en])
ko = re.findall(r'^\s*([A-Z0-9_]+)\s*=', open('orig/L_JA_JP.LUA', encoding='utf-8').read(), re.M)
print('order same as official', ko == keys)

# Lua-level parse of every entry line: KEY = "....", with only valid escapes
lines = t.split('\n')
bad = []
for i, l in enumerate(lines):
    s = l.strip()
    if not s or s in ('uiStrings = {', '}') or s.startswith('--'):
        continue
    m = re.match(r'^[A-Z0-9_]+\s*=\s*"(.*)",?$', s)
    if not m:
        bad.append((i + 1, l)); continue
    body = m.group(1); j = 0
    while j < len(body):
        ch = body[j]
        if ch == BS:
            if j + 1 >= len(body) or body[j + 1] not in ('n', '"', BS):
                bad.append((i + 1, 'bad escape')); break
            j += 2; continue
        if ch == '"':
            bad.append((i + 1, 'unescaped quote')); break
        j += 1
print('bad lines', bad)
print('first line', repr(lines[0]), 'last', repr(lines[-2:]))

J = r'[　-ヿ一-鿿＀-￯]'
tok = lambda s: sorted(re.findall(r'<[A-Z_0-9]+>|%[sd]|\^0x[0-9a-fA-F]+|\^-|\{[^}]*\}', s))
probs = 0
for k in en:
    v = ja[k]
    msgs = []
    if re.search('[​﻿\xa0]', v): msgs.append('ZW/NBSP')
    if re.search('[｡-ﾟ]', v): msgs.append('half-width kana')
    if re.search(J + ' +' + J, v): msgs.append('space between JP chars')
    if ' ' in v and re.search(J, v): msgs.append('ascii space in JP text')
    if tok(en[k]) != tok(v): msgs.append('token mismatch %s %s' % (tok(en[k]), tok(v)))
    nE, nJ = en[k].count(BS + 'n'), v.count(BS + 'n')
    if nE != nJ: msgs.append('newline count EN %d JA %d' % (nE, nJ))
    if msgs:
        probs += 1; print(k, msgs)
print('flagged', probs)
