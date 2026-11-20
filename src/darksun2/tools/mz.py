# DSUN.EXE（MZ + Borland VROOMM オーバーレイ FBOV）の情報
#   python -X utf8 tools/mz.py               ヘッダ・再配置の数
#   python -X utf8 tools/mz.py check OFF..   イメージ上の位置（16 進）が再配置で書き換わる語に掛かるか
import struct, sys, os
EXE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'test', 'game', 'RAVAGER', 'DSUN.EXE'))

def load(path=EXE):
    d = open(path, 'rb').read()
    cblp, cp, crlc, hdr = struct.unpack_from('<4H', d, 2)
    lfarlc = struct.unpack_from('<H', d, 0x18)[0]
    relocs = set()
    for k in range(crlc):
        off, seg = struct.unpack_from('<HH', d, lfarlc + 4 * k)
        relocs.add(seg * 16 + off)
    return d, hdr * 16, relocs

def touches(relocs, a, n):
    """イメージ上の [a, a+n) が再配置の語（2 バイト）に重なるか"""
    return any(r in relocs or r + 1 in relocs for r in range(a - 1, a + n))

if __name__ == '__main__':
    d, hb, relocs = load()
    if len(sys.argv) > 2 and sys.argv[1] == 'check':
        for s in sys.argv[2:]:
            a = int(s, 16)
            hit = sorted(r for r in relocs if a - 1 <= r < a + 8)
            print(hex(a), 'reloc at', [hex(r) for r in hit])
    else:
        print('header', hex(hb), 'relocs', len(relocs))
