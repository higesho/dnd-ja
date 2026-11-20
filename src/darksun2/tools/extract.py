# 翻訳する英文を抜き出す → text/units.json [[出どころ, 英文], ...]（ゲーム内の順、重複なし）
#   GPL / MAS スクリプトの文（GPLDATA.GFF、7 ビット詰め）
#   DSUN.EXE のデータ（画面の文字列）
#   RESOURCE.GFF SPIN（呪文・超能力の説明）、GPLDATA.GFF NAME（アイテム名）、SEGOBJEX.GFF RDFF（人物・怪物の名前）
import sys, os, re, json, struct
sys.path.insert(0, os.path.dirname(__file__))
from gff import GFF
import gpltext
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
G = os.path.join(ROOT, 'test', 'game', 'RAVAGER')
HDR = 0x5200

def gpl_strings():
    g = GFF(os.path.join(G, 'GPLDATA.GFF'))
    out = []
    for t, tag in (('MAS ', 'MAS'), ('GPL ', 'GPL')):
        for i in sorted(g.types[t]):
            for p, q, s in gpltext.scan(g.get(t, i)):
                out.append(('%s%03d:%05x' % (tag, i, p), s.decode('latin1')))
    return out

def exe_strings():
    e = open(os.path.join(G, 'DSUN.EXE'), 'rb').read()
    out = []
    lo, hi = HDR + 0x44000, HDR + 0x52370
    for m in re.finditer(rb'(?:(?<=\x00)|(?<=VC))[\x20-\x7e]{2,}(?=\x00)', e[lo:hi]):    # 前がポインタ表（..VC）のものも
        s = m.group(0)
        t = re.sub(rb'%[-0-9.]*[a-zA-Z]', b'', s)
        if re.search(rb'\.(gff|exe|cfg|bnk|ini|sav|oda|bat|ad|drv|com)\b', s, re.I): continue
        if not re.search(rb'[A-Za-z]{2}', t): continue
        words = re.findall(rb'[A-Za-z]+', t)
        if not words: continue
        vowelish = sum(1 for w in words if re.search(rb'[AEIOUYaeiouy]', w))
        if vowelish < max(1, len(words) * 0.6): continue
        if re.search(rb'[\\{}\[\]|^`~@]', s): continue
        out.append(('EXE:%05x' % (lo + m.start() - HDR), s.decode('latin1')))
    return out

def spin_strings():
    g = GFF(os.path.join(G, 'RESOURCE.GFF'))
    out = []
    for i in sorted(g.types['SPIN']):
        s = g.get('SPIN', i).split(b'\x00')[0].rstrip(b'\r\n')
        if s: out.append(('SPIN:%d' % i, s.decode('latin1')))
    return out

def name_strings():
    """RESOURCE.GFF TEXT 1000: アイテム名（1 行 1 つ）"""
    g = GFF(os.path.join(G, 'RESOURCE.GFF'))
    out = []
    for k, s in enumerate(g.get('TEXT', 1000).split(b'\r\n')):
        if s and re.fullmatch(rb'[\x20-\x7e]+', s): out.append(('NAME:%d' % k, s.decode('latin1')))
    return out

def rdff_strings():
    g = GFF(os.path.join(G, 'OBJEX.GFF'))
    out = []
    for i in sorted(g.types['RDFF']):
        b = g.get('RDFF', i)
        m = re.search(rb"[A-Z][a-zA-Z' -]{1,30}(?=\x00)", b[10:90])
        s = m.group(0) if m else b''
        if len(s) >= 2 and re.fullmatch(rb'[\x20-\x7e]+', s): out.append(('RDFF:%d' % i, s.decode('latin1')))
    return out

def butn_strings():
    """RESOURCE.GFF のボタン・窓の見出し（長さ 1 バイト + 文字）"""
    g = GFF(os.path.join(G, 'RESOURCE.GFF'))
    out = []
    for t in ('BUTN', 'WIND'):
        for i in sorted(g.types[t]):
            b = g.get(t, i)
            for m in re.finditer(rb'([\x01-\x30])([\x20-\x7e]+)', b):
                n = m.group(1)[0]; s = m.group(2)[:n]
                if len(m.group(2)) >= n and re.search(rb'[A-Za-z]{2}', s) and s[:1].isalnum():
                    out.append(('%s:%d' % (t, i), s.decode('latin1')))
    return out

if __name__ == '__main__':
    # 前に作った units.json の並び（翻訳済みの塊の番号）はそのままにし、新しく見つかったものだけ後ろに足す
    path = os.path.join(ROOT, 'text', 'units.json')
    res = json.load(open(path, encoding='utf-8')) if os.path.exists(path) else []
    seen = set(s for _, s in res)
    for src, s in exe_strings() + name_strings() + rdff_strings() + spin_strings() + gpl_strings() + butn_strings():
        if s in seen: continue
        seen.add(s); res.append([src, s])
    json.dump(res, open(path, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    from collections import Counter
    c = Counter(a.split(':')[0][:3] for a, _ in res)
    print(len(res), sum(len(s) for _, s in res), dict(c))
