# Python mirror of build_gff (eob3_jp.c) for experiments.
#   python tools/gff_build.py NAME OUT [mode]   mode: same (rows re-emitted unchanged) | reenc (orig pixels, our encoder) | ja
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__)); import gff

def frame_rows(c, f):
    p = f + 4; rows = []
    while True:
        r = p; y = c[p]; p += 1
        if y == 0xff: return rows, p
        while True:
            xw = struct.unpack_from('<H', c, p)[0]; p += 4 + c[p + 3]
            if xw & 0x8000: break
        rows.append((y, c[r:p]))

def rebuild_chunk(c, reps):   # reps: {frame_index: (y0, rowbytes)}
    out = bytearray(); b = 0; g = 0
    while b + 6 <= len(c):
        ln, n = struct.unpack_from('<IH', c, b)
        if not ln or not n: break
        offs = [struct.unpack_from('<I', c, b + 6 + 4 * k)[0] for k in range(n)]
        ob = len(out); out += c[b:b + 6 + 4 * n]
        for k, fo in enumerate(offs):
            fe = min([q for q in offs if q > fo] + [ln])
            struct.pack_into('<I', out, ob + 6 + 4 * k, len(out) - ob)
            if g in reps:
                y0, rb = reps[g]
                rows, end = frame_rows(c[b:b + ln], fo)
                start = len(out)                  # frames keep their size (ACF scripts hold block sizes)
                out += c[b + fo:b + fo + 4]
                for y, r in rows:
                    if y < y0: out += r
                out += rb + b'\xff'
                assert len(out) - start <= fe - fo
                out += bytes(fe - fo - (len(out) - start))
            else:
                out += c[b + fo:b + fe]
            g += 1
        struct.pack_into('<I', out, ob, len(out) - ob)
        b += ln
    out += c[b:]
    return bytes(out)

def rebuild(d, reps):   # reps: {(tag, id): {frame: (y0, rows)}}
    tl, tlen = struct.unpack_from('<II', d, 0xc)
    t = gff.toc(d)
    ents = []
    p = tl + 10
    for tag, lst in t.items():
        p += 8
        for i, (cid, off, ln) in enumerate(lst):
            ents.append([p + 12 * i, off, ln, tag, cid])
        p += 12 * len(lst)
    out = bytearray(); src = 0
    newpos = {}
    for e in sorted(ents, key=lambda e: e[1]):
        te, off, ln, tag, cid = e
        out += d[src:off]
        c = d[off:off + ln]
        if (tag, cid) in reps: c = rebuild_chunk(c, reps[(tag, cid)])
        newpos[te] = (len(out), len(c)); out += c; src = off + ln
    out += d[src:tl]; ntl = len(out); out += d[tl:]
    for te, (o, l) in newpos.items():
        struct.pack_into('<II', out, ntl + (te - tl) + 4, o, l)
    struct.pack_into('<I', out, 0xc, ntl)
    return bytes(out)
