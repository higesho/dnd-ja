# text/chunks/*.ja.json（id -> 訳）と text/fixes.json を text/ja/<分類>.json（原文 -> 訳）にまとめ、検査する
#   python -X utf8 tools/merge.py
import os, json, re, glob, math, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
T = os.path.join(ROOT, 'text')
SPEC = re.compile(r'%[-+ #0-9.]*[sdxXuc]')

def main():
    src = {}
    cats = {}
    for cat in ['exe', 'items', 'inf', 'textdat', 'extra']:
        p = os.path.join(T, 'src', cat + '.json')
        if not os.path.exists(p):
            continue
        for u in json.load(open(p, encoding='utf-8')):
            src[u['id']] = u['en']
            cats[u['id']] = cat
    ja = {}
    for fn in sorted(glob.glob(os.path.join(T, 'chunks', '*.ja.json'))):
        ja.update(json.load(open(fn, encoding='utf-8')))
    fx = os.path.join(T, 'fixes.json')
    if os.path.exists(fx):
        for k, v in json.load(open(fx, encoding='utf-8')).items():
            if not k.startswith('_'):
                ja[k] = v
    out = {}
    problems = 0
    missing = [i for i in src if i not in ja]
    for i, en in src.items():
        if i not in ja:
            continue
        j = ja[i].replace('\n', '\r').rstrip('\r')
        # 検査: 書式指定の数
        ns = len(SPEC.findall(en))
        nj = len(SPEC.findall(j)) + len(re.findall(r'\{\d\}', j))
        if ns != nj:
            print('SPEC', i, repr(en), repr(j)); problems += 1
        if re.search(r'[Ａ-Ｚａ-ｚ０-９]', j):
            print('FW', i, repr(j))
        out.setdefault(cats[i], {})[en] = j
    os.makedirs(os.path.join(T, 'ja'), exist_ok=True)
    for cat, d in out.items():
        json.dump(d, open(os.path.join(T, 'ja', cat + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print('merged', sum(len(d) for d in out.values()), 'missing', len(missing), 'problems', problems)
    if missing:
        print(' missing ids:', missing[:30])

if __name__ == '__main__':
    main()
