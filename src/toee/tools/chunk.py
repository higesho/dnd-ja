# text/src.json -> text/chunks/<name>.json  ([{"id", "f", "k", "en"}] work units for the translators)
#   python tools/chunk.py names     short names (items, NPCs, spells, feats, places) -> translated first, they make the glossary
#   python tools/chunk.py rest      everything else: mes (UI), gd/rp (generic replies), dlg (by file), help
# Every English string is translated once (first occurrence).
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'text', 'chunks')
NAME_FILES = ('mes/description.mes', 'mes/spell.mes', 'mes/feat.mes', 'mes/map_names.mes', 'mes/item_creation_names.mes',
              'mes/deity.mes', 'mes/skill.mes', 'tpmes/feat.mes', 'mes/gamekeylog.mes', 'mes/stat.mes')


def src():
    return json.load(open(os.path.join(ROOT, 'text', 'src.json'), encoding='utf-8'))


def is_name(u):
    t = u['en']
    return u['f'] in NAME_FILES and len(t) <= 40 and '\n' not in t and not re.search(r'[.!?:]\s*$', t) and len(t.split()) <= 6


def done():
    s = set()
    if os.path.isdir(OUT):
        for fn in os.listdir(OUT):
            s.update(u['en'] for u in json.load(open(os.path.join(OUT, fn), encoding='utf-8')))
    return s


def save(name, units):
    os.makedirs(OUT, exist_ok=True)
    out = [{'id': i + 1, 'f': u['f'], 'k': u['k'], 'en': u['en']} for i, u in enumerate(units)]
    json.dump(out, open(os.path.join(OUT, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)


def split(units, name, words=1800, keep_file=False):
    chunks, cur, w, lastf = [], [], 0, None
    for u in units:
        n = len(u['en'].split())
        # dialogs: do not cut a file in the middle unless it is very long
        if cur and w + n > words and (not keep_file or u['f'] != lastf or w > words * 1.6):
            chunks.append(cur); cur, w = [], 0
        cur.append(u); w += n; lastf = u['f']
    if cur: chunks.append(cur)
    for k, c in enumerate(chunks):
        save('%s_%03d' % (name, k + 1), c)
    print(name, len(chunks), 'chunks', sum(len(u['en'].split()) for u in units), 'words')


def uniq(us, seen):
    out = []
    for u in us:
        if u['en'] in seen: continue
        seen.add(u['en']); out.append(u)
    return out


def names():
    seen = set()
    split(uniq([u for u in src() if is_name(u)], seen), 'names', words=1200)


def rest():
    seen = done()
    S = src()
    cats = [
        ('ui', lambda u: u['k'] == 'mes' and not re.search(r'/(gd_|game_r|pcvoice/)', u['f'])),
        ('gd', lambda u: u['k'] == 'mes'),
        ('dlg', lambda u: u['k'] in ('dlgn', 'dlgp')),
        ('help', lambda u: u['k'] == 'help'),
    ]
    used = set()
    for name, f in cats:
        part = [u for i, u in enumerate(S) if i not in used and f(u)]
        used.update(i for i, u in enumerate(S) if f(u))
        split(uniq(part, seen), name, keep_file=(name == 'dlg'))


if __name__ == '__main__':
    {'names': names, 'rest': rest}[sys.argv[1]]()
