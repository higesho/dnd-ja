# 8x12 Japanese glyphs for the text fonts: Shinonome 12 (12x12) squeezed to 8 columns.
# Column mapping keeps every stroke: each target column ORs the source columns it covers by >= 1/3,
# so thin vertical strokes and dakuten survive (320x400 pixels are ~1.67:1 wide, so 8x12 looks square).
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
def load(load_bdf):
    g = {}
    for code, bm in load_bdf(os.path.join(ROOT, 'font', 'shnmk12.bdf'), 12).items():
        try:
            g[ord(bytes([(code >> 8) | 0x80, (code & 0xff) | 0x80]).decode('euc_jp'))] = bm
        except Exception:
            pass
    return g
def squeeze(bm):
    """12x12 (2 bytes/row) -> 8x12 (1 byte/row)"""
    out = bytearray()
    for y in range(12):
        row = (bm[2 * y] << 8) | bm[2 * y + 1]
        src = [(row >> (15 - x)) & 1 for x in range(12)]
        v = 0
        for tx in range(8):
            x0, x1 = tx * 1.5, (tx + 1) * 1.5
            on = 0
            for sx in range(12):
                ov = max(0.0, min(x1, sx + 1) - max(x0, sx))
                if ov >= 0.5 - 1e-9 and src[sx]: on = 1
            v |= on << (7 - tx)
        out.append(v)
    return bytes(out)
