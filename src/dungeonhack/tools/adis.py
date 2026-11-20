# disassemble AESOP.EXE (16-bit MZ). usage: adis.py IMGOFF [LEN] [SEG]   (IMGOFF = load image offset, hex)
import sys, capstone, re, struct
import os
d = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'orig', 'AESOP.EXE'), 'rb').read()
hdr = struct.unpack_from('<H', d, 8)[0] * 16
img = d[hdr:]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
if __name__ == '__main__':
    a = int(sys.argv[1], 16); n = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x100
    seg = int(sys.argv[3], 16) if len(sys.argv) > 3 else (a >> 4)
    base = seg * 16
    for i in md.disasm(img[a:a + n], a - base):
        s = i.op_str
        if i.mnemonic == 'lcall':
            m = re.match(r'(0x[0-9a-f]+|\d+), (0x[0-9a-f]+|\d+)', s)
            if m: s += '   ; -> %x' % (int(m.group(1), 0) * 16 + int(m.group(2), 0))
        print('%05x %04x: %-14s %s %s' % (i.address + base, i.address, i.bytes.hex(), i.mnemonic, s))
