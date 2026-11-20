# disassemble START.EXE (file offsets) around a location, from the previous 'push bp; mov bp,sp'
import sys, capstone, os
D = open(os.path.join(os.path.dirname(__file__), '..', 'orig', sys.argv[3] if len(sys.argv) > 3 else 'START.EXE'), 'rb').read()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
def func_start(o):
    s = D.rfind(b'\x55\x8b\xec', 0, o + 1)
    return s
def dis(s, n=200, stop_ret=True):
    out = []
    for i in md.disasm(D[s:s + n * 6], s):
        out.append('%05x: %-14s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
        if len(out) >= n: break
        if stop_ret and i.mnemonic in ('retf', 'ret') and i.address > s + 4 and len(out) > 3:
            nxt = D[i.address + i.size:i.address + i.size + 3]
            if nxt == b'\x55\x8b\xec': break
    return '\n'.join(out)
if __name__ == '__main__':
    o = int(sys.argv[1], 16)
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    print(dis(func_start(o), n))
