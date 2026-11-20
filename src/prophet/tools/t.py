# Menzoberranzan "T" text file: u32 n, n * u32 offset, entries {u16 len, bytes ^ 0xDC}
import struct, os
GAME = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'GAME', 'PROPHET'))
def load(path=None):
    d = open(path or os.path.join(GAME, 'T'), 'rb').read()
    n = struct.unpack_from('<I', d, 0)[0]
    offs = struct.unpack_from('<%dI' % n, d, 4)
    out = []
    for o in offs:
        l = struct.unpack_from('<H', d, o)[0]
        out.append(bytes(b ^ 0xdc for b in d[o + 2:o + 2 + l]))
    return out
def build(strings):
    n = len(strings)
    hdr = struct.pack('<I', n)
    pos = 4 + 4 * n
    offs = []; body = b''
    for s in strings:
        offs.append(pos + len(body))
        body += struct.pack('<H', len(s)) + bytes(b ^ 0xdc for b in s)
    return hdr + b''.join(struct.pack('<I', o) for o in offs) + body
