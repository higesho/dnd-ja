# SSI GFFI container reader
import struct, sys, os
GAME = os.path.join(os.path.dirname(__file__), '..', '..', 'GAME', 'EOB3')

def load(name):
    d = open(os.path.join(GAME, name), 'rb').read()
    m, v, dl, tl, tlen = struct.unpack_from('<4sIIII', d, 0)
    p = tl
    ntypes, tsize = struct.unpack_from('<IH', d, p)
    p += 4
    # observed: dword?, word count of types
    return d

def toc(d):
    m, v, dl, tl, tlen = struct.unpack_from('<4sIIII', d, 0)
    p = tl + 8
    ntypes = struct.unpack_from('<H', d, p)[0]; p += 2
    out = {}
    for _ in range(ntypes):
        tag = d[p:p+4].decode('latin1'); n = struct.unpack_from('<I', d, p+4)[0]; p += 8
        ents = []
        for _ in range(n):
            i, off, ln = struct.unpack_from('<III', d, p); p += 12
            ents.append((i, off, ln))
        out[tag] = ents
    return out

if __name__ == '__main__':
    for f in sys.argv[1:]:
        d = open(os.path.join(GAME, f), 'rb').read()
        t = toc(d)
        print(f)
        for k, v in t.items():
            print(' ', repr(k), len(v), [(i, hex(o), l) for i, o, l in v][:30])

def frames(chunk):
    """frame offsets of every sub-block (a chunk may hold several len/count/offset blocks back to back)"""
    out = []
    b = 0
    while b + 6 <= len(chunk):
        ln, n = struct.unpack_from('<IH', chunk, b)
        if ln == 0 or n == 0:
            break
        out += [b + struct.unpack_from('<I', chunk, b + 6 + 4 * k)[0] for k in range(n)]
        b += ln
    return out

def decode_frame(chunk, off, W=320, H=200):
    """returns (w, h, pixels dict-free list with None for transparent)"""
    w, h = struct.unpack_from('<HH', chunk, off)
    px = [None] * (w * h)
    p = off + 4
    while True:
        y = chunk[p]; p += 1
        if y == 0xff:
            break
        while True:
            xw = struct.unpack_from('<H', chunk, p)[0]; p += 2
            width = chunk[p] or 256; dl = chunk[p + 1]; p += 2
            x = xw & 0x7fff
            q = p; end = p + dl; out = []
            while q < end:
                b = chunk[q]; q += 1
                c = (b >> 1) + 1
                if b & 1:
                    out += [chunk[q]] * c; q += 1
                else:
                    out += list(chunk[q:q + c]); q += c
            p = end
            for k, v in enumerate(out[:width]):
                if 0 <= y < h and x + k < w:
                    px[y * w + x + k] = v
            if xw & 0x8000:
                break
    return w, h, px

def pal(d, t, i=1):
    for j, o, l in t['PAL ']:
        if j == i:
            return [min(255, v * 4) for v in d[o:o + 768]]
