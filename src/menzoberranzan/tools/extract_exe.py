# UI strings in the data object of CODE.1 / CODE.2 -> text/en/exe_strings.json
# Watcom places string literals on 4-byte boundaries; bytes before a literal may be leftover junk.
# room = bytes available for the replacement (up to the next 4-byte boundary after the NUL, minus the NUL).
import re, json, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import le
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GAME = os.path.join(ROOT, '..', 'GAME', 'MENZO')
SKIP = re.compile(r'\.(dat|map|lbm|cfg|ini|exe|ad|res|xmi|dfa|bnk|dll|com)\b|%0\d?[Xx]|\\|^[A-Z]:|slack|handle|SpacePlayer|LZW|GrabSprite|'
                  r'resource|Resource|Page Stack|DOS|memory|Memory|malloc|chunk|Indices|error \[|Error \[|^[a-z_]+$|DOS4G|WATCOM|Watcom', re.I)
out = []
for fn in ('CODE.1', 'CODE.2'):
    L = le.LE(os.path.join(GAME, fn))
    d = L.d; o2 = L.objs[1]
    lo = L.pageoff[o2['pmi']][2]; hi = L.pageoff[o2['pmi'] + o2['pmn'] - 1][2] + 0x1000
    n0 = len(out)
    for m in re.finditer(rb'[\x20-\x7e]{2,}\x00', d[lo:hi]):
        a = lo + m.start(); nul = lo + m.end() - 1
        st = (a + 3) & ~3
        if st >= nul - 1: continue
        s = d[st:nul].decode('latin1')
        if not re.search(r'[A-Za-z]{2,}', s) or SKIP.search(s) or len(s) < 2: continue
        room = ((nul + 1 + 3) & ~3) - st - 1
        out.append({'id': len(out), 'file': fn, 'off': st, 'room': room, 'en': s})
    print(fn, len(out) - n0)
json.dump(out, open(os.path.join(ROOT, 'text', 'en', 'exe_strings.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=0)
