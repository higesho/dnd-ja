# Build build/patch.bin (embedded into winmm.dll) from text/ja/strings.json.
#   python -X utf8 tools/make_patch.py
import sys, os, json, struct, zlib
sys.path.insert(0, os.path.dirname(__file__))
import res, le

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'EOB3')
BDF = os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf')
CB_BASE = 0x70
LOG = int(os.environ.get('EJP_LOG', '0'))

NO_START = set('、。，．・：；？！ー…‥」』）］｝〕〉》】ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々―')
NO_END = set('「『（［｛〔〈《【')
FW = {chr(0xff01 + i): chr(0x21 + i) for i in range(94)}
for k in '！？（）％：、。「」・ー＿／':
    FW.pop(k, None)

def load_bdf(path):
    glyphs = {}
    cur = None
    lines = open(path, encoding='latin1').read().split('\n')
    i = 0
    while i < len(lines):
        l = lines[i]
        if l.startswith('ENCODING'):
            cur = int(l.split()[1])
        elif l.startswith('BBX'):
            w, h, xo, yo = map(int, l.split()[1:])
        elif l == 'BITMAP':
            rows = [0] * 8
            top = 7 - yo - h
            for r in range(h):
                v = int(lines[i + 1 + r], 16) >> xo
                if 0 <= top + r < 8:
                    rows[top + r] = v & 0xff
            glyphs[cur] = bytes(rows)
            i += h
        i += 1
    return glyphs

CINE_Y0 = 121           # subtitle area: rows 121..199 (black, text in colour 11 on 10)
CINE_W = 38             # characters per line (8 px each)

def _kind(c):
    o = ord(c)
    if 0x3041 <= o <= 0x309f: return 'h'
    if 0x30a0 <= o <= 0x30ff or c == '・': return 'k'
    if 0x4e00 <= o <= 0x9fff or c in '々〆': return 'K'
    return 'p'

def _break_cost(a, b):
    """cost of a line break between characters a and b"""
    if a in '、。！？': return 0
    if b in '「': return 1
    ka, kb = _kind(a), _kind(b)
    if ka == 'k' and kb == 'k': return 40      # inside a katakana word
    if ka == 'K' and kb == 'K': return 14      # inside a kanji compound
    if ka == 'h' and kb == 'K': return 4       # okurigana/particle -> next word
    if ka == 'h' and kb == 'k': return 4
    if ka == 'K' and kb == 'h': return 20      # kanji + its okurigana
    if ka == 'k' and kb == 'h': return 8
    return 12

