# 翻訳単位（text/units.json）を塊（text/chunks/cNNN.en.json）に分ける。会話（GPL）はスクリプトの切れ目でなるべく区切る
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LIMIT = 3000

def gl_rx(e):
    return re.compile(r'(?<![A-Za-z])' + re.escape(e) + r'(?![A-Za-z])') if e[:1].isupper() else \
           re.compile(r'(?<![a-z])' + re.escape(e.lower()) + r'(?![a-z])')

def main():
    gl = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    gl_l = [(e, j, gl_rx(e)) for e, j in gl.items()]
    us = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    chunks, cur, size = [], [], 0
    for src, en in us:
        script = ':'.join(src.split(':')[:2])
        prev = ':'.join(cur[-1][0].split(':')[:2]) if cur else None
        if cur and (size + len(en) > LIMIT * 1.3 or (size + len(en) > LIMIT and script != prev) or script.split(':')[0] != prev.split(':')[0]):
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
            if src.startswith('UI:') and len(en) <= 24: notes.append('画面のメニュー・名前。短く（英語の字数以内が目安）')
            if '###' in en: notes.append('### で始まる語はボタン名。そのまま残す')
            if notes: it['note'] = '。'.join(notes)
            items.append(it)
        json.dump({'chunk': k, 'glossary': g, 'items': items},
                  open(os.path.join(d, 'c%03d.en.json' % k), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(len(chunks), 'chunks')

if __name__ == '__main__':
    main()
