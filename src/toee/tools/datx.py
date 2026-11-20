# ToEE DAT (TIG "1TAD") reader
#   python datx.py list DAT            -> flags size path
#   python datx.py extract DAT OUTDIR [ext,ext]
# footer (last 28 bytes): guid[16] '1TAD' u32 ? u32 tree_size; tree at EOF-tree_size:
#   u32 count, count * (u32 namelen, name, u32 ptr, u32 flags, u32 size, u32 csize, u32 offset, i32 parent, i32 child, i32 sibling)
#   flags & 0x400 = directory, flags & 2 = zlib compressed; offsets from the start of the file.
import sys, os, struct, zlib


def read_dat(path):
    f = open(path, 'rb')
    n = os.path.getsize(path)
    f.seek(-28, 2)
    t = f.read(28)
    assert t[16:20] == b'1TAD', path
    ts = struct.unpack('<I', t[24:])[0]
    f.seek(n - ts)
    d = f.read(ts)
    cnt = struct.unpack_from('<I', d, 0)[0]
    p = 4
    raw = []
    for i in range(cnt):
        nl = struct.unpack_from('<I', d, p)[0]; p += 4
        name = d[p:p + nl].rstrip(b'\0').decode('latin-1'); p += nl
        ptr, flags, size, csize, off, parent, child, sib = struct.unpack_from('<8i', d, p); p += 32
        raw.append([name, flags, size, csize, off, parent])
    ents = []
    for i, r in enumerate(raw):
        parts = [r[0]]
        q = r[5]
        while q >= 0:
            parts.append(raw[q][0]); q = raw[q][5]
        ents.append(('/'.join(reversed(parts)).lower(), r[1], r[2], r[3], r[4]))
    return f, ents


def get(f, e):
    name, flags, size, csize, off = e
    f.seek(off)
    if flags & 2:
        return zlib.decompress(f.read(csize))
    return f.read(size)


if __name__ == '__main__':
    f, ents = read_dat(sys.argv[2])
    if sys.argv[1] == 'list':
        for e in ents:
            print(e[1], e[2], e[0])
    else:
        exts = sys.argv[4].split(',') if len(sys.argv) > 4 else None
        k = 0
        for e in ents:
            if e[1] & 0x400: continue
            if exts and e[0].rsplit('.', 1)[-1] not in exts: continue
            out = os.path.join(sys.argv[3], e[0])
            os.makedirs(os.path.dirname(out), exist_ok=True)
            open(out, 'wb').write(get(f, e)); k += 1
        print(k, 'files')
