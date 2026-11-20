import pefile, capstone, struct, re
import os
P=pefile.PE(os.path.join(os.path.dirname(__file__),'..','..','DOSBOX','DOSBox.exe'))
D=P.__data__
IB=P.OPTIONAL_HEADER.ImageBase
def off2va(o):
    for s in P.sections:
        if s.PointerToRawData<=o<s.PointerToRawData+s.SizeOfRawData: return IB+s.VirtualAddress+o-s.PointerToRawData
def va2off(v):
    return P.get_offset_from_rva(v-IB)
md=capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
def dis(va,n=60):
    o=va2off(va); out=[]
    for i in md.disasm(D[o:o+n*8],va):
        out.append('%08x: %-24s %s %s'%(i.address,i.bytes.hex(),i.mnemonic,i.op_str))
        if len(out)>=n: break
    return '\n'.join(out)
def find_imm(v):
    b=struct.pack('<I',v)
    return [off2va(m.start()) for m in re.finditer(re.escape(b),D)]
def sva(s):
    return [off2va(m.start()) for m in re.finditer(re.escape(s),D)]
