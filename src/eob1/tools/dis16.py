# 16bit disassembly of guest memory dump: dis16.py dump.bin SEG:OFF [count] [SEG:END]
import sys, capstone, os
d = open(sys.argv[1], 'rb').read()
seg, off = [int(x, 16) for x in sys.argv[2].split(':')]
n = int(sys.argv[3]) if len(sys.argv) > 3 else 80
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
lin = seg * 16 + off
k = 0
for i in md.disasm(d[lin:lin + n * 8], off):
    print('%04x:%04x  %-18s %s %s' % (seg, i.address, i.bytes.hex(), i.mnemonic, i.op_str))
    k += 1
    if k >= n: break
