# Decode / encode AESOP VFX shapes ("1.10" bitmaps)
import struct
def shapes(b):
    assert b[:4] == b'1.10'
    n = struct.unpack_from('<I', b, 4)[0]
    return [struct.unpack_from('<II', b, 8 + i * 8) for i in range(n)]
def decode(b, idx):
    off = shapes(b)[idx][0]
    h = struct.unpack_from('<H', b, off)[0] + 1
    w = struct.unpack_from('<H', b, off + 2)[0] + 1
    hdr = b[off:off + 24]
    pix = [[None] * w for _ in range(h)]
    pos = off + 24
    for y in range(h):
        x = 0
        while True:
            m = b[pos]; pos += 1
            if m == 0: break
            if m == 1:
                x += b[pos]; pos += 1; continue
            a = m >> 1
            if m & 1:
                for i in range(a):
                    if x < w: pix[y][x] = b[pos]
                    pos += 1; x += 1
            else:
                v = b[pos]; pos += 1
                for i in range(a):
                    if x < w: pix[y][x] = v
                    x += 1
    return w, h, pix, hdr
