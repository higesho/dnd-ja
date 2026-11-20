# GPL / MAS スクリプトの中の圧縮文字列（0x05 + 7 ビット詰め、0x03 で終わり）を探して取り出す
#   python -X utf8 tools/gpltext.py           GPLDATA.GFF の文字列の数と例
import sys, os, re
sys.path.insert(0, os.path.dirname(__file__))
from gff import GFF
GAME = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'GAME', 'DARKSUN'))

def decode(b, p, maxlen=2000):
    """b[p] は 0x05 の次。戻り値 (文字列 bytes, 終わりの次の位置) / None"""
    out = bytearray()
    buf = 0; idx = 1; q = p
    while len(out) < maxlen:
        if idx > 0:
            if q >= len(b): return None
            buf = ((buf << 8) & 0xff00) | b[q]; q += 1
        c = (buf >> idx) & 0x7f
        if c == 3:
            return bytes(out), q
        out.append(c)
        idx += 1
        if idx > 7: idx = 0
    return None

def encode(s):
    """decode の逆（s は 0x00-0x7f の bytes）。0x03 で終わる"""
    vals = list(s) + [3]
    bits = ''.join(format(v, '07b') for v in vals)
    # decode は 1 字目を 2 バイト目までの窓の下から 1 ビット目…と読む = 先頭 1 ビット（バイト境界の手前）を捨てて MSB から 7 ビットずつ
    bits = '0' + bits
    while len(bits) % 8: bits += '0'
    return bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))

def good(s):
    if len(s) < 2: return False
    if any(c < 0x20 or c > 0x7e for c in s): return False
    letters = sum(1 for c in s if 65 <= c <= 90 or 97 <= c <= 122)
    return letters >= len(s) * 0.5

def scan(b):
    """(開始位置=0x05 の位置, 次の位置, 文字列)"""
    res = []
    p = 0
    while True:
        p = b.find(b'\x05', p)
        if p < 0: break
        r = decode(b, p + 1)
        if r and good(r[0]):
            res.append((p, r[1], r[0]))
            p = r[1]
        else:
            p += 1
    return res

if __name__ == '__main__':
    g = GFF(os.path.join(GAME, 'GPLDATA.GFF'))
    n = tot = 0
    for t in ('GPL ', 'MAS '):
        for i in sorted(g.types[t]):
            b = g.get(t, i)
            for p, q, s in scan(b):
                n += 1; tot += len(s)
                if n <= 25: print(t, i, hex(p), s.decode('latin1'))
    print(n, 'strings', tot, 'chars')
    # 往復の確認
    assert decode(encode(b'Hello there.'), 0)[0] == b'Hello there.'
