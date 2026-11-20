"""LIQDTEXT (.ltx) string table reader/writer for Dragonshard.

layout: 'LIQDTEXT' + 8 header bytes, then groups until EOF:
  u8 flag, wstr name, u32 count, count * (u32 id, u8 flag, wstr text)
wstr = u32 nchars (incl. terminating NUL, 0 = empty/absent) + UTF-16LE
"""
import struct


def parse(r):
    assert r[:8] == b'LIQDTEXT'
    hdr = r[8:0x10]
    p = 0x10
    groups = []

    def ws():
        nonlocal p
        n = struct.unpack_from('<I', r, p)[0]
        s = r[p + 4:p + 4 + 2 * n].decode('utf-16-le', 'surrogatepass')
        p += 4 + 2 * n
        if n == 0:
            return None
        if not s.endswith('\x00'):
            raise ValueError('%x %r' % (p, s[:60]))
        return s[:-1]

    while p < len(r):
        gf = r[p]
        p += 1
        name = ws()
        cnt = struct.unpack_from('<I', r, p)[0]
        p += 4
        ents = []
        for i in range(cnt):
            eid, fl = struct.unpack_from('<IB', r, p)
            p += 5
            ents.append([eid, fl, ws()])
        groups.append([gf, name, ents])
    return hdr, groups


def build(hdr, groups):
    out = bytearray(b'LIQDTEXT' + hdr)

    def ws(s):
        if s is None:
            out.extend(struct.pack('<I', 0))
            return
        b = (s + '\x00').encode('utf-16-le', 'surrogatepass')
        out.extend(struct.pack('<I', len(b) // 2))
        out.extend(b)

    for gf, name, ents in groups:
        out.append(gf)
        ws(name)
        out.extend(struct.pack('<I', len(ents)))
        for eid, fl, s in ents:
            out.extend(struct.pack('<IB', eid, fl))
            ws(s)
    return bytes(out)
