# text/src.json -> text/chunks/<name>.json  (work units for the translators)
#   python tools/chunk.py names      short names (units, buildings, items, abilities, maps) -> glossary first
#   python tools/chunk.py content    everything else except unit barks (needs text/glossary.md)
#   python tools/chunk.py barks      unit acknowledgement lines / sound captions
# Each unique English string is translated once.
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'text', 'chunks')

NAME_GROUPS = {'Units', 'Buildings', 'Objects', 'BattleGear', 'Maps', 'Upgrades'}
SYSTEM = {'Interface', 'Hotkeys', 'Keys', 'Errors', 'Upgrades', 'Maps'}
INFO = {'Units', 'Buildings', 'Objects', 'BattleGear'}
STORY = {'Missions', 'Narrator'}
SKIP = {'Credits'}


def src():
    return json.load(open(os.path.join(ROOT, 'text', 'src.json'), encoding='utf-8'))


def keep_en(e):
    s = e['en']
    if e['g'] in SKIP: return True
    if re.match(r'^(Spanish|French|German|Italian|Korean|Japanese|Chinese|Russian|Polish|Czech|English)\b.*:', s): return True
    if re.fullmatch(r'[\W\d_]*', s): return True          # punctuation / numbers only
    return False


def is_name(e):
    s = e['en'].strip()
    return e['g'] in NAME_GROUPS and len(s) <= 48 and not re.search(r'[.!?]\s*$', s) and '\n' not in s and s.count(' ') <= 7


def is_bark(e):
    g = e['g']
    if g.startswith('SFX_'): return True
    if g in SYSTEM | INFO | STORY | SKIP: return False
    return e['id'] < 1000      # speaker groups: ids below 1000 are selection / order acknowledgements


def unique(entries):
    seen, out = {}, []
    for e in entries:
        if e['en'] in seen:
            seen[e['en']]['groups'].add(e['g']); continue
        u = {'en': e['en'], 'groups': {e['g']}, 'g': e['g']}
        seen[e['en']] = u
        out.append(u)
    return out


def save(name, units):
    os.makedirs(OUT, exist_ok=True)
    for i, u in enumerate(units):
        u['id'] = i + 1
    units = [{'id': u['id'], 'g': u['g'], 'en': u['en']} for u in units]
    json.dump(units, open(os.path.join(OUT, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(name, len(units), 'units', sum(len(u['en'].split()) for u in units), 'words')


def split_words(units, name, words=1800):
    chunks, cur, w = [], [], 0
    for u in units:
        n = len(u['en'].split())
        if cur and w + n > words:
            chunks.append(cur); cur, w = [], 0
        cur.append(u); w += n
    if cur: chunks.append(cur)
    for k, c in enumerate(chunks):
        save('%s_%02d' % (name, k + 1), c)


def already_names():
    p = os.path.join(ROOT, 'text', 'chunks')
    s = set()
    if os.path.isdir(p):
        for fn in os.listdir(p):
            if fn.startswith('names_'):
                s.update(u['en'] for u in json.load(open(os.path.join(p, fn), encoding='utf-8')))
    return s


def names():
    S = [e for e in src() if not keep_en(e) and is_name(e)]
    us = unique(S)
    order = ['Units', 'Buildings', 'Maps', 'Upgrades', 'Objects', 'BattleGear']
    us.sort(key=lambda u: order.index(u['g']))
    split_words(us, 'names', words=900)


def content():
    done = already_names()
    S = [e for e in src() if not keep_en(e) and not is_bark(e) and e['en'] not in done]
    cats = [('ui', lambda e: e['g'] in SYSTEM), ('info', lambda e: e['g'] in INFO),
            ('mission', lambda e: e['g'] == 'Missions'), ('narr', lambda e: e['g'] == 'Narrator'),
            ('dialog', lambda e: True)]
    used = set()
    for name, f in cats:
        part = [e for e in S if id(e) not in used and f(e)]
        used.update(id(e) for e in part)
        split_words(unique(part), name)


def barks():
    done = already_names()
    S = [e for e in src() if not keep_en(e) and is_bark(e) and e['en'] not in done]
    split_words(unique(S), 'bark', words=1500)


if __name__ == '__main__':
    {'names': names, 'content': content, 'barks': barks}[sys.argv[1]]()
