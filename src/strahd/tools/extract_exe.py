# UI strings in the data area of CODE.1 / CODE.2 -> text/en/exe_strings.json  (all candidates, for review)
# Watcom places string literals on 4-byte boundaries; bytes before a literal may be leftover junk.
# room = bytes available for the replacement (up to the next 4-byte boundary after the NUL, minus the NUL).
import re, json, os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GAME = os.path.join(ROOT, '..', 'GAME', 'RLOFT')
REGIONS = {'CODE.1': (0x3e700, 0x3ea60), 'CODE.2': (0x75f28, 0x77500)}
out = []
for fn, (lo, hi) in REGIONS.items():
    d = open(os.path.join(GAME, fn), 'rb').read()
    for m in re.finditer(rb'[\x20-\x7e]{2,}\x00', d[lo:hi]):
        a = lo + m.start(); nul = lo + m.end() - 1
        st = (a + 3) & ~3
        if st >= nul: continue
        s = d[st:nul].decode('latin1')
        if not re.search(r'[A-Za-z]{2,}', s): continue
        room = ((nul + 1 + 3) & ~3) - st - 1
        out.append({'id': len(out), 'file': fn, 'off': st, 'room': room, 'en': s})
json.dump(out, open(os.path.join(ROOT, 'text', 'en', 'exe_strings.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=0)
for x in out:
    print('[%d] %s %x (%d) %s' % (x['id'], x['file'], x['off'], x['room'], x['en']))
