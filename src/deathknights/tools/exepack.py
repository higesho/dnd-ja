# Microsoft EXEPACK を展開して、普通の MZ（展開後のイメージ＋再配置表）を返す
import struct, sys
def unpack(exe):
    h = struct.unpack_from('<14H', exe)
    hdr = h[4] * 16
    cs, ip = h[11], h[10]
    eh = hdr + cs * 16
    rIP, rCS, _, epsize, rSP, rSS, dlen, skip, sig = struct.unpack_from('<8H2s', exe, eh)
    assert sig == b'RB', sig
    packed = bytearray(exe[hdr:eh])
    out = bytearray(dlen * 16)
    out[:len(packed)] = packed
    s = len(packed) - 1
    while out[s] == 0xff and s > len(packed) - 17: s -= 1
    d = len(out) - 1
    while True:
        cmd = out[s]; n = out[s - 2] | out[s - 1] << 8; s -= 3
        if cmd & 0xfe == 0xb0:
            fb = out[s]; s -= 1
            for k in range(n): out[d] = fb; d -= 1
        elif cmd & 0xfe == 0xb2:
            for k in range(n): out[d] = out[s]; d -= 1; s -= 1
        else: raise ValueError('bad cmd %x' % cmd)
        if cmd & 1: break
    # 再配置表（展開器の後ろ, cs:12d から 16 区分）
    p = eh + 0x12d
    rel = []
    for seg in range(16):
        cnt = struct.unpack_from('<H', exe, p)[0]; p += 2
        for k in range(cnt):
            off = struct.unpack_from('<H', exe, p)[0]; p += 2
            rel.append((off, seg * 0x1000))
    return bytes(out), rel, (rIP, rCS, rSP, rSS)

def to_mz(img, rel, regs, minalloc_par=0x1000):
    rIP, rCS, rSP, rSS = regs
    hdr_par = (28 + 4 * len(rel) + 15) // 16
    total = hdr_par * 16 + len(img)
    h = struct.pack('<14H', 0x5a4d, total % 512, (total + 511) // 512, len(rel), hdr_par, minalloc_par, 0xffff, rSS, rSP, 0, rIP, rCS, 28, 0)
    rt = b''.join(struct.pack('<HH', o, s) for o, s in rel)
    head = (h + rt).ljust(hdr_par * 16, b'\0')
    return head + img

if __name__ == '__main__':
    exe = open(sys.argv[1], 'rb').read()
    img, rel, regs = unpack(exe)
    print('image', hex(len(img)), 'relocs', len(rel), 'regs', [hex(x) for x in regs])
    if len(sys.argv) > 2:
        open(sys.argv[2], 'wb').write(img)

def is_packed(exe):
    h = struct.unpack_from('<14H', exe)
    hdr = h[4] * 16
    eh = hdr + h[11] * 16
    return exe[eh + 16:eh + 18] == b'RB' and exe[eh + h[10]:eh + h[10] + 5] == bytes.fromhex('8cc0051000')

def load(exe):
    """EXEPACK なら展開、そうでなければ MZ のイメージ・再配置表・入口をそのまま返す"""
    h = struct.unpack_from('<14H', exe)
    hdr = h[4] * 16
    if is_packed(exe):
        return unpack(exe)
    n, at = h[3], h[12]
    rel = [struct.unpack_from('<HH', exe, at + 4 * k) for k in range(n)]
    return exe[hdr:], rel, (h[10], h[11], h[8], h[7])
