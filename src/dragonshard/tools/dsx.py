import sys, struct, re, pefile, capstone
EXE = r'L:\SteamLibrary\steamapps\common\Dungeons & Dragons Dragonshard\Dragonshard.exe'
pe = pefile.PE(EXE, fast_load=True)
data = open(EXE,'rb').read()
IB = pe.OPTIONAL_HEADER.ImageBase
def off2va(o):
    for s in pe.sections:
        if s.PointerToRawData <= o < s.PointerToRawData+s.SizeOfRawData: return o - s.PointerToRawData + s.VirtualAddress + IB
def va2off(v):
    r = v-IB
    for s in pe.sections:
        if s.VirtualAddress <= r < s.VirtualAddress+s.SizeOfRawData: return r - s.VirtualAddress + s.PointerToRawData
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = False
TEXT = pe.sections[0]
def xrefs(va):
    b = struct.pack('<I', va)
    out=[]
    for m in re.finditer(re.escape(b), data[TEXT.PointerToRawData:TEXT.PointerToRawData+TEXT.SizeOfRawData]):
        out.append(off2va(m.start()+TEXT.PointerToRawData))
    return out
def calls_to(va):
    t0 = TEXT.PointerToRawData; t = data[t0:t0+TEXT.SizeOfRawData]
    out=[]
    for m in re.finditer(b'\xe8', t):
        o=m.start(); rel=struct.unpack_from('<i',t,o+1)[0] if o+5<=len(t) else 0
        src=off2va(o+t0)
        if src+5+rel==va: out.append(src)
    return out
def dis(va, n=40):
    o=va2off(va)
    for i,ins in enumerate(md.disasm(data[o:o+n*8], va)):
        if i>=n: break
        print('%08x: %-24s %s %s'%(ins.address, data[va2off(ins.address):va2off(ins.address)+ins.size].hex(), ins.mnemonic, ins.op_str))
def findstr(s, wide=False):
    b = s.encode('utf-16-le') if wide else s.encode()
    return [off2va(m.start()) for m in re.finditer(re.escape(b), data)]
def funcstart(va):
    # scan back for typical prologue after int3/ret padding
    o=va2off(va)
    while o>0:
        if data[o-1] in (0xcc,0x90,0xc3) and data[o] in (0x55,0x83,0x81,0x6a,0x56,0x53,0x57,0x8b,0x64,0xa1,0x51):
            if data[o-1]!=0xc3 or True: return off2va(o)
        o-=1
if __name__=='__main__':
    cmd=sys.argv[1]
    if cmd=='d': dis(int(sys.argv[2],16), int(sys.argv[3]) if len(sys.argv)>3 else 40)
    elif cmd=='x':
        for a in xrefs(int(sys.argv[2],16)): print(hex(a))
    elif cmd=='c':
        for a in calls_to(int(sys.argv[2],16)): print(hex(a))
    elif cmd=='s':
        for a in findstr(sys.argv[2], len(sys.argv)>3): print(hex(a), [hex(x) for x in xrefs(a)])
