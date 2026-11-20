"""Liquid Entertainment H2O archive (Dragonshard, version 8) reader."""
import struct, sys, os
sys.path.insert(0, os.path.dirname(__file__))
import blast

ARC = r'L:\SteamLibrary\steamapps\common\Dungeons & Dragons Dragonshard\Archives'


def unpack(blob):
    packed, raw, crc = struct.unpack_from('<III', blob, 0)
    data = blob[12:12 + packed]
    if packed == raw:
        return data, 12 + packed
    return blast.explode(data), 12 + packed


def read(path):
    d = open(path, 'rb').read()
    o = d.index(b'\x1a') + 1
    ver, n, nfiles = struct.unpack_from('<III', d, o)
    o += 0x28
    ents = []
    for i in range(n):
        e = struct.unpack_from('<10I', d, o + i * 0x28)
        ents.append(e)
    o += n * 0x28
    dirnames, k = unpack(d[o:]); o += k
    names, k = unpack(d[o:]); o += k
    return d, ents, dirnames, names, o


def namelist(names):
    out = []
    p = 0
    while p + 2 <= len(names):
        e = names.find(b'\x00\x00', p)
        while e != -1 and (e - p) % 2:
            e = names.find(b'\x00\x00', e + 1)
        if e == -1:
            break
        out.append(names[p:e].decode('utf-16-le'))
        p = e + 2
    return out


def files(path):
    d, ents, dirnames, names, o = read(path)
    nl = namelist(names)
    res = []
    for e in ents:
        raw, packed, off = e[4], e[5], e[6]
        if raw == 0 and packed == 0:
            continue
        res.append((e, off))
    return d, res, nl, dirnames


def extract(path, outdir):
    d, res, nl, dn = files(path)
    if outdir: os.makedirs(outdir, exist_ok=True)
    out = []
    for idx, (e, off) in enumerate(res):
        packed, raw, crc = struct.unpack_from('<3I', d, off)
        blob = d[off + 12:off + 12 + packed]
        data = blob if packed == raw else blast.explode(blob)
        out.append((e, data))
    return out, nl, dn


if __name__ == '__main__':
    d, ents, dirnames, names, o = read(sys.argv[1])
    print('entries', len(ents), 'dirnames', dirnames[:200], 'names', namelist(names)[:50], hex(o))
    for e in ents[:10]:
        print(' '.join('%08x' % x for x in e))
