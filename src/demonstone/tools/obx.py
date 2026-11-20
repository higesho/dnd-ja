# OBX string files
#   OBSS (UI\*.OBX):       'OBSS', u32 1, u32 1, u32 n, n x {u32 id, u32 offset}, UTF-16 strings (offsets from file start)
#   OBST (subtitle\*.OBX): 'OBST', u32 2, u32 n, u32 0, n x {u32 hash, u32 offset, f32 start, f32 end}, UTF-16 strings
import struct


def u16z(d, o):
    e = o
    while e + 2 <= len(d) and d[e:e + 2] != b'\0\0': e += 2
    return d[o:e].decode('utf-16le', 'replace')


def read(path):
    d = open(path, 'rb').read()
    mag = d[:4]
    if mag == b'OBSS':
        n = struct.unpack_from('<I', d, 12)[0]
        return 'OBSS', [(i, u16z(d, struct.unpack_from('<II', d, 16 + 8 * i)[1])) for i in range(n)]
    if mag == b'OBST':
        n = struct.unpack_from('<I', d, 8)[0]
        return 'OBST', [(i, u16z(d, struct.unpack_from('<IIff', d, 16 + 16 * i)[1])) for i in range(n)]
    raise ValueError(path)
