# 翻訳単位（text/units.json）を塊（text/chunks/cNNN.en.json）に分ける
#   会話（GPL/MAS）はスクリプトの切れ目でなるべく区切る
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LIMIT = 3200

def ends_sentence(s):
    return bool(re.search(r"[.!?'\")\]:]\s*$", s))

def main():
    gl = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    gl_l = [(e, j, re.compile(r'(?<![A-Za-z])' + re.escape(e) + r'(?![A-Za-z])') if e[:1].isupper() else
             re.compile(r'(?<![a-z])' + re.escape(e.lower()) + r'(?![a-z])')) for e, j in gl.items()]
    us = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    chunks, cur, size = [], [], 0
    for src, en in us:
        kind = src.split(':')[0]
        script = kind if kind[:3] in ('GPL', 'MAS') else kind[:3]
        prev_script = None
        if cur:
            k0 = cur[-1][0].split(':')[0]
            prev_script = k0 if k0[:3] in ('GPL', 'MAS') else k0[:3]
        if cur and (size + len(en) > LIMIT * 1.25 or (size + len(en) > LIMIT and script != prev_script)
                    or (script[:3] != (prev_script or '')[:3])):
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
            if src.startswith(('GPL', 'MAS')):
                if en.startswith('  '):
                    notes.append('選択肢（プレイヤーの返事）。25 字以内')
                elif i + 1 < len(ch) and not ends_sentence(en) and ch[i + 1][0].split(':')[0] == src.split(':')[0]:
                    notes.append('文の途中で切れている。次の項目に続く（つなげて読めるように訳す）')
                if en[:1].islower() or en.startswith(('. ', ', ', "'s")):
                    notes.append('前の項目（または名前）の続き')
            else:
                if re.search(r'%[-0-9.]*[a-zA-Z]', en): notes.append('書式。%d・%s などはそのまま同じ順番で残す（数や名前が入る）')
                if en.startswith(' ') or en.endswith(' '): notes.append('断片: 前後に名前・数・別の文がつながる')
            if notes: it['note'] = '。'.join(notes)
            items.append(it)
        json.dump({'chunk': k, 'glossary': g, 'items': items},
                  open(os.path.join(d, 'c%03d.en.json' % k), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(len(chunks), 'chunks')

if __name__ == '__main__':
    main()
