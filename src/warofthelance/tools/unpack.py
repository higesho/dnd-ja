# EXEPACK unpacker for START.EXE -> returns (image bytes, relocations[(seg,off)], ip, cs, sp, ss)
import struct, os
def unpack(d):
    h = struct.unpack_from('<14H', d, 0)
    hdr = h[4] * 16
    cs, ip = h[11], h[10]
    eh = hdr + cs * 16
    rIP, rCS, mstart, xsize, rSP, rSS, dlen, sig = struct.unpack_from('<8H', d, eh)
    assert sig == 0x4252
    packed = d[hdr:eh]
    buf = bytearray(max(dlen * 16, len(packed)))
    buf[:len(packed)] = packed
    si = len(packed) - 1
    k = 0
    while buf[si] == 0xff and k < 16: si -= 1; k += 1
    di = dlen * 16 - 1
    while True:
        cmd = buf[si]; si -= 1
        si -= 1
        cx = buf[si] | (buf[si + 1] << 8); si -= 2
        si += 1
        if cmd & 0xfe == 0xb0:
            v = buf[si]; si -= 1
            for _ in range(cx): buf[di] = v; di -= 1
        elif cmd & 0xfe == 0xb2:
            for _ in range(cx): buf[di] = buf[si]; di -= 1; si -= 1
        else:
            raise ValueError('bad cmd %x' % cmd)
        if cmd & 1: break
    # relocations: table after the error message, 16 groups
    t = d.index(b'Packed file is corrupt', eh) + len(b'Packed file is corrupt')
    rel = []
    for g in range(16):
        n = struct.unpack_from('<H', d, t)[0]; t += 2
        for i in range(n):
            rel.append((g * 0x1000, struct.unpack_from('<H', d, t)[0])); t += 2
    return bytes(buf[:dlen * 16]), rel, rIP, rCS, rSP, rSS

def build_mz(img, rel, ip, cs, sp, ss, minalloc, maxalloc):
    nrel = len(rel)
    hdrsize = (0x1c + nrel * 4 + 15) // 16 * 16
    total = hdrsize + len(img)
    h = struct.pack('<14H', 0x5a4d, total % 512, (total + 511) // 512, nrel, hdrsize // 16, minalloc, maxalloc,
                    ss, sp, 0, ip, cs, 0x1c, 0)
    r = b''.join(struct.pack('<HH', o, s) for s, o in rel)
    return (h + r).ljust(hdrsize, b'\0') + img

if __name__ == '__main__':
    ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    d = open(os.path.join(ROOT, '..', 'GAME', 'WL', 'START.EXE'), 'rb').read()
    img, rel, ip, cs, sp, ss = unpack(d)
    print(len(img), len(rel), hex(ip), hex(cs), hex(sp), hex(ss))
    print(img.find(b'------------------------'), img.find(b'PLAYING ON HARD DISK'))
    open(os.path.join(ROOT, 'build', 'unpacked.bin'), 'wb').write(img)
