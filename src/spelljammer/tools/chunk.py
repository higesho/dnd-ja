# 翻訳単位（text/units.json）を塊（text/chunks/cNNN.en.json）に分ける
import os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LIMIT = {'btn': 2500, 'ui': 2600, 'name': 2500, 'list': 2500, 'long': 3200}


def gl_rx(e):
    return re.compile(r'(?<![A-Za-z])' + re.escape(e) + r'(?![A-Za-z])', re.I)


def note_of(u):
    e, notes = u['en'], []
    w = u['where'][0]
    if u['kind'] == 'ui':
        if e.startswith(' ') or e.endswith(' ') or e.startswith((', ', ',')) or len(e.split()) == 1 and len(e) <= 4:
            notes.append('断片かもしれない（前後の項目とつなげて表示されることがある）')
        if e.endswith(':'):
            notes.append('欄の見出し')
    if w.startswith('RUM:'): notes.append('酒場で聞く噂（くだけた話し言葉）')
    if w.startswith('MISS:'): notes.append('任務の説明（依頼人の話）')
    if w.startswith('STORY:'): notes.append('港の酒場で受ける運送・旅客の仕事の説明（~PLANETA~ などはゲームが置き換える）')
    if w.startswith('NAME:'): notes.append('名前（%s）' % w.split(':')[1])
    if w.startswith('LIST:'): notes.append('呪文の名前の一覧（%s）' % w.split(':')[1])
    return '。'.join(notes)


def main():
    gl = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    gl_l = [(e, j, gl_rx(e)) for e, j in gl.items()]
    us = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    chunks, cur, size = [], [], 0
    for u in us:
        if cur and (cur[-1]['kind'] != u['kind'] or size + len(u['en']) > LIMIT[u['kind']]):
            chunks.append(cur); cur, size = [], 0
        cur.append(u); size += len(u['en'])
    if cur: chunks.append(cur)
    d = os.path.join(ROOT, 'text', 'chunks')
    os.makedirs(d, exist_ok=True)
    for k, ch in enumerate(chunks, 1):
        text = ' '.join(u['en'] for u in ch)
        g = {e: j for e, j, rx in gl_l if rx.search(text)}
        items = []
        for u in ch:
            it = {'id': u['id'], 'kind': u['kind'], 'src': u['where'][0], 'en': u['en']}
            if u['max'] is not None: it['max'] = u['max']
            n = note_of(u)
            if n: it['note'] = n
            items.append(it)
        json.dump({'chunk': k, 'glossary': g, 'items': items},
                  open(os.path.join(d, 'c%03d.en.json' % k), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(len(chunks), 'chunks', [(k + 1, c[0]['kind'], len(c), sum(len(u['en']) for u in c)) for k, c in enumerate(chunks)])


if __name__ == '__main__':
    main()
