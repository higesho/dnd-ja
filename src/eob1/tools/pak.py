# Westwood PAK archive reader (EOBDATA*.PAK)
import struct, os
GAME = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'GAME', 'EYE'))
def entries(d):
    ents = []; p = 0
    while True:
        off = struct.unpack_from('<I', d, p)[0]
        if off == 0 or off >= len(d): break
        e = d.index(b'\0', p + 4); name = d[p + 4:e].decode('latin1'); p = e + 1
        ents.append([name, off])
        if p >= ents[0][1]: break
    out = []
    for i, (n, o) in enumerate(ents):
        end = ents[i + 1][1] if i + 1 < len(ents) else len(d)
        out.append((n, o, end - o))
    return out
def read_pak(fn):
    d = open(os.path.join(GAME, fn), 'rb').read()
    return [(n, d[o:o + l]) for n, o, l in entries(d)]
def all_files():
    r = {}
    for i in range(1, 7):
        for n, b in read_pak('EOBDATA%d.PAK' % i):
            r.setdefault(n, ('EOBDATA%d.PAK' % i, b))
    return r

def lcw(src, outlen):
    """Westwood Format80 (LCW) decompression"""
    out = bytearray(); i = 0
    while i < len(src) and len(out) < outlen:
        c = src[i]; i += 1
        if c == 0x80: break
        if not (c & 0x80):                       # short relative copy
            n = (c >> 4) + 3; rel = ((c & 0x0f) << 8) | src[i]; i += 1
            p = len(out) - rel
            for k in range(n): out.append(out[p + k])
        elif not (c & 0x40):                     # literal
            n = c & 0x3f; out += src[i:i + n]; i += n
        elif c == 0xfe:
            n = src[i] | (src[i + 1] << 8); v = src[i + 2]; i += 3; out += bytes([v]) * n
        elif c == 0xff:
            n = src[i] | (src[i + 1] << 8); p = src[i + 2] | (src[i + 3] << 8); i += 4
            for k in range(n): out.append(out[p + k])
        else:
            n = (c & 0x3f) + 3; p = src[i] | (src[i + 1] << 8); i += 2
            for k in range(n): out.append(out[p + k])
    return bytes(out)

def cps(b):
    """CPS container: u16 size, u16 comp, u32 outsize, u16 palsize"""
    import struct
    sz, comp, outsz, pal = struct.unpack_from('<HHIH', b, 0)
    data = b[10 + pal:]
    if comp == 4: return lcw(data, outsz)
    if comp == 0: return data[:outsz]
    raise ValueError('comp %d' % comp)
