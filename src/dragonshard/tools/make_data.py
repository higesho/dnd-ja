# build/tr.json -> build/tr.bin (resource DSJP/1) + build/chars.bin (DSJP/2) + build/chars.txt
import os, json, struct
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
B = os.path.join(ROOT, 'build')


def fnv64(b):
    h = 0xcbf29ce484222325
    for c in b:
        h ^= c
        h = (h * 0x100000001b3) & 0xffffffffffffffff
    return h


def game(s):
    """translator text -> game text (line break code U+FB00, typographic fixes)"""
    s = s.replace('\r\n', '\n').replace('\n', 'ﬀ')
    s = s.replace('~', '～').replace('―', '—').replace('－', '-')
    return s


def main():
    tr = json.load(open(os.path.join(B, 'tr.json'), encoding='utf-8'))
    ents, blob, chars = [], bytearray(), set()
    base = 4
    items = []
    for en, ja in tr.items():
        k = fnv64(game_en(en).encode('utf-16-le'))
        j = game(ja)
        items.append((k, j))
        chars.update(c for c in j if ord(c) >= 0x80)
    items.sort()
    seen = set()
    uniq = []
    for k, j in items:
        if k in seen: continue
        seen.add(k); uniq.append((k, j))
    hdr = 4 + len(uniq) * 12
    out = bytearray(struct.pack('<I', len(uniq)))
    off = hdr
    for k, j in uniq:
        b = (j + '\0').encode('utf-16-le')
        out += struct.pack('<QI', k, off + len(blob))
        blob += b
    out += blob
    open(os.path.join(B, 'tr.bin'), 'wb').write(out)
    # charset: translation + kana / punctuation / full-width forms the player may type or see
    for a, b in ((0x3000, 0x30ff), (0xff01, 0xff5e)):
        chars.update(chr(c) for c in range(a, b + 1))
    chars = sorted(c for c in chars if 0x80 <= ord(c) < 0xfb00 or 0xff00 <= ord(c) <= 0xffef)
    open(os.path.join(B, 'chars.bin'), 'wb').write(struct.pack('<I', len(chars)) + b''.join(struct.pack('<H', ord(c)) for c in chars))
    open(os.path.join(B, 'chars.txt'), 'w', encoding='utf-8').write(''.join(chars))
    print(len(uniq), 'strings', len(out), 'bytes;', len(chars), 'chars')


def game_en(s):
    return s.replace('\n', 'ﬀ')


if __name__ == '__main__':
    main()
