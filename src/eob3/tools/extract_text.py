# Extract all "S:" string resources of EYE.RES -> text/en/strings.json
import sys, os, json
sys.path.insert(0, os.path.dirname(__file__))
import res
d = res.load(); e = res.entries(d); n = res.names(d)
out = []
for i, (o, f) in enumerate(e):
    if not o: continue
    at, b = res.get(d, o)
    if b.startswith(b'S:') and b.endswith(b'\0'):
        out.append({'id': i, 'name': n.get(i, ''), 'en': b[2:-1].decode('latin1')})
root = os.path.join(os.path.dirname(__file__), '..')
json.dump(out, open(os.path.join(root, 'text', 'en', 'strings.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=1)
print(len(out))
