# Build build/patch.bin (embedded into winmm.dll) from text/ja/strings.json.
#   python -X utf8 tools/make_patch.py
import sys, os, json, struct, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import le

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'RLOFT')
DH = os.path.join('L:/SteamLibrary/steamapps/common/Forgotten Realms The Archives - Collection Three/games/Dungeon Hack-ENG/_jp', 'tools')
sys.path.insert(0, DH)
import make_patch as dhm          # font loaders (misaki 8x8, shinonome 14)
CB_BASE = 0x70
LOG = int(os.environ.get('RLJP_LOG', '0'))
SMALL_W = int(os.environ.get('RLJP_SMALLW', '9'))   # advance of a small Japanese character (6 = k6x8, 9 = misaki)
TEXT_HEIGHTS = [int(x) for x in os.environ.get('RLJP_TEXTH', '11').split(',') if x]   # font heights drawn with k8x12 (prose)
TEXT_W = 8                                                # advance of a k8x12 character
SEP = '\x1f'                      # line-break opportunity (the patched word wrap breaks here, width 0)

NO_START = dhm.NO_START
NO_END = dhm.NO_END

def cls(c):
    o = ord(c)
    if 0x3041 <= o <= 0x309f: return 'h'
    if 0x30a0 <= o <= 0x30ff: return 'k'
    if 0x4e00 <= o <= 0x9fff or c in '々〆': return 'K'
    return 'p'

def add_breaks(s):
    """insert SEP where a line may break: between Japanese characters (kinsoku), after spaces"""
    out = []
    for i, c in enumerate(s):
        if i > 0:
            p = s[i - 1]
            ok = False
            if p == ' ':
                ok = c != ' '
            elif ord(p) >= 0x80 or ord(c) >= 0x80:
                ok = c not in NO_START and p not in NO_END and c not in '|]' and p not in '[|' and c != ' '
                # keep katakana words and kanji+okurigana together when possible
                if ok and cls(p) == 'k' and cls(c) == 'k': ok = False
                if ok and ord(c) < 0x80 and c not in '[': ok = False
            if ok and out and out[-1] != SEP:
                out.append(SEP)
        out.append(c)
    return ''.join(out)

