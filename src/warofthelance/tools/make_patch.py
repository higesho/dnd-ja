# Build build/patch.bin (embedded into winmm.dll).   python -X utf8 tools/make_patch.py
#  - START.EXE is EXEPACKed: the DLL unpacks it ('U' section) and the records patch the plain MZ built by unpack.build_mz
#  - Japanese: lead 0x81-0xBF, trail 0xC0-0xFF (lead 0x80 is not used: data files store chars +0x80 and 0x00 ends them)
import sys, os, json, struct, zlib, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'WL')
DH = os.path.join(ROOT, '..', '..', '..', '..', 'Forgotten Realms The Archives - Collection Three', 'games', 'Dungeon Hack-ENG', '_jp', 'tools')
sys.path.insert(0, HERE)
sys.path.insert(0, DH)
import make_patch as dhm          # misaki loader, norm()
from unpack import unpack
CB_BASE = 0x70
LOG = int(os.environ.get('WLJP_LOG', '0'))
DSB = 0x58c40


def read_lines(p):
    out = {}
    if os.path.exists(p):
        for line in open(p, encoding='utf-8-sig'):
            m = re.match(r'^\[(\d+)\] ?(.*?)\r?$', line.rstrip('\n'))
            if m: out[int(m.group(1))] = m.group(2)
    return out


NO_START = set('、。，．・：；？！ー」』）】ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々!?.,')
NO_END = set('「『（【')


def cells(s):
    return len(s)


def wrap(s, width):
    out = []
    s = s.strip()
    while len(s) > width:
        cut = width
        while cut > width - 4 and (s[cut] in NO_START or s[cut - 1] in NO_END): cut -= 1
        if s[cut] in NO_START and cut == width - 4: cut = width
        out.append(s[:cut]); s = s[cut:]
    if s: out.append(s)
    return out


