# 翻訳する英文を抜き出す → text/units.json [[出どころ, 英文], ...]（ゲーム内の順、重複なし）
#   ECL.GLB（イベント文: 6 ビット詰め）と DQK.EXE のデータ（画面の文字列: C 文字列）
import sys, os, re, json, struct
sys.path.insert(0, os.path.dirname(__file__))
from extract_ecl import strings
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
G = os.path.join(ROOT, 'test', 'game', 'DQK')

def exe_strings():
    e = open(os.path.join(G, 'DQK.EXE'), 'rb').read()
    out = []
    for m in re.finditer(rb'(?<=\x00)[\x20-\x7e]{2,}(?=\x00)', e):
        s = m.group(0)
        t = re.sub(rb'%[-0-9.]*[a-zA-Z]', b'', s)
        if re.search(rb'\.(tlb|glb|xmi|cmp|dat|qsv|qch|cfg|exe|adv|fon|voc)\b', s, re.I): continue
        if not re.search(rb'[A-Za-z]{2}', t): continue
        words = re.findall(rb'[A-Za-z]+', t)
        if not words: continue
        vowelish = sum(1 for w in words if re.search(rb'[AEIOUYaeiouy]', w))
        if vowelish < max(1, len(words) * 0.6): continue
        if re.search(rb'[\\{}\[\]|^`~]', s): continue
        out.append(('EXE:%05x' % m.start(), s.decode('latin1')))
    return out

def ecl_strings():
    d = open(os.path.join(G, 'DISK3', 'ECL.GLB'), 'rb').read()
    offs = list(struct.unpack_from('<49I', d, 16))
    out = []
    for i in range(48):
        for p, s in strings(d[offs[i]:offs[i + 1]]):
            out.append(('ECL%02d:%05x' % (i, p), s))
    return out

if __name__ == '__main__':
    seen = set(); res = []
    for src, s in exe_strings() + ecl_strings():
        if s in seen: continue
        seen.add(s); res.append([src, s])
    json.dump(res, open(os.path.join(ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(res), sum(len(s) for _, s in res), 'exe', sum(1 for a, _ in res if a.startswith('EXE')))
