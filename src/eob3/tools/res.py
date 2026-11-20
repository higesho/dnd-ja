# AESOP resource file (EYE.RES) reader
import struct, os
GAME = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'GAME', 'EOB3'))
def load(fn='EYE.RES'):
    return open(os.path.join(GAME, fn), 'rb').read()
def entries(d):
    fob = struct.unpack_from('<I', d, 0x18)[0]
    out = []
    p = fob
    while p:
        nxt = struct.unpack_from('<I', d, p)[0]
        flags = d[p+4:p+4+128]
        idx = struct.unpack_from('<128I', d, p+4+128)
        for i in range(128):
            out.append((idx[i], flags[i]))
        p = nxt
    return out
def get(d, off):
    ts, attr, size = struct.unpack_from('<III', d, off)
    return attr, d[off+12:off+12+size]
def dict_pairs(b):
    nb = struct.unpack_from('<H', b, 0)[0]
    offs = struct.unpack_from('<%dI' % nb, b, 2)
    out = []
    for o in offs:
        p = o
        if not p: continue
        while True:
            l = struct.unpack_from('<H', b, p)[0]
            if l == 0: break
            k = b[p+2:p+2+l-1].decode('latin1'); p += 2 + l
            l = struct.unpack_from('<H', b, p)[0]
            v = b[p+2:p+2+l-1].decode('latin1'); p += 2 + l
            out.append((k, v))
    return out
def names(d=None):
    if d is None: d = load()
    e = entries(d)
    a, b = get(d, e[0][0])
    return {int(v): k for k, v in dict_pairs(b)}
def sources(d=None):
    if d is None: d = load()
    e = entries(d)
    a, b = get(d, e[1][0])
    return dict(dict_pairs(b))
