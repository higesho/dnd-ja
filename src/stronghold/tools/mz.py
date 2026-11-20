# Borland 実モード MZ（STRONG.EXE）: ロードイメージと再配置、16 ビットの逆アセンブル
import struct, os, capstone
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
EXE = os.path.join(ROOT, '..', 'GAME', 'STRONG', 'STRONG.EXE')
class MZ:
    def __init__(self, fn=EXE):
        d = self.d = open(fn, 'rb').read()
        cblp, cp, crlc, hdr, mn, mx, ss, sp, ck, ip, cs, lfarlc = struct.unpack_from('<12H', d, 2)
        self.hdr = hdr * 16
        self.size = (cp - 1) * 512 + (cblp or 512)
        self.img = d[self.hdr:self.size]
        self.ip, self.cs, self.ss, self.sp = ip, cs, ss, sp
        self.relocs = [struct.unpack_from('<HH', d, lfarlc + 4 * i) for i in range(crlc)]
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    def lin(self, seg, off): return seg * 16 + off
    def dis(self, seg, off, n=40):
        a = self.lin(seg, off)
        out = []
        for ins in self.md.disasm(self.img[a:a + n * 6], off):
            out.append('%04x:%04x  %-16s %s %s' % (seg, ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))
            if len(out) >= n: break
        return '\n'.join(out)
if __name__ == '__main__':
    import sys
    m = MZ()
    if len(sys.argv) > 2: print(m.dis(int(sys.argv[1], 16), int(sys.argv[2], 16), int(sys.argv[3]) if len(sys.argv) > 3 else 40))
    else:
        print('hdr %x size %x img %x cs:ip %x:%x ss:sp %x:%x relocs %d' % (m.hdr, m.size, len(m.img), m.cs, m.ip, m.ss, m.sp, len(m.relocs)))
        print(m.dis(m.cs, m.ip, 30))

# オーバーレイ（Borland VROOMM）: 段の表（ファイル 0x14570、8 バイト）とスタブ（INT 3Fh）
OVBASE = 0x1f430
def seltab(m):
    return [struct.unpack_from('<4H', m.d, 0x14570 + 8 * i) for i in range(0x5b)]
def stub(m, seg):
    a = seg * 16
    fo, cs, rs, ne = struct.unpack_from('<IHHH', m.img, a + 4)
    ents = [struct.unpack_from('<H', m.img, a + 0x20 + 5 * k + 2)[0] for k in range(ne)]
    return fo, cs, rs, ents
def resolve(m, sel, off):
    """オーバーレイの far call（段の表の番号:オフセット）→ (ファイル位置, 説明)"""
    seg = seltab(m)[sel // 8][0]
    if seltab(m)[sel // 8][2] & 2:          # スタブ
        fo, cs, rs, ents = stub(m, seg)
        k = (off - 0x20) // 5
        return OVBASE + fo + ents[k], 'ov %x+%x' % (fo, ents[k])
    return m.hdr + seg * 16 + off, 'main %x:%x' % (seg, off)
def ovl_of(m, fileoff):
    for sel, (seg, mx, fl, mn) in enumerate(seltab(m)):
        if fl & 2:
            fo, cs, rs, ents = stub(m, seg)
            if OVBASE + fo <= fileoff < OVBASE + fo + cs: return sel * 8, seg, fo, cs, rs
def disf(m, fo, n=40):
    out = []
    for ins in m.md.disasm(m.d[fo:fo + n * 6], fo):
        out.append('%06x  %-16s %s %s' % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))
        if len(out) >= n: break
    return '\n'.join(out)