def main():
    g8 = dhm.load_bdf(dhm.BDF8, 8)
    if SMALL_W == 6:                                            # k6x8 (6x8) font, misaki for anything it lacks
        g8.update(dhm.load_bdf(os.path.join(ROOT, 'font', 'k6x8_gothic.bdf'), 8))
    g14 = dhm.load_shnm(dhm.BDF14)
    en = {x['id']: x for x in json.load(open(os.path.join(ROOT, 'text', 'en', 'strings.json'), encoding='utf-8'))}
    ja = {int(k): dhm.norm(v) for k, v in json.load(open(os.path.join(ROOT, 'text', 'ja', 'strings.json'), encoding='utf-8')).items()}
    import re
    def read_lines(p):
        out = {}
        if os.path.exists(p):
            for line in open(p, encoding='utf-8-sig'):
                m = re.match(r'^\[([^\]]+)\] ?(.*?)\r?$', line.rstrip('\n'))
                if m: out[m.group(1)] = dhm.norm(m.group(2))
        return out
    def jload(*parts, default=None):
        q = os.path.join(ROOT, *parts)
        return json.load(open(q, encoding='utf-8')) if os.path.exists(q) else default
    exe_items = jload('text', 'exe', 'items.json', default=[])
    exe_ja = {int(k): v for k, v in read_lines(os.path.join(ROOT, 'text', 'exe', 'exe.ja.txt')).items()}
    exe_ja.update({int(k): dhm.norm(v) for k, v in json.load(open(os.path.join(ROOT, 'text', 'exe', 'fixes.json'), encoding='utf-8')).items()}
                  if os.path.exists(os.path.join(ROOT, 'text', 'exe', 'fixes.json')) else {})
    for k, x in enumerate(jload('text', 'exe', 'extra.json', default=[])):
        x['id'] = 5000 + k                      # strings found by hand (labels the extractor skipped)
        exe_items.append(x); exe_ja[x['id']] = x['ja']
    names_ja = read_lines(os.path.join(ROOT, 'text', 'names', 'names.ja.txt'))
    allja = list(ja.values()) + list(exe_ja.values()) + list(names_ja.values())
    chars = sorted({c for s in allja for c in s if ord(c) >= 0x80} | set('「」。、'))
    if len(chars) > 64 * 64:
        raise SystemExit('too many characters: %d' % len(chars))
    code = {c: ((0x80 + k // 64) << 8) | (0xc0 + k % 64) for k, c in enumerate(chars)}
    missing = [c for c in chars if ord(c) not in g8]
    if missing:
        print('no glyph (8):', ''.join(missing))
    def enc(s):
        b = bytearray()
        for c in s:
            if ord(c) < 0x80: b.append(ord(c))
            else: v = code[c]; b += bytes([v >> 8, v & 0xff])
        return bytes(b)
    sections = []
    def sect(t, name, payload):
        sections.append(struct.pack('<II', ord(t), len(payload) + 16) + name.encode().ljust(16, b'\0') + payload)
    tmask = sum(1 << h for h in TEXT_HEIGHTS)
    sections.append(struct.pack('<II', ord('P'), 20) + struct.pack('<IIIII', CB_BASE, LOG, SMALL_W, tmask, TEXT_W))
    n = len(chars)
    codes = b''.join(struct.pack('<H', code[c]) for c in chars)
    bm8 = b''.join(g8.get(ord(c), bytes(8)) for c in chars)
    fl = bytes((1 if c in NO_START else 0) | (2 if c in NO_END else 0) for c in chars)
    bm14 = b''.join(g14.get(ord(c), g14.get(ord('〓'), bytes(28))) for c in chars)
    bm6 = bytes(8 * n)
    g12 = dhm.load_bdf(os.path.join(ROOT, 'font', 'k8x12.bdf'), 12)
    miss12 = [c for c in chars if ord(c) not in g12]
    if miss12:
        print('no glyph (k8x12):', ''.join(miss12))
    def k12(c):
        if ord(c) in g12:
            b = g12[ord(c)]; return bytes(b[2 * y] for y in range(12))
        b = g8.get(ord(c), bytes(8)); return bytes(2) + b + bytes(2)   # misaki, centred
    bm12 = b''.join(k12(c) for c in chars)
    payload = struct.pack('<I', n) + codes + bm8 + fl + bm14 + bm6 + bm12
    sections.append(struct.pack('<II', ord('F'), len(payload)) + payload)
    # CODE.1 (character creation) and CODE.2 (game): text module patched by byte patterns (tools/code_patches.py)
    import code_patches
    for fn in ('CODE.1', 'CODE.2'):
        exe = open(os.path.join(GAME, fn), 'rb').read()
        patches = code_patches.patches(fn, exe, CB_BASE)
        for x in exe_items:                                                      # UI strings, in place
            if x['file'] != fn or x['id'] not in exe_ja: continue
            b = enc(exe_ja[x['id']])
            if len(b) > x['room']:
                print('too long (%d > %d): [%d] %s' % (len(b), x['room'], x['id'], exe_ja[x['id']])); continue
            old = exe[x['off']:x['off'] + x['room'] + 1]
            patches.append((x['off'], old, b + bytes(len(old) - len(b))))
        xp = struct.pack('<II', len(exe), zlib.crc32(exe) & 0xffffffff)
        for f, old, new in patches:
            assert exe[f:f + len(old)] == old, (fn, hex(f))
            assert len(new) == len(old)
            xp += struct.pack('<II', f, len(new)) + new
        sect('X', fn, xp)
        print(fn, len(patches), 'patches')
    # item and creature names (OBJECTS.DAT / NPCS.DAT name fields)
    import names_chunk
    for fn, size, room, tag in names_chunk.FILES:
        raw = open(os.path.join(GAME, fn), 'rb').read()
        xp = struct.pack('<II', len(raw), zlib.crc32(raw) & 0xffffffff)
        cnt = 0
        for i, o, nm in names_chunk.records(fn, size):
            ja_nm = names_ja.get(tag + ':' + nm)
            if not ja_nm: continue
            b = enc(ja_nm)
            if len(b) > room:
                print('name too long:', nm, ja_nm); continue
            xp += struct.pack('<II', o, room + 1) + b + bytes(room + 1 - len(b)); cnt += 1
        sect('X', fn, xp)
        print(fn, cnt, 'names')
    # T
    d = open(os.path.join(GAME, 'T'), 'rb').read()
    recs = []
    longest = 0
    for i, s in sorted(ja.items()):
        if i not in en: continue
        b = enc(add_breaks(s))
        longest = max(longest, len(b))
        recs.append(struct.pack('<II', i, len(b)) + b)
    sect('T', 'T', struct.pack('<III', len(d), zlib.crc32(d) & 0xffffffff, len(recs)) + b''.join(recs))
    print('T: %d strings, longest %d bytes' % (len(recs), longest))
    blob = b'MZJP' + struct.pack('<II', 1, len(sections)) + b''.join(sections)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin %d bytes, %d chars' % (len(blob), n))

if __name__ == '__main__':
    main()
