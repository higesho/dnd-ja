# QuickBASIC constant strings in the unpacked START.EXE image: {u16 len, u16 ptr = DS offset of the text, text}
import os, json, struct, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.dirname(__file__))
from unpack import unpack
d = open(os.path.join(ROOT, '..', 'GAME', 'WL', 'START.EXE'), 'rb').read()
img = unpack(d)[0]
DSB = 0x58c40
out = []
o = DSB
end = min(len(img), DSB + 0x10000)
while o < end - 4:
    n, p = struct.unpack_from('<HH', img, o)
    if 1 <= n <= 200 and DSB + p == o + 4 and all(0x20 <= c < 0x7f for c in img[o + 4:o + 4 + n]):
        out.append({'id': len(out), 'desc': o - DSB, 'off': o + 4, 'room': n, 'en': img[o + 4:o + 4 + n].decode('latin1')})
        o += 4 + n
        continue
    o += 1
json.dump(out, open(os.path.join(ROOT, 'text', 'en', 'exe_all.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=0)
print(len(out))
with open(os.path.join(ROOT, 'text', 'en', 'exe_all.txt'), 'w', encoding='utf8') as f:
    for x in out: f.write('[%d] %s\n' % (x['id'], x['en']))
