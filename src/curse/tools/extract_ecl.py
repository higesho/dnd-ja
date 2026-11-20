# ECL*.DAX（台本）から 6 ビットで詰めた文字列を抜き出す
import sys, os, re, json
sys.path.insert(0, os.path.dirname(__file__))
from goldbox import dax
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, 'test', 'game', [d for d in os.listdir(os.path.join(ROOT, 'test', 'game')) if os.path.isdir(os.path.join(ROOT, 'test', 'game', d))][0])

def unpack6(data):
    bits = int.from_bytes(data, 'big'); tot = len(data) * 8; n = tot // 6; s = ''
    for i in range(n):
        v = (bits >> (tot - 6 * (i + 1))) & 0x3f
        s += chr(v + 0x40 if v <= 0x1f else v)
    return s

OK = re.compile(r"[A-Z0-9 ,.'!?\-:;\"()/&%+*=#<>\[\]]+")
def strings(blk):
    res = []; p = 0
    while p < len(blk) - 2:
        if blk[p] == 0x80:
            n = blk[p + 1]
            if 1 <= n and p + 2 + n <= len(blk):
                s = unpack6(blk[p + 2:p + 2 + n]).rstrip('@')
                if s and OK.fullmatch(s) and (' ' in s or (len(s) >= 2 and re.search('[AEIOUY]', s))):
                    res.append((p, s)); p += 2 + n; continue
        p += 1
    return res

if __name__ == '__main__':
    out = {}
    for f in sorted(os.listdir(GAME)):
        if not re.fullmatch(r'ECL\d\.DAX', f): continue
        for k, v in sorted(dax(os.path.join(GAME, f)).items()):
            for p, s in strings(v):
                out['%s:%d:%05x' % (f[:4], k, p)] = s
    print(len(out), 'strings', len(set(out.values())), 'unique', sum(len(s) for s in set(out.values())), 'chars')
    json.dump(out, open(os.path.join(ROOT, 'text', 'ecl_strings.json'), 'w'), indent=0)
