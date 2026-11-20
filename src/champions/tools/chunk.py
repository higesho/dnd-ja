# 翻訳用に原文を text/chunks/cNN.en.json に分ける（用語集の抜粋・断片の注記つき）
#   python -X utf8 tools/chunk.py
import os, json, glob, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
CH = os.path.join(ROOT, 'text', 'chunks')
LIMIT = 2600
ORDER = ['menu', 'code', 'data', 'names', 'ecl']

def note_of(en, kind):
    if kind == 'menu':
        return '横並びメニューの項目。キーの英字は機械が付けるので、日本語だけを 2〜4 字で'
    n = []
    if en.startswith(' '):
        n.append('前に名前などが来る断片')
    core = en.strip()
    if en.endswith(' ') and not re.search(r'[.!?:\'"]$', core):
        n.append('後ろに名前などが続く断片')
    elif en.endswith(' '):
        n.append('後ろに別の文が続くことがある')
    return '・'.join(n)

def main():
    os.makedirs(CH, exist_ok=True)
    gloss = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    units = []
    for src in ORDER:
        for it in json.load(open(os.path.join(ROOT, 'text', 'en', src + '.json'), encoding='utf-8'))['items']:
            core = it['en'].strip()
            if not core:
                continue
            units.append((src, it, core))
    chunks, cur, size, cur_src = [], [], 0, None
    for u in units:
        if cur and (size + len(u[2]) > LIMIT or (u[0] != cur_src and (u[0] == 'ecl' or cur_src == 'menu'))):
            chunks.append(cur); cur, size = [], 0
        cur.append(u); size += len(u[2]); cur_src = u[0]
    if cur:
        chunks.append(cur)
    for f in glob.glob(os.path.join(CH, 'c*.en.json')):
        os.remove(f)
    index = {}
    for k, ch in enumerate(chunks, 1):
        items = []
        prev_cont = False
        for i, (src, it, core) in enumerate(ch):
            d = {'id': '%02d-%03d' % (k, i), 'src': it['id'], 'en': core}
            nt = note_of(it['en'], it['kind'])
            if src == 'ecl':
                cont = it['en'].endswith(' ') and not re.search(r"[.!?'\"]$", core)
                if prev_cont:
                    nt = '前の項目の続き（つないで 1 つの文）' + ('・' + nt if nt else '')
                if cont:
                    nt = nt.replace('・後ろに名前などが続く断片', '').replace('後ろに名前などが続く断片', '')
                    nt = (nt + '・' if nt else '') + '次の項目へ文が続く（または後ろに名前などが入る）'
                prev_cont = cont
            if nt:
                d['note'] = nt
            if it['kind'] == 'menu':
                d['kind'] = 'menu'
            items.append(d)
            index[d['id']] = {'en': it['en'], 'kind': it['kind']}
        text = '\n'.join(it['en'] for it in items).upper()
        g = {e: j for e, j in gloss.items() if len(e) > 1 and re.search(r'(?<![A-Z])' + re.escape(e.upper()) + r'(?![A-Z])', text)}
        json.dump({'chunk': k, 'glossary': g, 'items': items}, open(os.path.join(CH, 'c%02d.en.json' % k), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
        print('c%02d' % k, len(items), sum(len(it['en']) for it in items), 'chars', ch[0][0], '..', ch[-1][0], 'gloss', len(g))
    json.dump(index, open(os.path.join(CH, 'index.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)

if __name__ == '__main__':
    main()
