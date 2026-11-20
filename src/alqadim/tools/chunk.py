# 翻訳単位（text/units.json）を塊（text/chunks/cNNN.en.json）に分ける。会話（GPL）はスクリプトの切れ目でなるべく区切る
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LIMIT = 3200

def gl_rx(e):
    return re.compile(r'(?<![A-Za-z])' + re.escape(e) + r'(?![A-Za-z])') if e[:1].isupper() else \
           re.compile(r'(?<![a-z])' + re.escape(e.lower()) + r'(?![a-z])')

def main():
    gl = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    gl_l = [(e, j, gl_rx(e)) for e, j in gl.items()]
    us = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    chunks, cur, size = [], [], 0
    for src, en in us:
        script = src.split(':')[0]
        prev = cur[-1][0].split(':')[0] if cur else None
        if cur and (size + len(en) > LIMIT * 1.3 or (size + len(en) > LIMIT and script != prev) or script[:3] != prev[:3]):
            chunks.append(cur); cur, size = [], 0
        cur.append((src, en)); size += len(en)
    if cur: chunks.append(cur)
    d = os.path.join(ROOT, 'text', 'chunks')
    os.makedirs(d, exist_ok=True)
    for k, ch in enumerate(chunks, 1):
        text = ' '.join(en for _, en in ch)
        g = {e: j for e, j, rx in gl_l if rx.search(text if e[:1].isupper() else text.lower())}
        items = []
        for i, (src, en) in enumerate(ch):
            it = {'id': '%03d-%03d' % (k, i), 'src': src, 'en': en}
            notes = []
            if src.startswith('GPL'):
                if en[:1].islower() or en.startswith((', ', '. ', ' ', "'s", '.', ',')):
                    notes.append('断片: 前の項目（または名前・数）の続き')
                if en.endswith(' ') or (en.count('{') > en.count('}')):
                    notes.append('断片: 後ろに名前・数・次の項目がつながる')
                if '{' not in en and '}' not in en and '~' not in en and len(en) <= 50 and not notes:
                    notes.append('短い文。選択肢（主人公の返事・行き先）なら 30 字以内')
            if notes: it['note'] = '。'.join(notes)
            items.append(it)
        json.dump({'chunk': k, 'glossary': g, 'items': items},
                  open(os.path.join(d, 'c%03d.en.json' % k), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(len(chunks), 'chunks')

if __name__ == '__main__':
    main()
