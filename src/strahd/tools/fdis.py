# disassemble a file region by FILE offset (32-bit). usage: fdis.py FILE OFF [N]
import sys, capstone
d = open(sys.argv[1], 'rb').read()
a = int(sys.argv[2], 16); n = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x100
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
for i in md.disasm(d[a:a + n], a):
    print('%x: %-16s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
