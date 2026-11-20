# Archives\Text.H2O -> work/DND_Text.ltx -> text/src.json  [{"g": group, "id": id, "en": text}]
# U+FB00 (the game's line break code) is written as "\n" for the translators.
import sys, os, json
sys.path.insert(0, os.path.dirname(__file__))
import h2o, ltx

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))
SKIP = {'MSI', 'Install', 'Autoplay'}   # installer / autorun strings, never shown by the game


def load_ltx():
    out, names, dn = h2o.extract(os.path.join(GAME, 'Archives', 'Text.H2O'), None)
    return out[0][1]


def main():
    raw = load_ltx()
    open(os.path.join(ROOT, 'work', 'DND_Text.ltx'), 'wb').write(raw)
    hdr, groups = ltx.parse(raw)
    src = []
    for gf, name, ents in groups:
        if name in SKIP:
            continue
        for eid, fl, s in ents:
            if not s or not s.strip():
                continue
            src.append({'g': name, 'id': eid, 'en': s.replace('ﬀ', '\n')})
    os.makedirs(os.path.join(ROOT, 'text'), exist_ok=True)
    json.dump(src, open(os.path.join(ROOT, 'text', 'src.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(src), 'strings', len({e['en'] for e in src}), 'unique', sum(len(e['en']) for e in src), 'chars')


if __name__ == '__main__':
    main()
