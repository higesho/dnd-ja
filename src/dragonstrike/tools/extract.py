# DragonStrike の訳す文字列を集める -> text/units.json
#   文章ファイル（DF.TXT 物語・任務の前置き, GAME.TXT 任務の結果）: 項目（~ 区切り）ごと、段落のリスト
#   MAIN.EXE の文字列: 位置・長さ（訳は同じバイト数に収める）
#   コピープロテクトの文（MAIN.EXE、1 バイトずつ c = 5 - b で符号化）
import os, re, json, sys
sys.path.insert(0, os.path.dirname(__file__))
import wwtext
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'DS')


def parse_entry(raw):
    """項目 -> (前置き, 段落のリスト, 字下げ, 1 段落目の字下げ, 幅)。文の行は '@' で終わる（40 桁ちょうどの行は '@' なし）"""
    lines = raw.split('\r\n')
    text_idx = [i for i, l in enumerate(lines) if re.search('[A-Za-z]', l)]
    if not text_idx: return None
    a, b = text_idx[0], text_idx[-1]
    body = lines[a:b + 1]
    clean = [l[:-1] if l.endswith('@') else l for l in body]
    indents = [len(l) - len(l.lstrip(' ')) for l in clean]
    base = min(indents)
    width = max(len(l) for l in clean)
    paras, cur = [], []
    for i, l in enumerate(clean):
        s = l.strip()
        if cur and (indents[i] > base):          # 字下げ = 段落の始め
            paras.append(' '.join(cur)); cur = []
        cur.append(s)
        # 次の行の最初の語が入るのに改行している = 段落の終わり
        if i + 1 < len(clean):
            nxt = clean[i + 1].strip().split(' ')[0]
            if len(l.rstrip()) + 1 + len(nxt) <= width - 1 and body[i].endswith('@') and indents[i + 1] <= base:
                paras.append(' '.join(cur)); cur = []
    if cur: paras.append(' '.join(cur))
    return {'head': '\r\n'.join(lines[:a]), 'tail': '\r\n'.join(lines[b + 1:]), 'indent': base,
            'first_indent': indents[0], 'width': width, 'paras': paras}


def text_units(fname, prefix):
    t = wwtext.decode(open(os.path.join(GAME, fname), 'rb').read())
    ents = t.split('~')
    units = []
    for i, e in enumerate(ents):
        p = parse_entry(e)
        if not p: continue
        p['id'] = '%s%02d' % (prefix, i)
        p['file'] = fname; p['entry'] = i
        units.append(p)
    return units


# MAIN.EXE: 訳す文字列（ファイル位置, 長さ）。長さはゼロ終わりの前まで、または固定長の欄
EXE_SKIP = {'PUSQWR', 'PUSQR', 'ZY[]X', 'PUSQRWV', '<Ot <\\t', '^_ZY[]X', 'PSQR.', 'PSQV2', 'USQR3', '2ZY[]', 'PUSQW', 'hHCAJ',
            'PSQWR', 'PSQR3', 'tFPSQ.', 'D4DSD', 'E:EME`EmE', 'F!FPF', 'F]GdGkGrGyGzG', 'PUSQR', 'tD,<x@2', 'N.NHNbN',
            'PaPgP', 'P&QFQqQ', 'W=WwW', 'XQXfX', 'Z,ZHZ', 'PSQRU', ']ZY[X', 'tJPSQW', '0127777789ABC', '?(yiyO', 'sPSUQWR',
            '.+6Ko', '2ZY[X', 'q%q%qnqnq', 'q5rfr', 'PSUQR', 'ZY][X', 'PSQWR.', 'PSQRP'}


EXE_JUNK = {'x043cf', 'x04e3e', 'x0631d', 'x06b5e', 'x06c38', 'x06db3', 'x072d3', 'x0777d', 'x077ec', 'x07de6', 'x0a12b',
            'x0a20e', 'x0acb3', 'x0adf0', 'x064d3', 'x0a25d'}


def exe_units():
    d = open(os.path.join(GAME, 'MAIN.EXE'), 'rb').read()
    units = []
    for m in re.finditer(rb'[\x20-\x7e]{3,}', d):
        a = m.start()
        if not (0x5f0 < a < 0xb000) or 0x98e0 <= a < 0x9b40: continue
        s = m.group().decode('latin1')
        if s.strip() in EXE_SKIP or s in EXE_SKIP: continue
        if not re.search('[A-Za-z]{3}', s) and s.strip() not in ('Yes', 'No'): continue
        if 'x%05x' % a in EXE_JUNK: continue
        if len(s.strip()) <= 6 and s.strip() not in ('Yes', 'No', 'Load', 'Save', 'Exit', 'Thew', 'Rout', 'Demo', 'Dolce',
                                                       'Argent', 'Sirdar', 'Ulaina', 'Wyvern', 'GORED!'): continue
        term = d[m.end()] if m.end() < len(d) else 0
        units.append({'id': 'x%05x' % a, 'off': a, 'len': len(s), 'en': s, 'term': term})
    return units


def copy_units():
    """コピープロテクトの問い（c = 5 - b）。ゼロ終わり（符号化しない 0）"""
    d = open(os.path.join(GAME, 'MAIN.EXE'), 'rb').read()
    units = []
    for a, end in ((0x98ef, 0x99e0), (0xcea5, 0xcf96)):      # 始めの問い、ゲームの途中の問い
        while a < end:
            e = d.index(b'\0', a)
            s = bytes((5 - b) & 0xff for b in d[a:e]).decode('latin1')
            units.append({'id': 'c%05x' % a, 'off': a, 'len': e - a, 'en': s})
            a = e + 1
    return units


def main():
    u = {'df': text_units('DF.TXT', 'df'), 'game': text_units('GAME.TXT', 'gm'), 'exe': exe_units(), 'copy': copy_units()}
    json.dump(u, open(os.path.join(ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    for k, v in u.items():
        print(k, len(v), sum(len(' '.join(x.get('paras', [x.get('en', '')]))) for x in v), 'chars')


if __name__ == '__main__':
    main()
