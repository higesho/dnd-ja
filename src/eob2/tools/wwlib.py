# Westwood file helpers (CPS/LCW "format 80")
import struct, os
GAME = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'GAME', 'DARKMOON'))

def lcw_decode(src, outlen=None):
    out = bytearray(); i = 0
    while i < len(src):
        c = src[i]; i += 1
        if c == 0x80:
            break
        if not (c & 0x80):                      # 0cccpppp p : copy relative
            n = ((c & 0x70) >> 4) + 3
            pos = ((c & 0x0f) << 8) | src[i]; i += 1
            for k in range(n): out.append(out[-pos])
        elif not (c & 0x40):                    # 10cccccc : literal
            n = c & 0x3f
            out += src[i:i+n]; i += n
        elif c == 0xfe:                         # fill
            n = struct.unpack_from('<H', src, i)[0]; v = src[i+2]; i += 3
            out += bytes([v]) * n
        elif c == 0xff:                         # long abs copy
            n, pos = struct.unpack_from('<HH', src, i); i += 4
            for k in range(n): out.append(out[pos + k])
        else:                                   # 11cccccc pppp : short abs copy
            n = (c & 0x3f) + 3
            pos = struct.unpack_from('<H', src, i)[0]; i += 2
            for k in range(n): out.append(out[pos + k])
    return bytes(out)

def lcw_encode_literal(data):
    """Trivial LCW encoder (literals only, 63-byte runs)."""
    out = bytearray()
    for k in range(0, len(data), 63):
        ch = data[k:k+63]
        out.append(0x80 | len(ch)); out += ch
    out.append(0x80)
    return bytes(out)

def read_cps(path):
    d = open(path, 'rb').read()
    fsize, comp, usize, palsz = struct.unpack_from('<HHIH', d, 0)
    body = d[10 + palsz:]
    if comp == 4:
        return lcw_decode(body), d[:10 + palsz]
    if comp == 0:
        return body[:usize], d[:10 + palsz]
    raise ValueError('comp %d' % comp)
