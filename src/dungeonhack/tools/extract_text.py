# Extract all "S:" string resources of HACK.RES / OPEN.RES -> text/en/strings.json
# ids: HACK.RES index, OPEN.RES index + 10000
import sys, os, json
sys.path.insert(0, os.path.dirname(__file__))
import res
out = []
for fn, base in (('HACK.RES', 0), ('OPEN.RES', 10000)):
    d = res.load(fn); e = res.entries(d); n = res.names(d)
    for i, (o, f) in enumerate(e):
        if not o: continue
        at, b = res.get(d, o)
        if b.startswith(b'S:') and b.endswith(b'\0'):
            s = b[2:-1].decode('latin1')
            out.append({'id': base + i, 'name': n.get(i, ''), 'en': s})
root = os.path.join(os.path.dirname(__file__), '..')
json.dump(out, open(os.path.join(root, 'text', 'en', 'strings.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=1)
print(len(out), sum(len(x['en']) for x in out))
