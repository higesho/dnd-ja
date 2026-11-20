import pefile,capstone,sys
pe=pefile.PE('../temple.exe',fast_load=True); img=pe.get_memory_mapped_image(); B=0x400000
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
a=int(sys.argv[1],16); n=int(sys.argv[2]) if len(sys.argv)>2 else 60
for i in md.disasm(img[a-B:a-B+n*8],a):
    print(hex(i.address),i.mnemonic,i.op_str); n-=1
    if n<=0: break
