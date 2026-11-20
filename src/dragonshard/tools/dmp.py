# print exception code/address and stack return addresses from the game's minidumps, then delete them
import struct, glob, os, sys
G = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
for f in glob.glob(os.path.join(G, '*.dmp')):
    d = open(f, 'rb').read()
    n, rva = struct.unpack_from('<II', d, 8)
    for i in range(n):
        t, sz, r = struct.unpack_from('<III', d, rva + i * 12)
        if t == 6:
            tid = struct.unpack_from('<I', d, r)[0]
            code, flags, rec, addr = struct.unpack_from('<IIQQ', d, r + 8)
            info = struct.unpack_from('<2Q', d, r + 40)
            print(os.path.basename(f), 'tid', tid, hex(code), 'at', hex(addr), [hex(x) for x in info])
    if '-k' not in sys.argv: os.remove(f)
