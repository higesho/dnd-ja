# DragonStrike の文章（GAME.TXT など）: Westwood の 2 文字辞書圧縮
#   0x80 以上の 1 バイト c が 2 文字 T1[(c&0x7f)>>3] + T2[c&0x7f] になる。表は MAIN.EXE のファイル位置 0x200（T1 16 字）と 0x210（T2 128 字）
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'DS')
_d = open(os.path.join(GAME, 'MAIN.EXE'), 'rb').read()
T1 = _d[0x200:0x210].decode('latin1')
T2 = _d[0x210:0x290].decode('latin1')


def decode(b):
    out = []
    for c in b:
        if c & 0x80:
            c &= 0x7f
            out.append(T1[c >> 3]); out.append(T2[c])
        else:
            out.append(chr(c))
    return ''.join(out)


if __name__ == '__main__':
    import sys
    d = open(sys.argv[1], 'rb').read()
    print(decode(d)[:int(sys.argv[2]) if len(sys.argv) > 2 else 3000])
