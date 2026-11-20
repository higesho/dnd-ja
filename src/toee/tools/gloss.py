# Per-chunk glossaries: text/gloss/<chunk>.md lists the translated names (units, buildings, items,
# abilities, maps) whose English appears in that chunk.   python tools/gloss.py [chunk ...]
import os, json, re, glob, sys
sys.path.insert(0, os.path.dirname(__file__))
import merge
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


def names():
    tr = merge.load_all()
    out = {}
    for f in glob.glob(os.path.join(ROOT, 'text', 'chunks', 'names_*.json')):
        for u in json.load(open(f, encoding='utf-8')):
            en = u['en'].strip()
            if en in tr or u['en'] in tr:
                if len(en) >= 3 and '|' not in en and ':' not in en and not en.startswith('%'):
                    out[en] = tr.get(u['en'], tr.get(en))
    return out


def main(chunks):
    nm = names()
    keys = sorted(nm, key=len, reverse=True)
    pats = {k: re.compile(r'(?<![A-Za-z])' + re.escape(k) + r'(?![A-Za-z])', re.I) for k in keys}
    os.makedirs(os.path.join(ROOT, 'text', 'gloss'), exist_ok=True)
    for c in chunks:
        src = json.load(open(os.path.join(ROOT, 'text', 'chunks', c + '.json'), encoding='utf-8'))
        text = '\n'.join(u['en'] for u in src)
        hit = [k for k in keys if pats[k].search(text)]
        hit.sort(key=str.lower)
        with open(os.path.join(ROOT, 'text', 'gloss', c + '.md'), 'w', encoding='utf-8') as f:
            f.write('# このチャンクに出てくる名前の訳語（必ずこれを使う）\n\n| 英語 | 訳語 |\n|---|---|\n')
            for k in hit:
                f.write('| %s | %s |\n' % (k, nm[k]))
        print(c, len(hit), 'terms')


if __name__ == '__main__':
    cs = sys.argv[1:] or sorted(os.path.basename(f)[:-5] for f in glob.glob(os.path.join(ROOT, 'text', 'chunks', '*.json'))
                                if not os.path.basename(f).startswith('names_'))
    main(cs)
