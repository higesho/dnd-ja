# デザインのマップファイル（GEOnnn.DAT）の文章を取り出して、日本語の符号を戻して表示する
#   python -X utf8 tools/dump_text.py test/game/UA/TUTORIAL.DSN/GEO005.DAT
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
import uajp

def unpack6(b):
    out = []; bits = 0; nb = 0
    for x in b:
        bits = (bits << 8) | x; nb += 8
        while nb >= 6:
            nb -= 6; out.append((bits >> nb) & 0x3f)
    return out

def chunks(d):
    p = 16; r = {}
    while p + 8 <= len(d):
        tag = d[p:p + 4]; n = struct.unpack('>I', d[p + 4:p + 8])[0]
        r[tag] = d[p + 8:p + 8 + n]; p += 8 + n + (n & 1)
    return r

def strings(path):
    s = chunks(open(path, 'rb').read())[b'STRG']
    tab = s[6:6 + 400]; body = s[0x196:]; p = 0; out = []
    for i, l in enumerate(tab):
        if l:
            v = unpack6(body[p:p + l])
            while v and v[-1] == 0: v.pop()
            out.append((i, l, ''.join(chr(x + 0x40) if x <= 0x1f else chr(x) for x in v)))
        p += l
    return out

if __name__ == '__main__':
    for i, l, t in strings(sys.argv[1]):
        print('%3d %3d bytes %3d symbols  %s' % (i, l, len(t), t))
        d = uajp.decode(t)
        if d != t: print('      -> ' + d)
