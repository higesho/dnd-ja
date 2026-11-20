# Split text/en/strings.json (T file) into translation chunks text/chunks/cNN.en.txt (+ glossary subsets).
# Escapes in the chunk files: \x01 -> <P>, \x09 -> <T>, real line break -> \n
import json, os, re
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
LIMIT = 4500

def esc(s):
    return s.replace('\\', '\\\\').replace('\n', '\\n').replace('\x01', '<P>').replace('\x09', '<T>')

def kind(x):
    s = x['en'].strip()
    if s.startswith('"'): return '台詞'
    if s.isupper(): return '画面の語'
    return ''

def main():
    en = json.load(open(os.path.join(ROOT, 'text', 'en', 'strings.json'), encoding='utf-8'))
    gl = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    rp = os.path.join(ROOT, 'text', 'reuse.json')
    reuse = json.load(open(rp, encoding='utf-8')) if os.path.exists(rp) else {}
    items = [x for x in en if x['en'].strip() and x['en'].strip().lower() != 'empty' and str(x['id']) not in reuse]
    chunks, cur, size = [], [], 0
    for x in items:
        if size > LIMIT and cur:
            chunks.append(cur); cur, size = [], 0
        cur.append(x); size += len(x['en']) + 20
    if cur: chunks.append(cur)
    od = os.path.join(ROOT, 'text', 'chunks')
    os.makedirs(od, exist_ok=True)
    for k, ch in enumerate(chunks):
        lines = []
        for x in ch:
            lines.append('[%d] %s' % (x['id'], esc(x['en'])))
        open(os.path.join(od, 'c%02d.en.txt' % k), 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
        text = ' '.join(x['en'].lower() for x in ch)
        sub = [g for g in gl if re.search(r'(?<![a-z])' + re.escape(g['en'].lower().split(' (')[0]) + r'(?![a-z])', text)]
        json.dump(sub, open(os.path.join(od, 'c%02d.gloss.json' % k), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(chunks), 'chunks', [len(c) for c in chunks])

if __name__ == '__main__':
    main()
