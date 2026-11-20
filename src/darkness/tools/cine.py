# Pools of Darkness のオープニング（INTRO.BMA）: フレームの行データの位置を調べる
import struct, os, collections
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BMA = os.path.join(ROOT, '..', 'GAME', 'DARKNESS', 'INTRO.BMA')

def frames(d):
    out = []; b = 0
    while b + 6 <= len(d):
        ln, n = struct.unpack_from('<IH', d, b)
        if ln == 0 or n == 0: break
        out += [(b + struct.unpack_from('<I', d, b + 6 + 4 * k)[0], b + ln) for k in range(n)]
        b += ln
    return out          # (フレームの位置, そのブロックの終わり)

def rows(d, off):
    """[(y, 行の先頭位置, [(x, 幅, ピクセル列)])], 終端 0xff の位置"""
    w, h = struct.unpack_from('<HH', d, off)
    p = off + 4; res = []
    while True:
        y = d[p]
        if y == 0xff: return res, p
        start = p; p += 1; segs = []
        while True:
            xw = struct.unpack_from('<H', d, p)[0]; width = d[p + 2] or 256; dl = d[p + 3]; p += 4
            q, end, out = p, p + dl, []
            while q < end:
                b = d[q]; q += 1; c = (b >> 1) + 1
                if b & 1: out += [d[q]] * c; q += 1
                else: out += list(d[q:q + c]); q += c
            p = end
            segs.append((xw & 0x7fff, width, out[:width]))
            if xw & 0x8000: break
        res.append((y, start, segs))

Y0, Y1 = 164, 197
BG = {14: 0, 66: 176}          # ほかは 32
FG = 7

def wrap(s, width=38):
    """句読点の後ろで切る（行の長さがそろう位置を優先）"""
    from mp_helpers import NO_START
    lines = []
    while len(s) > width:
        n = (len(s) + width - 1) // width
        target = (len(s) + n - 1) // n
        cands = [i + 1 for i, ch in enumerate(s[:width]) if ch in '、。！？…' and i + 1 < len(s) and s[i + 1] not in NO_START]
        if cands:
            cut = min(cands, key=lambda c: abs(c - target))
        else:
            cut = min(width, target)
            while cut < len(s) and s[cut] in NO_START: cut += 1
        lines.append(s[:cut]); s = s[cut:]
    lines.append(s)
    return lines

def mask(text, gl):
    """字幕 1 枚分の 1bpp（40 バイト × 行）"""
    H = Y1 - Y0 + 1
    pix = [[0] * 320 for _ in range(H)]
    lines = wrap(text)
    top = (H - (10 * len(lines) - 2)) // 2
    for li, line in enumerate(lines):
        x0 = (320 - 8 * len(line)) // 2
        for k, ch in enumerate(line):
            g = gl.get(ord(ch))
            if g is None:
                if ch not in ' 　': print('no glyph', ch)
                continue
            for gy in range(8):
                for gx in range(8):
                    if g[gy] & (0x80 >> gx): pix[top + 10 * li + gy][x0 + 8 * k + gx] = 1
    out = bytearray()
    for row in pix:
        for b in range(40):
            out.append(sum(row[b * 8 + i] << (7 - i) for i in range(8)))
    return bytes(out), lines

def rle(seg):
    out = bytearray(); i = 0
    while i < len(seg):
        r = 1
        while i + r < len(seg) and seg[i + r] == seg[i] and r < 128: r += 1
        if r >= 3:
            out += bytes([((r - 1) << 1) | 1, seg[i]]); i += r; continue
        r = 1
        while i + r < len(seg) and r < 128 and not (i + r + 2 < len(seg) and seg[i + r] == seg[i + r + 1] == seg[i + r + 2]): r += 1
        out += bytes([(r - 1) << 1]) + bytes(seg[i:i + r]); i += r
    return bytes(out)

