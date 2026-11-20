# 翻訳単位（text/units.json）を塊（text/chunks/cNNN.en.json）に分ける
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LIMIT = 2600

def main():
    gl = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    gl_l = [(e, j, re.compile(r'(?<![a-z])' + re.escape(e.lower()) + r'(?![a-z])')) for e, j in gl.items()]
    us = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    chunks, cur, size = [], [], 0
    for src, en in us:
        kind = src[:3]
        if cur and (size + len(en) > LIMIT or kind != cur[-1][0][:3]):
            chunks.append(cur); cur, size = [], 0
        cur.append((src, en)); size += len(en)
    if cur: chunks.append(cur)
    d = os.path.join(ROOT, 'text', 'chunks')
    os.makedirs(d, exist_ok=True)
    for k, ch in enumerate(chunks, 1):
        text = ' '.join(en for _, en in ch).lower()
        g = {e: j for e, j, rx in gl_l if rx.search(text)}
        items = []
        for i, (src, en) in enumerate(ch):
            it = {'id': '%03d-%03d' % (k, i), 'src': src, 'en': en}
            notes = []
            if src.startswith('EXE'):
                if re.search(r'%[-0-9.]*[a-zA-Z]', en): notes.append('書式。%d・%s などはそのまま同じ順番で残す（数や名前が入る）')
                if en.startswith(' ') or en.endswith(' '): notes.append('断片: 前後に名前・数・別の文がつながる')
            if notes: it['note'] = '。'.join(notes)
            items.append(it)
        json.dump({'chunk': k, 'glossary': g, 'items': items},
                  open(os.path.join(d, 'c%03d.en.json' % k), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(len(chunks), 'chunks')

if __name__ == '__main__':
    main()
