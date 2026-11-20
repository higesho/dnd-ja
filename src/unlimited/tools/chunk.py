# 翻訳単位（text/units.json）を塊（text/chunks/cNNN.en.json）に分ける
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LIMIT = 2600

def main():
    gl = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    gl_l = [(e, j, re.compile(r'(?<![a-z])' + re.escape(e.lower()) + r'(?![a-z])')) for e, j in gl.items()]
    us = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    # 名前の表（NAM）は text/names.json で訳す。EXE の 8e7c0h より後ろはプログラムの内部エラー文
    us = [(src, en) for src, en in us if not src.startswith('NAM')
          and not (src.startswith('EXE') and int(src[4:], 16) >= 0x8e7c0 and 'printer' not in en)]
    # 同じエンジンの Dark Queen of Krynn で訳し済みの画面の文字列（text/reuse_dqk.json）は訳し直さない
    rp = os.path.join(ROOT, 'text', 'reuse_dqk.json')
    reuse = json.load(open(rp, encoding='utf-8')) if os.path.exists(rp) else {}
    us = [(src, en) for src, en in us if not (src.startswith('EXE') and en in reuse)]
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
            if src.startswith('BTN'):
                notes.append('選択肢のボタン。2〜5 字で短く')
            if '^' in en:
                notes.append('^ は仲間の名前に置き換わる。同じ数だけ残す')
            if src.startswith('EXE') or src.startswith('SCR'):
                notes.append('英語は %d 字。訳はこの字数以内' % len(en))
                if re.search(r'%[-0-9.]*[a-zA-Z]', en): notes.append('書式。%d・%s などはそのまま同じ順番で残す（数や名前が入る）')
                if en.startswith(' ') or en.endswith(' '): notes.append('断片: 前後に名前・数・別の文がつながる')
            if notes: it['note'] = '。'.join(notes)
            items.append(it)
        json.dump({'chunk': k, 'glossary': g, 'items': items},
                  open(os.path.join(d, 'c%03d.en.json' % k), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(len(chunks), 'chunks')

if __name__ == '__main__':
    main()
