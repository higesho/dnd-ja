# STRONG.DAT のヘルプと住民の訴え（目次 0x1173d から 34 項目）を抜き出す -> text/dat_units.json
import os, json, struct
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
d = open(os.path.join(ROOT, '..', 'GAME', 'STRONG', 'STRONG.DAT'), 'rb').read()
dirofs = struct.unpack_from('<I', d, 0)[0]
out = []
for k in range(34):
    fld = dirofs + 0x1173d + 21 * k
    off, size = struct.unpack_from('<II', d, fld)
    t = d[off:off + size]
    out.append({'id': 'h%02d' % k, 'field': fld, 'en': t.rstrip(b'\x1a').decode('latin-1').replace('\r\n', '\n'),
                'kind': 'help page' if k < 8 else 'advisor complaint', 'eof': t.endswith(b'\x1a')})
json.dump(out, open(os.path.join(ROOT, 'text', 'dat_units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(len(out), sum(len(x['en']) for x in out))
print([x['en'][-3:] for x in out[:10]])
