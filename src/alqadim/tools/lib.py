# Al-Qadim .LIB container: u32 n, n x (u32 offset, u32 size), then the entries
import struct
def entries(d):
    n = struct.unpack_from('<I', d, 0)[0]
    return [struct.unpack_from('<II', d, 4 + 8 * i) for i in range(n)]
def is_font(b):
    n = b[0]
    if not (1 <= n <= 31) or len(b) < n + 40: return False
    name = b[1:1 + n]
    if not all(0x20 <= c < 0x7f for c in name): return False
    k = struct.unpack_from('<H', b, 1 + n)[0]
    h = 1 + n + 2 * k
    return h + 26 <= len(b) and struct.unpack_from('<H', b, h)[0] in (0x9000, 0xb000, 0x9002, 0xb002)
def font_header(b):
    n = b[0]; k = struct.unpack_from('<H', b, 1 + n)[0]
    h = 1 + n + 2 * k
    f = struct.unpack_from('<13H', b, h)
    keys = 'fontType firstChar lastChar widMax kernMax nDescent fRectWidth fRectHeight owTLoc ascent descent leading rowWords'.split()
    return h, b[1:1 + n].decode('latin1'), dict(zip(keys, f))
