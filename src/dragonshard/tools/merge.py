# Merge translated chunks -> build/tr.json {english: japanese}
# priority: text/fixes.json (exact English) > rules below > text/ja/*.json
import os, json, re, glob, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


REPL = [('スリ＝クリーン', 'スリークリーン'), ('スリ・クリーン', 'スリークリーン'), ('スリクリーン', 'スリークリーン'),
        ('カラテッシュ', 'カラテシュ'), ('戦争フェルドレイク', 'ウォー・フェルドレイク'), ('ファイアドレイク', 'ファイア・ドレイク'),
        ('バアテズ', 'バーテズ'), ('キングズ・スクリーン', '王の画面'), ('"sentinel"', '「番人」'), ('「sentinel」', '「番人」')]


def rules(en, ja):
    for a, b in REPL:
        ja = ja.replace(a, b)
    if 'Energy' in en and 'Mana' not in en:
        ja = ja.replace('エナジー', 'エネルギー').replace('マナ', 'エネルギー')
    ja = ja.replace('不具化', '弱体')
    return ja


def keep_english():
    """strings that only appear in the Keys group (key names) stay English"""
    src = json.load(open(os.path.join(ROOT, 'text', 'src.json'), encoding='utf-8'))
    g = {}
    for e in src: g.setdefault(e['en'], set()).add(e['g'])
    return {k for k, v in g.items() if v == {'Keys'}}


def load_all():
    tr = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', '*.json'))):
        name = os.path.basename(f)[:-5]
        p = os.path.join(ROOT, 'text', 'ja', name + '.json')
        if not os.path.exists(p):
            continue
        src = json.load(open(f, encoding='utf-8'))
        ja = {u['id']: u['ja'] for u in json.load(open(p, encoding='utf-8')) if isinstance(u, dict) and 'id' in u}
        for u in src:
            if u['id'] in ja and isinstance(ja[u['id']], str) and ja[u['id']].strip():
                tr[u['en']] = rules(u['en'], ja[u['id']])
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(fx):
        for k, v in json.load(open(fx, encoding='utf-8')).items():
            tr[k] = v
    return tr


if __name__ == '__main__':
    tr = load_all()
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    json.dump(tr, open(os.path.join(ROOT, 'build', 'tr.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(tr), 'translations')
