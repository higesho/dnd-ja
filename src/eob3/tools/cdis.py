# disassemble CINE.EXE (16-bit MZ). usage: cdis.py LINEAR [LEN]  (linear = image offset)
import sys, capstone, re
d = open(__file__.replace('tools\cdis.py','').replace('tools/cdis.py','') + '../GAME/EOB3/CINE.EXE', 'rb').read()[0x2000:]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
a = int(sys.argv[1], 16); n = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x100
seg = int(sys.argv[3], 16) if len(sys.argv) > 3 else (a >> 4)
base = seg * 16
for i in md.disasm(d[a:a + n], a - base):
    s = i.op_str
    if i.mnemonic == 'lcall':
        m = re.match(r'(0x[0-9a-f]+|\d+), (0x[0-9a-f]+|\d+)', s)
        if m: s += '   ; -> %x' % (int(m.group(1), 0) * 16 + int(m.group(2), 0))
    print('%05x %04x: %s %s' % (i.address + base, i.address, i.mnemonic, s))
