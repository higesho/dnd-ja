"""LIQDFONT (.lfn) reader/writer.

'LIQDFONT' f32 version(1.0)
u32 nglyphs, u32 nsub
nsub * sub:
   7*u32 hdr (a,b,c,d, texW, texH, ntex)
   ntex * texW*texH*4 bytes (A8R8G8B8)
   nglyphs * 0x18 glyph: u16 ch, u16 tex, 5*u32
"""
import struct, sys


def parse(d):
    assert d[:8] == b'LIQDFONT'
    ver = d[8:12]
    nglyph, nsub = struct.unpack_from('<II', d, 12)
    p = 20
    subs = []
    for s in range(nsub):
        hdr = list(struct.unpack_from('<7I', d, p)); p += 28
        w, h, nt = hdr[4], hdr[5], hdr[6]
        texs = []
        for t in range(nt):
            texs.append(d[p:p + w * h * 4]); p += w * h * 4
        glyphs = []
        for g in range(nglyph):
            glyphs.append(list(struct.unpack_from('<HHi4f', d, p))); p += 0x18
        subs.append(dict(hdr=hdr, texs=texs, glyphs=glyphs))
    assert p == len(d), (p, len(d))
    return ver, subs


def build(ver, subs):
    n = len(subs[0]['glyphs'])
    out = bytearray(b'LIQDFONT' + ver + struct.pack('<II', n, len(subs)))
    for s in subs:
        hdr = list(s['hdr']); hdr[6] = len(s['texs'])
        out += struct.pack('<7I', *hdr)
        for t in s['texs']:
            out += t
        assert len(s['glyphs']) == n
        for g in s['glyphs']:
            out += struct.pack('<HHi4f', *g)
    return bytes(out)


if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    ver, subs = parse(d)
    for i, s in enumerate(subs):
        print(i, s['hdr'], len(s['texs']))
        for g in s['glyphs'][:4] + s['glyphs'][30:36] + s['glyphs'][-3:]:
            print('   ', g[0], repr(chr(g[0])), g[1:])
