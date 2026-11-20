# 訳を DLL のリソース（build/patch.bin）にまとめる
#   text/units.json（原文）+ text/ja.json（id -> 訳、チャンクから tools/merge.py が作る）+ text/fixes.json（原文 -> 訳、手直し・追加）
#   日本語の文字は font/k8x12.bdf のグリフ（8x12）を入れる
import os, json, struct, re, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


def bdf(path, fh=12):
    g, cur, rows, bbx = {}, None, None, None
    asc = 10
    for line in open(path, encoding='latin1'):
        t = line.split()
        if not t: continue
        if t[0] == 'FONT_ASCENT': asc = int(t[1])
        elif t[0] == 'ENCODING': cur = int(t[1])
        elif t[0] == 'BBX': bbx = list(map(int, t[1:5]))
        elif t[0] == 'BITMAP': rows = []
        elif t[0] == 'ENDCHAR':
            w, h, ox, oy = bbx
            top = asc - (oy + h)               # 枠の上（12 行の枠）からの行
            frame = [0] * fh
            for i, r in enumerate(rows):
                y = top + i
                if 0 <= y < fh: frame[y] = (int(r, 16) >> ox) & 0xff if len(r) <= 2 else (int(r[:2], 16) >> ox) & 0xff
            g[cur] = frame
            rows = None
        elif rows is not None:
            rows.append(t[0])
    return g


def load_tr():
    units = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    ja = {}
    p = os.path.join(ROOT, 'text', 'ja.json')
    if os.path.exists(p): ja = json.load(open(p, encoding='utf-8'))
    tr = {}
    for u in units:
        if u['id'] in ja and ja[u['id']]: tr[u['en']] = ja[u['id']]
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p):
        for k, v in json.load(open(p, encoding='utf-8')).items():
            if k.startswith('_'): continue
            tr[k] = v
    # モンスター名は "_" を空白にした形でも
    for u in units:
        if u.get('va') == 'mon' and u['en'] in tr: tr.setdefault(u['en'].replace('_', ' '), tr[u['en']])
    return tr


def main():
    tr = load_tr()
    font = bdf(os.path.join(ROOT, 'font', 'k8x12.bdf'))
    font8 = bdf(os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf'), 8)
    sys.path.insert(0, os.path.dirname(__file__))
    import make_subs
    subs_bin, subs_chars = make_subs.build()
    open(os.path.join(ROOT, 'build', 'subs.bin'), 'wb').write(subs_bin)
    print('subs.bin', len(subs_bin), 'bytes')
    chars = sorted({ord(c) for v in list(tr.values()) + [subs_chars] for c in v if ord(c) >= 0x80})
    miss = [chr(c) for c in chars if c not in font]
    if miss: print('no glyph:', ''.join(miss))
    glyphs = [(c, font[c]) for c in chars if c in font]
    glyphs8 = [(c, font8.get(c, [0] * 8)) for c in chars if c in font]
    miss8 = [chr(c) for c in chars if c not in font8]
    if miss8: print('no 8x8 glyph:', ''.join(miss8))
    exact = [(k, v) for k, v in tr.items() if not re.search(r'%[-0-9l]*[sdiuc]', k)]
    fmts = [(k, v) for k, v in tr.items() if re.search(r'%[-0-9l]*[sdiuc]', k)]
    # 長い書式（固定部分の多いもの）から照合する
    fmts.sort(key=lambda kv: -len(re.sub(r'%[-0-9l]*[sdiuc]', '', kv[0])))
    pool = bytearray()
    def add(s):
        o = len(pool); pool.extend(s.encode('utf-8') + b'\0'); return o
    hdr = 36
    goff = hdr
    g8off = goff + 16 * len(glyphs)
    soff = g8off + 12 * len(glyphs8)
    foff = soff + 8 * len(exact)
    poff = foff + 8 * len(fmts)
    out = bytearray(b'DKJ2' + struct.pack('<8I', len(glyphs), goff, len(exact), soff, len(fmts), foff, len(glyphs8), g8off))
    for c, rows in glyphs: out += struct.pack('<I', c) + bytes(rows)
    for c, rows in glyphs8: out += struct.pack('<I', c) + bytes(rows)
    for k, v in exact: out += struct.pack('<II', poff + add(k), poff + add(v))
    pairs = []
    for k, v in fmts: pairs.append((add(k), add(v)))
    for a, b in pairs: out += struct.pack('<II', poff + a, poff + b)
    assert len(out) == poff
    out += pool
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(out)
    # 画像（build/img/*.PCX、tools/make_images.py が作る）-> build/images.bin: 'DKIM', n, {name[16], off, size}
    import glob
    fs = sorted(glob.glob(os.path.join(ROOT, 'build', 'img', '*.PCX')))
    im = bytearray(b'DKIM' + struct.pack('<I', len(fs)))
    body = bytearray()
    base = 8 + 24 * len(fs)
    for f in fs:
        d = open(f, 'rb').read()
        im += os.path.basename(f).upper().encode().ljust(16, bytes(1)) + struct.pack('<II', base + len(body), len(d))
        body += d
    open(os.path.join(ROOT, 'build', 'images.bin'), 'wb').write(im + body)
    print('images.bin', len(fs), 'files', len(im) + len(body), 'bytes')
    print('patch.bin', len(out), 'bytes;', len(exact), 'strings', len(fmts), 'formats', len(glyphs), 'glyphs')


if __name__ == '__main__':
    main()
