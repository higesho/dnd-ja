import os, json
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
u = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
os.makedirs(os.path.join(ROOT, 'text', 'chunks'), exist_ok=True)
items = [{'id': x['id'], 'en': x['en'], 'max': max(1, int(len(x['en']) / 1.7))} for x in u]
n = 3; per = (len(items) + n - 1) // n
for k in range(n):
    json.dump(items[k * per:(k + 1) * per], open(os.path.join(ROOT, 'text', 'chunks', 'c%d.json' % (k + 1)), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(len(items))
