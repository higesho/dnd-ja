import sys, capstone
d=open(sys.argv[1],'rb').read()
s=int(sys.argv[2],16); e=int(sys.argv[3],16); base=int(sys.argv[4],16) if len(sys.argv)>4 else s
md=capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
for i in md.disasm(d[s:e], base):
    print('%05x(%05x): %-20s %s %s'%(i.address, i.address-base+s, i.bytes.hex(), i.mnemonic, i.op_str))
