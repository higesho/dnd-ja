# disassemble CODE.2 LE code object. usage: dis.py ADDR [LEN]   (link address, code object at 0x10000)
import sys, os, capstone
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import le
L = le.LE()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
a = int(sys.argv[1], 16); n = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x100
obj = 0 if a < L.objs[1]['base'] else 1
base = L.objs[obj]['base']
c = bytes(L.raw[obj])
for i in md.disasm(c[a - base:a - base + n], a):
    print('%x: %-16s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
