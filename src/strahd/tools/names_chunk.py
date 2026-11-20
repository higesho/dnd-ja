# OBJECTS.DAT (u32 n, n * 65-byte records, name at +0) and NPCS.DAT (u32 n, n * 69-byte records, name at +0, 22 bytes)
# -> text/names/names.en.txt "[key] (max N bytes) NAME" with unique names; key = O:<name> / N:<name>
import json, os, struct
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GAME = os.path.join(ROOT, '..', 'GAME', 'RLOFT')
FILES = (('OBJECTS.DAT', 65, 38, 'O'), ('NPCS.DAT', 69, 21, 'N'))

def records(fn, size):
    d = open(os.path.join(GAME, fn), 'rb').read()
    n = struct.unpack_from('<I', d, 0)[0]
    for i in range(n):
        o = 4 + i * size
        nm = d[o:d.index(b'\0', o)]
        if nm and all(32 <= c < 127 for c in nm):
            yield i, o, nm.decode('latin1')

if __name__ == '__main__':
    os.makedirs(os.path.join(ROOT, 'text', 'names'), exist_ok=True)
    seen = {}
    for fn, size, room, tag in FILES:
        for i, o, nm in records(fn, size):
            seen.setdefault(tag + ':' + nm, room)
    with open(os.path.join(ROOT, 'text', 'names', 'names.en.txt'), 'w', encoding='utf8') as f:
        for k, (key, room) in enumerate(sorted(seen.items())):
            f.write('[%s] (max %d bytes) %s\n' % (key, room, key[2:]))
    print(len(seen))