def wrap_ja(s, width=CINE_W):
    """split into balanced lines; explicit \\n in the text wins"""
    if '\n' in s:
        return [l for part in s.split('\n') for l in wrap_ja(part, width)]
    n = max(1, -(-len(s) // 32))            # aim for lines of about 32 characters (max width)
    lines = []
    while s:
        if len(s) <= width and len(lines) + 1 >= n:
            lines.append(s); break
        left = n - len(lines)
        target = -(-len(s) // left)
        best = None
        for cut in range(max(1, len(s) - width * (left - 1)), min(width, len(s) - 1) + 1):
            if s[cut] in NO_START or s[cut - 1] in NO_END:
                continue
            score = abs(cut - target) + _break_cost(s[cut - 1], s[cut])
            if best is None or score < best[0]:
                best = (score, cut)
        cut = best[1] if best else min(width, target)
        lines.append(s[:cut]); s = s[cut:]
    return lines

def cine_rows(text, bdf):
    W, H = 320, 200
    pix = [[10] * W for _ in range(H)]
    lines = wrap_ja(text)
    top = max(124, 152 - (12 * len(lines) - 4) // 2) if len(lines) < 3 else max(124, 140 - 6 * (len(lines) - 3))
    for li, line in enumerate(lines):
        x0 = (W - 8 * len(line)) // 2
        for k, ch in enumerate(line):
            cp = ord(ch)
            if cp < 0x7f and cp > 0x20:
                cp += 0xfee0                      # ASCII -> full width glyph
            g = bdf.get(cp)
            if g is None:
                if ch not in ' 　':
                    print('cine: no glyph for', ch)
                continue
            for gy in range(8):
                for gx in range(8):
                    if g[gy] & (0x80 >> gx):
                        pix[top + 12 * li + gy][x0 + 8 * k + gx] = 11
    def rle(seg):
        out = bytearray(); i = 0
        while i < len(seg):
            r = 1
            while i + r < len(seg) and seg[i + r] == seg[i] and r < 128: r += 1
            if r >= 3:
                out += bytes([((r - 1) << 1) | 1, seg[i]]); i += r; continue
            r = 1
            while i + r < len(seg) and r < 128 and not (i + r + 2 < len(seg) and seg[i + r] == seg[i + r + 1] == seg[i + r + 2]): r += 1
            out += bytes([(r - 1) << 1]) + bytes(seg[i:i + r]); i += r
        return bytes(out)
    rows = bytearray()
    for y in range(CINE_Y0, H):
        rows.append(y)
        for sx, last in ((0, 0), (160, 0x8000)):
            data = rle(pix[y][sx:sx + 160])
            assert len(data) < 256
            rows += struct.pack('<HBB', sx | last, 160, len(data)) + data
    return bytes(rows), lines

def cine_sections(bdf):
    import gff
    enp = os.path.join(ROOT, 'text', 'cine', 'en.json'); jap = os.path.join(ROOT, 'text', 'cine', 'ja.json')
    if not os.path.exists(jap) or os.environ.get('EJP_NOCINE'):
        return []
    en = json.load(open(enp, encoding='utf-8'))
    ja = {e['id']: e['ja'] for e in json.load(open(jap, encoding='utf-8'))}
    per = {}
    for e in en:
        if e['id'] not in ja:
            continue
        rows, lines = cine_rows(ja[e['id']], bdf)
        if LOG:
            print('cine %d: %s' % (e['id'], ' / '.join(lines)))
        for f, tag, cid, fr in e['frames']:
            per.setdefault(f, []).append(tag.encode('latin1') + struct.pack('<IIII', cid, fr, CINE_Y0, len(rows)) + rows)
    out = []
    for f, recs in sorted(per.items()):
        d = open(os.path.join(GAME, f + '.GFF'), 'rb').read()
        out.append((f + '.GFF', struct.pack('<III', len(d), zlib.crc32(d) & 0xffffffff, len(recs)) + b''.join(recs)))
    return out

def main():
    bdf = load_bdf(BDF)
    ja = {int(k): v for k, v in json.load(open(os.path.join(ROOT, 'text', 'ja', 'strings.json'), encoding='utf-8')).items()}
    chg = json.load(open(os.path.join(ROOT, 'text', 'chargen.json'), encoding='utf-8'))['strings']
    # character set
    chars = set('。「運命を選べ')
    for s in list(ja.values()) + [x[1] for x in chg]:
        for ch in s:
            ch = FW.get(ch, ch)
            if ord(ch) > 0x7e:
                chars.add(ch)
    if '　' in chars:
        chars.discard('　')
    missing = sorted(c for c in chars if ord(c) not in bdf)
    if missing:
        print('no glyph for:', ''.join(missing))
    chars = sorted(c for c in chars if ord(c) in bdf)
    if len(chars) > 64 * 64:
        raise SystemExit('too many characters: %d' % len(chars))
    code = {c: bytes([0x80 + (i >> 6), 0xc0 + (i & 63)]) for i, c in enumerate(chars)}

    def encode(s):
        out = bytearray()
        for ch in s:
            ch = FW.get(ch, ch)
            if ch == '　':
                out += b'  '
            elif ord(ch) < 0x7f:
                out.append(ord(ch))
            elif ch in code:
                out += code[ch]
            else:
                out += b'?'
        return bytes(out)

    # EYE.RES replacements
    d = res.load()
    reps = []
    for i, s in sorted(ja.items()):
        if os.environ.get('EJP_NOTEXT'): break
        reps.append((i, b'S:' + encode(s) + b'\0'))

    # INTERP.EXE patches
    L = le.LE()
    exe = L.d
    def cb(n):
        return bytes([0xfe, 0x38, CB_BASE + n, 0x00])
    patches = [
        (0x22ce3, b'\x55\x8b\xec\x53\x56', cb(0) + b'\xc3'),          # VFX_character_width
        (0x22d03, b'\x55\x8b\xec\x83\xc4\xdc', cb(1) + b'\x90\x90'),  # VFX_character_draw
        (0x14ef0, b'\x8b\x54\x24\x04', cb(2)),                        # GIL2VFX_print_buffer wrapper
    ]
    xp = struct.pack('<II', len(exe), zlib.crc32(exe) & 0xffffffff)
    if os.environ.get('EJP_NOCB'): patches = []
    for va, orig, new in patches:
        if L.raw[0][va - 0x10000: va - 0x10000 + len(orig)] != orig:
            raise SystemExit('INTERP.EXE does not match at %x' % va)
        xp += struct.pack('<II', L.file_off(va), len(new)) + new

    sects = []
    def sect_t(ch, payload):
        sects.append(struct.pack('<II', ord(ch), len(payload)) + payload)
    sect_t('P', struct.pack('<II', CB_BASE, LOG))
    codes = []; bitmaps = b''; flags = b''
    for c in chars:
        e = code[c]
        codes.append((e[0] << 8) | e[1])
        bitmaps += bdf[ord(c)]
        flags += bytes([(1 if c in NO_START else 0) | (2 if c in NO_END else 0) | (4 if c == '。' else 0) | (8 if c == '「' else 0)])
    sect_t('F', struct.pack('<I', len(codes)) + struct.pack('<%dH' % len(codes), *codes) + bitmaps + flags)
    sect_t('X', b'INTERP.EXE'.ljust(16, b'\0') + xp)
    # character generation: MGA.OVL Print -> callback; English -> Japanese table
    mga = open(os.path.join(GAME, 'CHARGEN', 'MGA.OVL'), 'rb').read()
    if mga[0x2636:0x2639] != b'\x55\x8b\xec':
        raise SystemExit('MGA.OVL does not match')
    mp = struct.pack('<II', len(mga), zlib.crc32(mga) & 0xffffffff) + struct.pack('<II', 0x2636, 5) + cb(3) + b'\xcb'
    sect_t('X', b'MGA.OVL'.ljust(16, b'\0') + mp)
    tb = struct.pack('<I', len(chg))
    for en, j, fl in chg:
        e = en.encode('latin1'); q = encode(j)
        tb += struct.pack('<BHIH', fl, len(e), zlib.crc32(e) & 0xffffffff, len(q)) + q
    sect_t('T', tb)
    rp = struct.pack('<III', len(d), zlib.crc32(d) & 0xffffffff, len(reps))
    for i, b in reps:
        rp += struct.pack('<II', i, len(b)) + b
    # picture edits (done by the DLL on the user's own EYE.RES)
    def op_erase(x0, y0, x1, y1, colors):
        return b'\x01' + struct.pack('<hhhhB', x0, y0, x1, y1, len(colors)) + bytes(colors)
    def op_text(x, y, ink, shadow, s):
        cs = [(code[c][0] << 8) | code[c][1] for c in s]
        return b'\x02' + struct.pack('<hhBBB', x, y, ink, shadow, len(cs)) + struct.pack('<%dH' % len(cs), *cs)
    title = '運命を選べ'
    for c in title:
        assert c in code, c
    pics = [
        (188, op_erase(65, 101, 252, 153, list(range(136, 140)) + list(range(152, 156)))   # menu items
             + op_erase(104, 86, 218, 92, [58, 60, 62, 74, 76, 147, 10])                   # CHOOSE YOUR DESTINY
             + op_text(161 - len(title) * 4, 86, 147, 10, title)),
    ]
    rp += struct.pack('<I', len(pics))
    for i, ops in pics:
        rp += struct.pack('<II', i, len(ops)) + ops
    # script byte patches: (resource index, [(offset, orig bytes, new bytes)])
    # PC (1369): character info lines 7 px apart -> 8 px (Japanese glyphs are 7 px tall)
    pc = []
    for off, y0, y1 in [(0x25df, 0x37, 53), (0x2601, 0x3e, 61), (0x2623, 0x45, 69)]:
        pc.append((off, bytes([y0]), bytes([y1])))
    ys = [0x52, 0x59, 0x60, 0x67, 0x6e, 0x75, 0x7c]
    for k, (lab, val) in enumerate(zip([0x2645, 0x265f, 0x2679, 0x2693, 0x26ad, 0x26c7, 0x26e1],
                                       [0x2714, 0x2792, 0x27b7, 0x27dc, 0x2801, 0x2826, 0x284b])):
        pc.append((lab, bytes([ys[k]]), bytes([78 + 8 * k])))
        pc.append((val, bytes([ys[k]]), bytes([78 + 8 * k])))
    scripts = [(1369, pc)]
    rp += struct.pack('<I', len(scripts))
    for i, recs in scripts:
        rp += struct.pack('<II', i, len(recs))
        for off, a, b in recs:
            rp += struct.pack('<IB', off, len(a)) + a + b
    sect_t('R', b'EYE.RES'.ljust(16, b'\0') + rp)
    # cinematics: subtitle rows of BMP/BMA frames in the GFF files
    for name, payload in cine_sections(bdf):
        sect_t('G', name.encode().ljust(16, b'\0') + payload)
    blob = struct.pack('<4sII', b'EJP3', 1, len(sects)) + b''.join(sects)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    json.dump({c: code[c].hex() for c in chars}, open(os.path.join(ROOT, 'build', 'charmap.json'), 'w', encoding='utf-8'), ensure_ascii=False)
    print('patch.bin %d bytes, %d chars, %d strings' % (len(blob), len(chars), len(reps)))

if __name__ == '__main__':
    main()
