# disassemble DemonStone.exe .text -> build/dis.txt ; strings xref helper
import pefile, capstone, sys, os
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
pe = pefile.PE(os.path.join(ROOT, '..', 'DemonStone.exe'))
t = pe.sections[0]; base = pe.OPTIONAL_HEADER.ImageBase
code = t.get_data(); va0 = base + t.VirtualAddress
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
out = []; p = 0
while p < len(code):
    n = 0
    for i in md.disasm(code[p:], va0 + p):
        out.append('%08x: %-20s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str)); n += i.size
    p += n
    if p < len(code): out.append('%08x: %02x ??' % (va0 + p, code[p])); p += 1
open(os.path.join(ROOT, 'build', 'dis.txt'), 'w').write('\n'.join(out))
print(len(out))