def main():
    g8 = dhm.load_bdf(dhm.BDF8, 8)
    g8[0x30a0] = bytes([0, 0, 0, 0x3c, 0, 0x3c, 0, 0])   # ゠ (ケ゠シュ): short bars; misaki ＝ / ASCII = look like ニ
    ch = os.path.join(ROOT, 'text', 'chunks')
    exe_ja = {k: dhm.norm(v) for k, v in read_lines(os.path.join(ch, 'exe.ja.txt')).items()}
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    f = json.load(open(fx, encoding='utf-8')) if os.path.exists(fx) else {}
    exe_ja.update({int(k): v for k, v in f.get('exe', {}).items()})
    dat_ja = {}            # file -> {index: text}
    for name in ('MENU', 'NAT', 'WL2'):
        t = read_lines(os.path.join(ch, name.lower() + '.ja.txt'))
        t.update({int(k): v for k, v in f.get(name.lower(), {}).items()})
        if t: dat_ja[name] = {k: dhm.norm(v) for k, v in t.items()}
    # Q1/Q2: messages ('units' of whole 36-char lines) wrapped again into the same lines, 18 cells each
    units = json.load(open(os.path.join(ROOT, 'text', 'en', 'q_units.json'), encoding='utf8'))
    qja = {}
    for fn in ('qa', 'qb'):
        qja.update(read_lines(os.path.join(ch, fn + '.ja.txt')))
    qja.update({int(k): v for k, v in f.get('q', {}).items()})
    for k, u in enumerate(units):
        if k not in qja: continue
        lines = wrap(dhm.norm(qja[k]), 18)
        if len(lines) > len(u['lines']):
            print('Q unit %d: %d lines > %d: %s' % (k, len(lines), len(u['lines']), qja[k])); continue
        lines += [''] * (len(u['lines']) - len(lines))
        for li, t in zip(u['lines'], lines):
            dat_ja.setdefault(u['file'], {})[li] = t
    allja = list(exe_ja.values()) + [v for d in dat_ja.values() for v in d.values()] + list(f.get('data', {}).values())
    chars = sorted({c for s in allja for c in s if ord(c) >= 0x80})
    code = {c: ((0x81 + k // 64) << 8) | (0xc0 + k % 64) for k, c in enumerate(chars)}
    missing = [c for c in chars if ord(c) not in g8]
    if missing: print('no glyph:', ''.join(missing))

    def enc(s):
        assert not re.search('[a-z]', s) or s.startswith('DAVID'), s   # lowercase = 7-bit Japanese lead in the DLL
        b = bytearray()
        for c in s:
            if ord(c) < 0x80: b.append(ord(c))
            else: v = code[c]; b += bytes([v >> 8, v & 0xff])
        return bytes(b)
    def enc7(s):          # data files keep 7 bits only: Japanese = lowercase lead a-z + trail 21h-7Eh
        b = bytearray()
        for c in s:
            if ord(c) < 0x80:
                assert not ('a' <= c <= 'z'), s
                b.append(ord(c))
            else:
                v = code[c]; k = ((v >> 8) - 0x81) * 64 + (v & 0xff) - 0xc0
                b += bytes([0x61 + k // 94, 0x21 + k % 94])
        return bytes(b)
    sections = []

    def sect(t, name, payload):
        sections.append(struct.pack('<II', ord(t), len(payload) + 16) + name.encode().ljust(16, b'\0') + payload)
    sections.append(struct.pack('<II', ord('P'), 8) + struct.pack('<II', CB_BASE, LOG))
    n = len(chars)
    payload = struct.pack('<I', n) + b''.join(struct.pack('<H', code[c]) for c in chars) + \
        b''.join(g8.get(ord(c), bytes(8)) for c in chars) + bytes(n) + bytes(28 * n) + bytes(8 * n)
    sections.append(struct.pack('<II', ord('F'), len(payload)) + payload)

    # START.EXE: strings in place in the unpacked image (len word of the descriptor updated)
    exe = open(os.path.join(GAME, 'START.EXE'), 'rb').read()
    img, rel, ip, cs, sp, ss = unpack(exe)
    hsize = (0x1c + len(rel) * 4 + 15) // 16 * 16
    items = {x['id']: x for x in json.load(open(os.path.join(ROOT, 'text', 'en', 'exe_all.json'), encoding='utf8'))}
    xp = struct.pack('<II', len(exe), zlib.crc32(exe) & 0xffffffff)
    cnt = 0
    for i, s in sorted(exe_ja.items()):
        x = items.get(i)
        if not x: continue
        b = enc(s)
        if len(b) > x['room']:
            print('too long [%d] %d>%d %s' % (i, len(b), x['room'], s)); continue
        assert img[x['off']:x['off'] + x['room']] == x['en'].encode('latin1')
        o = x['off'] - 4
        xp += struct.pack('<II', hsize + o, 4 + len(b)) + struct.pack('<HH', len(b), x['off'] - DSB) + b
        cnt += 1
    # DATA statements: records {u16 tag, text, 00} read by READ; same length (unquoted items: spaces are ignored)
    for tag, txt in f.get('data', {}).items():   # key = image offset (hex) of the record's tag word
        o = int(tag, 16)
        assert img[o - 1] == 0 and img[o + 2] == 0x20, tag
        e = img.index(bytes(1), o + 2)
        b = enc(' ' + txt)
        if len(b) > e - o - 2:
            print('DATA %s too long %d > %d' % (tag, len(b), e - o - 2)); continue
        xp += struct.pack('<II', hsize + o + 2, e - o - 2) + b.ljust(e - o - 2, b' ')
    sect('U', 'START.EXE', xp)

    # data files (BSAVE: FD seg off len, then chars +0x80, items separated by FF, 00 at the end)
    for name, tr in dat_ja.items():
        fn = name + '.DAT'
        raw = open(os.path.join(GAME, fn), 'rb').read()
        body = raw[7:7 + struct.unpack_from('<H', raw, 5)[0]]
        parts = body.split(b'\xff')
        for k, s in tr.items():
            b = enc7(s)
            if name in ('Q1', 'Q2'): b = b.ljust(36, b' ')
            parts[k] = bytes((c + 0x80) & 0xff for c in b)
        nb = b'\xff'.join(parts)
        if len(nb) > len(body):
            print('%s grew %d > %d' % (fn, len(nb), len(body)))
        nf = raw[:5] + struct.pack('<H', len(nb)) + nb + raw[7 + len(body):]
        sect('R', fn, struct.pack('<III', len(raw), zlib.crc32(raw) & 0xffffffff, len(nf)) + nf)
    blob = b'WLJP' + struct.pack('<II', 1, len(sections)) + b''.join(sections)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('START.EXE %d strings; patch.bin %d bytes, %d chars' % (cnt, len(blob), n))

if __name__ == '__main__':
    main()
