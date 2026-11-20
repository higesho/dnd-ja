# translated chunks -> build/tr.json {english: japanese}; text/fixes.json (exact English) wins
import os, json, glob, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_chunk import toks


def safe(en, j):
    # an entry that would break the game's format strings / markup stays English
    if toks(en) != toks(j): return False
    return all(en.count(c) == j.count(c) for c in '~|\n') and not any(c in j for c in '{}\t')


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


def load_all():
    tr = {}
    bad = []
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', '*.json'))):
        name = os.path.basename(f)[:-5]
        p = os.path.join(ROOT, 'text', 'ja', name + '.json')
        if not os.path.exists(p): continue
        try:
            ja = {u['id']: u['ja'] for u in json.load(open(p, encoding='utf-8')) if isinstance(u, dict) and 'id' in u}
        except Exception:
            continue
        for u in json.load(open(f, encoding='utf-8')):
            j = ja.get(u['id'])
            if isinstance(j, str) and j.strip() and safe(u['en'], j): tr[u['en']] = j
            elif isinstance(j, str) and j.strip(): bad.append((name, u['id']))
    # spelling variants of names the translators wrote differently -> one form
    rp = os.path.join(ROOT, 'text', 'replace.json')
    if os.path.exists(rp):
        reps = json.load(open(rp, encoding='utf-8'))
        for k in tr:
            for a, b in reps.items():
                if a in tr[k]: tr[k] = tr[k].replace(a, b)
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(fx): tr.update(json.load(open(fx, encoding='utf-8')))
    if bad: print('skipped (unsafe):', len(bad), bad[:10])
    return tr


if __name__ == '__main__':
    tr = load_all()
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    json.dump(tr, open(os.path.join(ROOT, 'build', 'tr.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(tr), 'translations')
