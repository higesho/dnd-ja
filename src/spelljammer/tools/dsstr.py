# Strings in the data segment (DGROUP) of an EXE -> build/<exe>_strings.txt  ("dsoff string")
import re, sys, os, struct
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'SPELLJAM')
for name in sys.argv[1:]:
    d = open(os.path.join(GAME, name), 'rb').read()
    hdr = struct.unpack_from('<H', d, 8)[0] * 16
    pages, last = struct.unpack_from('<HH', d, 4)[1], struct.unpack_from('<H', d, 2)[0]
    end = (pages - 1) * 512 + last if last else pages * 512
    ip, cs = struct.unpack_from('<HH', d, 0x14)
    # Borland startup: mov dx, DGROUP
    ent = hdr + cs * 16 + ip
    assert d[ent] == 0xba
    dseg = struct.unpack_from('<H', d, ent + 1)[0]
    ds = hdr + dseg * 16
    out = ['; DGROUP seg %x file %x' % (dseg, ds)]
    for m in re.finditer(rb'[\x20-\x7e]{2,}\x00', d[ds:end]):
        out.append('%04x %s' % (m.start(), m.group()[:-1].decode()))
    open(os.path.join(ROOT, 'build', name + '_strings.txt'), 'w').write('\n'.join(out))
    print(name, hex(ds), len(out))
