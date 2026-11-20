# Linear 16-bit disassembly of an MZ EXE (whole file after the header) -> build/dis/<name>.txt
# addresses are file offsets
import sys, os, struct, capstone
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'DS')
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
md.skipdata = True
for name in sys.argv[1:]:
    d = open(os.path.join(GAME, name), 'rb').read()
    hdr = struct.unpack_from('<H', d, 8)[0] * 16
    pages, last = struct.unpack_from('<HH', d, 4)[::-1][::-1]
    out = open(os.path.join(ROOT, 'build', 'dis', name + '.txt'), 'w')
    out.write('; header %x, image end %x, file %x\n' % (hdr, (pages - 1) * 512 + last if last else pages * 512, len(d)))
    for ins in md.disasm(d[hdr:], hdr):
        out.write('%05x: %-20s %s %s\n' % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))
    out.close()
