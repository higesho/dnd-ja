# Build build/patch.bin (embedded into winmm.dll).   python -X utf8 tools/make_patch.py
import sys, os, json, struct, zlib, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'FLAME')
DH = os.path.join(ROOT, '..', '..', '..', '..', 'Forgotten Realms The Archives - Collection Three', 'games', 'Dungeon Hack-ENG', '_jp', 'tools')
sys.path.insert(0, DH)
import make_patch as dhm          # misaki loader, NO_START / NO_END, norm()
CB_BASE = 0x70
LOG = int(os.environ.get('DFJP_LOG', '0'))
WIDTH = 32                        # character cells per line in the message boxes (a Japanese character = 1 cell)

def read_lines(p):
    out = {}
    if os.path.exists(p):
        for line in open(p, encoding='utf-8-sig'):
            m = re.match(r'^\[(\d+)\] ?(.*?)\r?$', line.rstrip('\n'))
            if m: out[int(m.group(1))] = m.group(2)
    return out

def cls(c):
    o = ord(c)
    if 0x3041 <= o <= 0x309f: return 'h'
    if 0x30a0 <= o <= 0x30ff: return 'k'
    if 0x4e00 <= o <= 0x9fff: return 'K'
    return 'p'

def wrap(s, width=WIDTH):
    """greedy wrap with kinsoku; prefers breaks after punctuation / at kana-kanji boundaries"""
    lines = []
    for para in s.split('\n'):
        para = para.strip()
        while len(para) > width:
            best = None
            for cut in range(width, max(1, width - 8), -1):
                a, b = para[cut - 1], para[cut]
                if b in dhm.NO_START or a in dhm.NO_END: continue
                if a.isascii() and b.isascii() and a != ' ' and b != ' ': continue
                score = 0 if a in '、。！？」' or a == ' ' else (1 if cls(a) == 'h' and cls(b) != 'h' else 3)
                if best is None or score < best[0]: best = (score, cut)
                if score == 0: break
            cut = best[1] if best else width
            lines.append(para[:cut].rstrip()); para = para[cut:].lstrip()
        lines.append(para)
    return lines

def centre(lines, width=34):
    return '\r'.join(' ' * max(0, (width - len(l)) // 2) + l for l in lines)

def main():
    g8 = dhm.load_bdf(dhm.BDF8, 8)
    g8[0x30a0] = bytes([0, 0, 0, 0x3c, 0, 0x3c, 0, 0])   # ゠ (ケ゠シュ): short bars; misaki ＝ / ASCII = look like ニ
    text_ja = {}
    exe_ja = {k: dhm.norm(v) for k, v in read_lines(os.path.join(ROOT, 'text', 'chunks', 'exe.ja.txt')).items()}
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(fx):
        f = json.load(open(fx, encoding='utf-8'))
        text_ja.update({int(k): v for k, v in f.get('text', {}).items()}); exe_ja.update({int(k): v for k, v in f.get('exe', {}).items()})
        packs = f.get('pack', [])
    else:
        packs = []
    allja = list(text_ja.values()) + list(exe_ja.values())
    chars = sorted({c for s in allja for c in s if ord(c) >= 0x80} | set('「」。、'))
    code = {c: ((0x80 + k // 64) << 8) | (0xc0 + k % 64) for k, c in enumerate(chars)}
    missing = [c for c in chars if ord(c) not in g8]
    if missing: print('no glyph:', ''.join(missing))
    def enc(s):
        b = bytearray()
        for c in s:
            if ord(c) < 0x80: b.append(ord(c))
            else: v = code[c]; b += bytes([v >> 8, v & 0xff])
        return bytes(b)
    sections = []
    def sect(t, name, payload):
        sections.append(struct.pack('<II', ord(t), len(payload) + 16) + name.encode().ljust(16, b'\0') + payload)
    sections.append(struct.pack('<II', ord('P'), 8) + struct.pack('<II', CB_BASE, LOG))
    n = len(chars)
    payload = struct.pack('<I', n) + b''.join(struct.pack('<H', code[c]) for c in chars) + \
        b''.join(g8.get(ord(c), bytes(8)) for c in chars) + bytes(n) + bytes(28 * n) + bytes(8 * n)
    sections.append(struct.pack('<II', ord('F'), len(payload)) + payload)
    # SHADOW.COM: callback in the string printer
    com = open(os.path.join(GAME, 'START.COM'), 'rb').read()
    off = 0x1529 - 0x100
    assert com[off:off + 4] == bytes.fromhex('1e562c20')
    sect('X', 'START.COM', struct.pack('<II', len(com), zlib.crc32(com) & 0xffffffff) +
         struct.pack('<II', off, 4) + bytes([0xfe, 0x38, CB_BASE, 0]))
    # GAME.EXE: UI strings in place
    exe = open(os.path.join(GAME, 'GAME.EXE'), 'rb').read()
    items = {x['id']: x for x in json.load(open(os.path.join(ROOT, 'text', 'en', 'exe.json'), encoding='utf8'))}
    xp = struct.pack('<II', len(exe), zlib.crc32(exe) & 0xffffffff)
    packed = set()
    for pk in packs:     # consecutive strings reached through a pointer table: repack them so short ones can grow
        ids = list(range(pk['ids'][0], pk['ids'][1] + 1))
        lo, hi, dsb = int(pk['table'][0], 16), int(pk['table'][1], 16), int(pk['ds'], 16)
        r0 = items[ids[0]]['off']; r1 = items[ids[-1]]['off'] + items[ids[-1]]['room'] + 1
        buf = bytearray(); ptrs = []
        for i in ids:
            x = items[i]
            assert exe[x['off']:x['off'] + x['room']] == x['en'].encode('latin1')
            b = enc(exe_ja.get(i, x['en']).replace('\\r', '\r'))
            ptrs.append((x['off'], r0 + len(buf))); buf += b + b'\0'
        assert len(buf) <= r1 - r0, 'pack overflow %d > %d' % (len(buf), r1 - r0)
        xp += struct.pack('<II', r0, r1 - r0) + bytes(buf) + bytes(r1 - r0 - len(buf))
        for old, new in ptrs:
            if old == new: continue
            ow = struct.pack('<H', old - dsb)
            hits = [k for k in range(lo, hi, 2) if exe[k:k + 2] == ow]
            assert len(hits) == 1, (hex(old), hits)
            xp += struct.pack('<II', hits[0], 2) + struct.pack('<H', new - dsb)
        packed |= set(ids)
    for i, s in sorted(exe_ja.items()):
        x = items.get(i)
        if not x or i in packed: continue
        b = enc(s.replace('\\r', '\r'))
        if len(b) > x['room']:
            print('too long [%d] %s' % (i, s)); continue
        assert exe[x['off']:x['off'] + x['room']] == x['en'].encode('latin1')
        xp += struct.pack('<II', x['off'], x['room'] + 1) + b + bytes(x['room'] + 1 - len(b))
    sect('X', 'GAME.EXE', xp)
    blob = b'DFJP' + struct.pack('<II', 1, len(sections)) + b''.join(sections)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('GAME.EXE %d strings; patch.bin %d bytes, %d chars' % (len(exe_ja), len(blob), n))

if __name__ == '__main__':
    main()
