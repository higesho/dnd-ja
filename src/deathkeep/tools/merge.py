# text/chunks/c*.ja.json をまとめて text/ja.json に
import os, json, glob
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ja = {}
for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.json'))):
    ja.update(json.load(open(f, encoding='utf-8')))
json.dump(ja, open(os.path.join(ROOT, 'text', 'ja.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1, sort_keys=True)
print(len(ja), 'translations')
