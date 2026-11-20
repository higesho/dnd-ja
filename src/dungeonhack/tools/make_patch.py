# Build build/patch.bin (embedded into winmm.dll) from text/ja/strings.json.
#   python -X utf8 tools/make_patch.py
import sys, os, json, struct, zlib
sys.path.insert(0, os.path.dirname(__file__))
import res

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'HACK')
BDF8 = os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf')
BDF14 = os.path.join(ROOT, 'font', 'shnmk14.bdf')
CB_BASE = 0x70
LOG = int(os.environ.get('DHJP_LOG', '0'))

NO_START = set('、。，．・：；？！ー…‥」』）］｝〕〉》】ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々―')
NO_END = set('「『（［｛〔〈《【')
FW = {chr(0xff01 + i): chr(0x21 + i) for i in range(94)}
for k in '！？（）％：、。「」・ー＿／':
    FW.pop(k, None)

def load_bdf(path, size):
    glyphs = {}
    cur = None
    lines = open(path, encoding='latin1').read().split('\n')
    asc = size - 1
    for l in lines:
        if l.startswith('FONT_ASCENT'):
            asc = int(l.split()[1])
    i = 0
    bpr = (size + 7) // 8
    while i < len(lines):
        l = lines[i]
        if l.startswith('ENCODING'):
            cur = int(l.split()[1])
        elif l.startswith('BBX'):
            w, h, xo, yo = map(int, l.split()[1:])
        elif l == 'BITMAP':
            rows = [0] * size
            top = asc - yo - h
            for r in range(h):
                hx = lines[i + 1 + r].strip()
                v = int(hx, 16) << (8 * bpr - 4 * len(hx))     # left aligned in bpr bytes
                v >>= xo
                if 0 <= top + r < size:
                    rows[top + r] = v & ((1 << (8 * bpr)) - 1)
            glyphs[cur] = b''.join(r.to_bytes(bpr, 'big') for r in rows)
            i += h
        i += 1
    return glyphs

def jis_to_unicode_table(path):
    """shnmk14.bdf is JIS X 0208 encoded: build unicode -> jis via Python's codec"""
    return None

def load_shnm(path):
    g = load_bdf(path, 14)
    out = {}
    for code, bm in g.items():
        hi, lo = code >> 8, code & 0xff
        try:
            ch = bytes([hi | 0x80, lo | 0x80]).decode('euc_jp')
        except Exception:
            continue
        out[ord(ch)] = bm
    return out

def norm(s):
    s = s.replace('—', '―').replace('〜', 'ー')
    return ''.join(FW.get(c, c) for c in s)

def main():
    g8 = load_bdf(BDF8, 8)
    g14 = load_shnm(BDF14)
    en = {x['id']: x for x in json.load(open(os.path.join(ROOT, 'text', 'en', 'strings.json'), encoding='utf-8'))}
    ja = {int(k): norm(v) for k, v in json.load(open(os.path.join(ROOT, 'text', 'ja', 'strings.json'), encoding='utf-8')).items()}
    chars = sorted({c for s in ja.values() for c in s if ord(c) >= 0x80} | set('「」。、'))
    if len(chars) > 64 * 64:
        raise SystemExit('too many characters: %d' % len(chars))
    code = {}
    for k, c in enumerate(chars):
        code[c] = ((0x80 + k // 64) << 8) | (0xc0 + k % 64)
    missing = [c for c in chars if ord(c) not in g8]
    if missing:
        print('no glyph (8):', ''.join(missing))
    def enc(s):
        b = bytearray()
        for c in s:
            if ord(c) < 0x80:
                b.append(ord(c))
            else:
                v = code[c]; b += bytes([v >> 8, v & 0xff])
        return bytes(b)
    sections = []
    def sect(t, name, payload):
        sections.append(struct.pack('<II', ord(t), len(payload) + 16) + name.encode().ljust(16, b'\0') + payload)
    # params
    sections.append(struct.pack('<II', ord('P'), 8) + struct.pack('<II', CB_BASE, LOG))
    # font
    n = len(chars)
    codes = b''.join(struct.pack('<H', code[c]) for c in chars)
    bm8 = b''.join(g8.get(ord(c), bytes(8)) for c in chars)
    def cls(c):
        o = ord(c)
        if 0x3041 <= o <= 0x309f: return 4                      # hiragana
        if 0x30a0 <= o <= 0x30ff: return 8                      # katakana (incl. ー・)
        if 0x4e00 <= o <= 0x9fff or c in '々〆': return 16      # kanji
        if c in '、。，．！？…‥」』）】': return 32               # good place to end a line
        return 0
    fl = bytes((1 if c in NO_START else 0) | (2 if c in NO_END else 0) | cls(c) for c in chars)
    bm14 = b''.join(g14.get(ord(c), g14.get(ord('〓'), bytes(28))) for c in chars)
    def squeeze(b):
        rows = list(b[:7])
        best = min(range(6), key=lambda i: (bin(rows[i] ^ rows[i + 1]).count('1') + (3 if i == 0 else 0), -i))
        rows[best] |= rows[best + 1]; del rows[best + 1]
        return bytes(rows) + bytes(2)
    bm6 = b''.join(squeeze(g8.get(ord(c), bytes(8))) for c in chars)
    payload = struct.pack('<I', n) + codes + bm8 + fl + bm14 + bm6
    sections.append(struct.pack('<II', ord('F'), len(payload)) + payload)
    # AESOP.EXE
    exe = open(os.path.join(GAME, 'AESOP.EXE'), 'rb').read()
    hdr = struct.unpack_from('<H', exe, 8)[0] * 16
    seg = hdr + 0x12fc * 16
    def cb(k): return bytes([0xfe, 0x38, CB_BASE + k, 0])
    patches = [
        (0x51ea, bytes.fromhex('558bec1e56'), cb(0) + b'\xcb'),       # char width: callback; retf
        (0x5236, bytes.fromhex('8b1ed516'), cb(1)),                   # char draw: after the window copy
        (0x5c40, bytes.fromhex('558bec1e'), cb(2)),                   # print_buffer entry
    ]
    xp = struct.pack('<II', len(exe), zlib.crc32(exe) & 0xffffffff)
    for off, old, new in patches:
        a = seg + off
        assert exe[a:a + len(old)] == old, hex(off)
        assert len(new) == len(old)
        xp += struct.pack('<II', a, len(new)) + new
    sect('X', 'AESOP.EXE', xp)
    # resources
    for fn, base in (('HACK.RES', 0), ('OPEN.RES', 10000)):
        d = open(os.path.join(GAME, fn), 'rb').read()
        recs = []
        for i, s in sorted(ja.items()):
            if not (base <= i < base + 10000) or i not in en:
                continue
            recs.append(struct.pack('<II', i - base, len(enc(s)) + 3) + b'S:' + enc(s) + b'\0')
        if not recs:
            continue
        grow = [(6, 8)] if fn == 'HACK.RES' else []      # 6x8 font: 6 -> 7 rows (Japanese glyphs are 7 rows high)
        sect('R', fn, struct.pack('<III', len(d), zlib.crc32(d) & 0xffffffff, len(recs)) + b''.join(recs)
             + struct.pack('<I', len(grow)) + b''.join(struct.pack('<II', a, b) for a, b in grow))
        print(fn, len(recs), 'strings')
    blob = b'DHJP' + struct.pack('<II', 1, len(sections)) + b''.join(sections)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin %d bytes, %d chars' % (len(blob), n))

if __name__ == '__main__':
    main()