def splice(d, off, size, m, fg, bg):
    """DLL と同じ処理（確かめ用）。新しいフレームのバイト列（size に満たなければ 0 で埋める）か None"""
    rs, term = rows(d, off)
    out = bytearray(d[off:off + 4])
    def orig_row(y):
        for yy, st, segs in rs:
            if yy == y: return segs
        return None
    done = False
    for y, st, segs in rs + [(256, None, None)]:
        if y >= Y0 and not done:
            for yy in range(Y0, Y1 + 1):
                line = [bg] * 320
                for x, w, p in (orig_row(yy) or []):
                    for i, v in enumerate(p):
                        if x + i < 320: line[x + i] = v
                mrow = m[(yy - Y0) * 40:(yy - Y0 + 1) * 40]
                for x in range(320):
                    if line[x] == fg: line[x] = bg
                    if mrow[x >> 3] & (0x80 >> (x & 7)): line[x] = fg
                out.append(yy)
                for sx, last in ((0, 0), (160, 0x8000)):
                    data = rle(line[sx:sx + 160])
                    if len(data) > 255: return None
                    out += struct.pack('<HBB', sx | last, 160, len(data)) + data
            done = True
        if y == 256: break
        if Y0 <= y <= Y1: continue
        nxt = [r[1] for r in rs if r[1] > st]
        out += d[st:(min(nxt) if nxt else term)]
    out.append(0xff)
    if len(out) > size: return None
    return bytes(out) + bytes(size - len(out))

def splice2(fdata, m, fg, bg):
    """フレームのバイト列 → 字幕を差し替えたフレーム（大きさは変わってよい）"""
    rs, term = rows(fdata, 0)
    out = bytearray(fdata[:4])
    segs_of = {y: segs for y, st, segs in rs}
    starts = [st for y, st, segs in rs] + [term]
    done = False
    for i, (y, st, segs) in enumerate(rs + [(256, term, None)]):
        if y >= Y0 and not done:
            for yy in range(Y0, Y1 + 1):
                line = [bg] * 320
                for x, w, p in (segs_of.get(yy) or []):
                    for k, v in enumerate(p):
                        if x + k < 320: line[x + k] = v
                mrow = m[(yy - Y0) * 40:(yy - Y0 + 1) * 40]
                for x in range(320):
                    if line[x] == fg: line[x] = bg
                    if mrow[x >> 3] & (0x80 >> (x & 7)): line[x] = fg
                out.append(yy)
                for sx, last in ((0, 0), (160, 0x8000)):
                    data = rle(line[sx:sx + 160])
                    out += struct.pack('<HBB', sx | last, 160, len(data)) + data
            done = True
        if y == 256: break
        if Y0 <= y <= Y1: continue
        out += fdata[st:starts[i + 1]]
    out.append(0xff)
    return bytes(out)

def rebuild(d, acf, recs):
    """recs: {フレーム番号: (mask, fg, bg)} → (新しい BMA, 新しい ACF)"""
    out = bytearray(); b = 0; fi = 0; sizes = []
    while b + 6 <= len(d):
        ln, n = struct.unpack_from('<IH', d, b)
        if ln == 0: break
        offs = [struct.unpack_from('<I', d, b + 6 + 4 * k)[0] for k in range(n)] + [ln]
        assert offs[0] == 6 + 4 * n
        fr = []
        for k in range(n):
            f = d[b + offs[k]:b + offs[k + 1]]
            if fi in recs: f = splice2(f, *recs[fi])
            fr.append(f); fi += 1
        hdr = 6 + 4 * n; pos = hdr; o2 = []
        for f in fr: o2.append(pos); pos += len(f)
        blk = struct.pack('<IH', pos, n) + b''.join(struct.pack('<I', x) for x in o2) + b''.join(fr)
        sizes.append(len(blk)); out += blk; b += ln
    out += d[b:]
    a = bytearray(acf); i = 0; nb = 0
    while True:
        i = a.find(b'\x08\x05', i)
        if i < 0: break
        idx, cnt, sz = struct.unpack_from('<HHH', a, i + 2)
        if idx < len(sizes) and nb == idx:
            assert sizes[idx] < 65536
            struct.pack_into('<H', a, i + 6, sizes[idx]); nb += 1; i += 8
        else: i += 1
    assert nb == len(sizes), (nb, len(sizes))
    return bytes(out), bytes(a), sizes
