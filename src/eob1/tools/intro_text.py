# オープニングの字幕画像 TEXT.CMP（EOBDATA1.PAK の中、320x200、1 行 16 ドットの帯が 12 本）を
# 日本語で描き直す。東雲ゴシック 14 ドット（パブリックドメイン）を使う。
#   python -X utf8 tools/intro_text.py      -> build/TEXT.CMP と capture/intro_ja.png
import os, sys, json, struct
sys.path.insert(0, os.path.dirname(__file__))
import pak

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
K14 = os.path.join(ROOT, 'font', 'shnmk14.bdf')
A14 = os.path.join(ROOT, 'font', 'shnm7x14a.bdf')
BG = 12
WHITE, WHITE2 = 143, 141     # 本体と縁の色（白い行）
RED, RED2 = 168, 134         # 赤い行

def load_bdf(path):
    g, cur, i = {}, None, 0
    lines = open(path, encoding='latin1').read().split('\n')
    while i < len(lines):
        l = lines[i]
        if l.startswith('ENCODING'):
            cur = int(l.split()[1])
        elif l.startswith('DWIDTH'):
            dw = int(l.split()[1])
        elif l.startswith('BBX'):
            w, h, xo, yo = map(int, l.split()[1:])
        elif l == 'BITMAP':
            rows = [int(lines[i + 1 + r], 16) for r in range(h)]
            g[cur] = (dw, w, h, xo, yo, rows, (len(lines[i + 1]) * 4))
            i += h
        i += 1
    return g

KG, AG = load_bdf(K14), load_bdf(A14)

def glyph(ch):
    if ord(ch) < 0x80:
        return AG[ord(ch)]
    b = ch.encode('euc_jp')
    return KG[((b[0] & 0x7f) << 8) | (b[1] & 0x7f)]

def text_width(s):
    return sum(glyph(c)[0] for c in s)

def draw(img, s, top, col, col2):
    x = (320 - text_width(s)) // 2
    asc = 12                             # 14 ドットフォントのベースライン
    for ch in s:
        dw, w, h, xo, yo, rows, bits = glyph(ch)
        for r in range(h):
            yy = top + asc - yo - h + r
            for k in range(w):
                if rows[r] >> (bits - 1 - k) & 1:
                    for dx, dy, c in ((1, 1, col2), (0, 0, col)):
                        px, py = x + xo + k + dx, yy + dy
                        if 0 <= px < 320 and 0 <= py < 200 and (c == col or img[py * 320 + px] == BG):
                            img[py * 320 + px] = c
        x += dw

def lcw_compress(d):
    out = bytearray(); i = 0; lit = bytearray()
    def flush():
        nonlocal lit
        while lit:
            n = min(63, len(lit)); out.append(0x80 | n); out.extend(lit[:n]); lit = lit[n:]
    while i < len(d):
        j = i
        while j < len(d) and d[j] == d[i] and j - i < 0xffff: j += 1
        if j - i >= 5:
            flush(); out += bytes([0xfe]) + struct.pack('<H', j - i) + bytes([d[i]]); i = j
        else:
            lit.append(d[i]); i += 1
    flush(); out.append(0x80)
    return bytes(out)

def main():
    lines = json.load(open(os.path.join(ROOT, 'text', 'intro.json'), encoding='utf-8'))
    img = bytearray([BG]) * 64000
    for i, (s, red) in enumerate(lines):
        top = i * 16
        draw(img, s, top, RED if red else WHITE, RED2 if red else WHITE2)
    assert pak.lcw(lcw_compress(bytes(img)), 64000) == bytes(img)
    body = struct.pack('<HIH', 4, 64000, 0) + lcw_compress(bytes(img))
    data = struct.pack('<H', len(body)) + body
    open(os.path.join(ROOT, 'build', 'TEXT.CMP'), 'wb').write(data)
    from PIL import Image
    p = pak.all_files()['PALETTE.COL'][1]
    im = Image.frombytes('P', (320, 200), bytes(img)); im.putpalette(bytes(min(255, c * 4) for c in p))
    im.convert('RGB').resize((640, 400), Image.NEAREST).save(os.path.join(ROOT, 'capture', 'intro_ja.png'))
    print('TEXT.CMP', len(data))

if __name__ == '__main__':
    main()
