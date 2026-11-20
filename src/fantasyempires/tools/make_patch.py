# 訳と字形と EXE の書き換えをまとめて build/patch.bin を作る（DLL のリソースになる）
#   'T' 完全一致の訳   'P' %s/%d の雛形   'V' 声の字幕（VOC 番号）   'G' 字形   'E' EXE の書き換え
import os, sys, json, struct, re, glob, zlib
sys.path.insert(0, os.path.dirname(__file__))
from le import LE
from squash import squash
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
T = lambda *p: os.path.join(ROOT, *p)

def bdf(path):
    g, cur, rows, bbx, asc, dw = {}, None, None, None, 7, 8
    for line in open(path, encoding='latin1'):
        t = line.split()
        if not t: continue
        if t[0] == 'FONT_ASCENT': asc = int(t[1])
        elif t[0] == 'ENCODING': cur = int(t[1])
        elif t[0] == 'DWIDTH': dw = int(t[1])
        elif t[0] == 'BBX': bbx = list(map(int, t[1:5]))
        elif t[0] == 'BITMAP': rows = []
        elif t[0] == 'ENDCHAR':
            w, h, ox, oy = bbx
            top = asc - (oy + h)
            fr = [0] * 8
            for i, r in enumerate(rows):
                y = top + i
                if 0 <= y < 8: fr[y] = (int(r[:2], 16) >> ox) & 0xff
            g[cur] = (bytes(fr), dw)
            rows = None
        elif rows is not None:
            rows.append(t[0])
    return g

def fnv64(b):
    h = 14695981039346656037
    for c in b: h = ((h ^ c) * 1099511628211) & 0xffffffffffffffff
    return h

# ---------------------------------------------------------------- 訳を集める
units = json.load(open(T('text', 'units.json'), encoding='utf-8'))
ja = {}
for f in sorted(glob.glob(T('text', 'ja', '*.json'))):
    ja.update(json.load(open(f, encoding='utf-8')))
if os.path.exists(T('text', 'replace.json')):
    REPL = [(a, b) for a, b in json.load(open(T('text', 'replace.json'), encoding='utf-8')).items() if not a.startswith('_')]
    for k in ja:
        for a, b in REPL: ja[k] = ja[k].replace(a, b)
fixes = json.load(open(T('text', 'fixes.json'), encoding='utf-8')) if os.path.exists(T('text', 'fixes.json')) else {}
for k, v in fixes.items():
    if not k.startswith('_'): ja[k] = v

# 英字で描ける字（ゲームの字形）: A-Z 0-9 空白 ! + - . ' , : ?   それ以外の半角は全角にする
ASCII_OK = set('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !+-.\',:?')
def norm(s):
    s = s.replace('\r', '')
    out = []
    for ch in s:
        if ch == '\n': out.append(ch); continue
        if 'a' <= ch <= 'z': ch = ch.upper()
        if ch == '：': ch = ':'                      # コロンは半角（字形が 4 ドット）
        if ord(ch) < 0x80 and ch not in ASCII_OK:
            ch = chr(ord(ch) + 0xfee0) if 0x21 <= ord(ch) <= 0x7e else ch
        # 全角英数は半角にする（ゲームの字形で描く）
        if 0xff10 <= ord(ch) <= 0xff19 or 0xff21 <= ord(ch) <= 0xff3a: ch = chr(ord(ch) - 0xfee0)
        out.append(ch)
    return ''.join(out).strip(' ')

# 雛形の %s/%d を \x01.. に（{1} {2} の形で順番を入れ替えられる）
def tmpl(j, en):
    n = len(re.findall(r'%[sdc]', en.decode('latin-1')))
    k = [0]
    def rep(m):
        if m.group(1): return chr(int(m.group(1)))
        k[0] += 1
        return chr(k[0])
    j = re.sub(r'\{(\d)\}|％[ｓｄｃ]|%[sdc]', rep, j)
    used = sorted(ord(c) for c in j if ord(c) < 10)
    assert used == list(range(1, n + 1)), (en, j, used)
    return j

entries = {}          # en(bytes) -> (ja, flags)
patterns = []         # (en bytes, ja, flags)
def add(en, j, flags=0, first=False):
    if j is None: return
    enb = en.encode('latin-1')
    if re.search(r'%[sdc]', en):
        patterns[:] = [p for p in patterns if p[0] != enb]
        item = (enb, norm(tmpl(j, enb)), flags)
        if first: patterns.insert(0, item)
        else: patterns.append(item)
        return
    j = norm(j)
    if en.startswith(' ') and en.strip(): flags |= 1          # 先頭の空白は中央寄せの意味（1 行の描画のとき）
    entries[enb] = (j, flags)

for x in units['exe'] + units['data']:
    if x['id'] in ja: add(x['en'], ja[x['id']])
# 手で書いた雛形・追加の訳 {"en": ..., "ja": ..., "flags": n}
if os.path.exists(T('text', 'extra.json')):
    for x in json.load(open(T('text', 'extra.json'), encoding='utf-8')):
        add(x['en'], x['ja'], x.get('flags', 0), True)

# 呼び出し元ごとの訳（狭い欄だけ短い形にする） {"en", "ja", "at": "0x1a0eb"}
ctx = []
if os.path.exists(T('text', 'context.json')):
    for x in json.load(open(T('text', 'context.json'), encoding='utf-8')):
        if 'en' not in x: continue
        for at in (x['at'] if isinstance(x['at'], list) else [x['at']]):
            ctx.append((x['en'].encode('latin-1'), int(at, 16), norm(x['ja'])))

# 声の字幕
voice = {}
if os.path.exists(T('text', 'voice', 'ja.json')):
    for k, v in json.load(open(T('text', 'voice', 'ja.json'), encoding='utf-8')).items():
        voice[int(k)] = norm(v)

def u16(s): return s.encode('utf-16-le')

# ---------------------------------------------------------------- 字形
font = bdf(T('font', 'misaki_gothic_2nd.bdf'))
chars = set()
for j, f in entries.values(): chars.update(j)
for e, j, f in patterns: chars.update(j)
for j in voice.values(): chars.update(j)
for e_, a_, j in ctx: chars.update(j)
chars.update('（答え：）')
chars.update(chr(c) for c in range(0x21, 0x7f))   # ゲームの字形にない半角（大きい字の ' , : ? など）は美咲で
chars.discard('\n')
glyphs = []
miss = []
KIN_HEAD = set('、。，．）」』！？ー…ぁぃぅぇぉっゃゅょァィゥェォッャュョ・：')
KIN_TAIL = set('（「『')
for ch in sorted(chars):
    c = ord(ch)
    if c < 10: continue
    if c not in font: miss.append(ch); continue
    bits, dw = font[c]
    fl = (1 if ch in KIN_HEAD else 0) | (2 if ch in KIN_TAIL else 0) | (4 if dw <= 4 else 0)
    glyphs.append((c, fl, bits))
if miss: print('no glyph:', ''.join(miss))

# ---------------------------------------------------------------- EXE の書き換え
e = LE()
CB = 0x70
def rel32(frm, to): return struct.pack('<i', to - frm)
def site_bytes(va, n): return bytes(e.raw[0][va - 0x10000: va - 0x10000 + n])
patches = []    # (file off, orig, new)
def patch(va, new):
    orig = site_bytes(va, len(new))
    for (oi, off, st, to, tgt) in e.fix:
        if oi == 0 and va - 0x10000 - 3 <= off < va - 0x10000 + len(new): raise SystemExit('fixup inside patch at %x' % va)
    patches.append((e.file_off(va), orig, new))
def cb(k, n=4): return bytes([0xfe, 0x38, CB + k, 0]) + b'\x90' * (n - 4)
assert site_bytes(0x15cc5, 6) == bytes.fromhex('5657c81c0000')
assert site_bytes(0x15ffc, 6) == bytes.fromhex('5657c8140000')
assert site_bytes(0x1627c, 7) == bytes.fromhex('89c6bf0a000000')
assert site_bytes(0x28af2, 4) == bytes.fromhex('c8780000')
assert site_bytes(0x28bd3, 9) == bytes.fromhex('803e0a0f84d5dcffff')
assert site_bytes(0x42c71, 4) == bytes.fromhex('51565755')
patch(0x15cc5, cb(0, 6))                          # 0: 折り返しありの文字列
patch(0x15ffc, cb(1, 6))                          # 1: 1 行の文字列
dm = cb(2) + cb(3)                                # 2: DM の文の準備, 3: 1 ページ描く（eax = 待つ長さ / 0 で終わり）
dm += bytes.fromhex('85c00f84') + rel32(0x1628c, 0x164a1)     # test eax,eax; jz 終わり（leave; pop..; ret）
dm += bytes.fromhex('e8') + rel32(0x16291, 0x1961a)           # call 待つ
dm += bytes.fromhex('eb') + struct.pack('<b', 0x16280 - 0x16293)   # jmp 3 へ
assert len(dm) == 0x16293 - 0x1627c
patch(0x1627c, dm)
patch(0x28af2, cb(4))                             # 4: 声の口の動き（入口: enter 0x78 の代わり）
patch(0x28bd3, cb(5, 9))                          # 5: 声の口の動き（毎回: 終わりの判定）
patch(0x42c71, cb(6))                             # 6: 効果音・声の再生（入口）

# ---------------------------------------------------------------- まとめる
def sect(tag, data): return tag.encode() + struct.pack('<I', len(data)) + data
tb = struct.pack('<I', len(entries))
for en, (j, fl) in sorted(entries.items()):
    w = u16(j)
    tb += struct.pack('<QHBBH', fnv64(en), len(en), fl, 0, len(w) // 2) + w
pb = struct.pack('<I', len(patterns))
for en, j, fl in patterns:
    w = u16(j)
    if en.startswith(b' ') and en.strip(): fl |= 1
    pb += struct.pack('<HBBH', len(en), fl, 0, len(w) // 2) + en + (b'\0' if len(en) & 1 else b'') + w
vb = struct.pack('<I', len(voice))
for k, j in sorted(voice.items()):
    w = u16(j)
    vb += struct.pack('<HH', k, len(w) // 2) + w
cb_ = struct.pack('<I', len(ctx))
for en, at, j in ctx:
    w = u16(j)
    cb_ += struct.pack('<QIHH', fnv64(en), at, len(en), len(w) // 2) + w
gb = struct.pack('<I', len(glyphs))
for c, fl, bits in glyphs: gb += struct.pack('<HBB', c, fl, 0) + bits + squash(bits) + bytes(2)
exe_size = os.path.getsize(os.path.join(ROOT, '..', 'GAME', 'FANTASY', 'FANTASY.EXE'))
eb = struct.pack('<II', exe_size, len(patches))
for off, o, n in patches: eb += struct.pack('<IH', off, len(o)) + o + n
body = sect('T', tb) + sect('P', pb) + sect('V', vb) + sect('C', cb_) + sect('G', gb) + sect('E', eb)
blob = b'FEJ1' + struct.pack('<I', 6) + body
os.makedirs(T('build'), exist_ok=True)
open(T('build', 'patch.bin'), 'wb').write(blob)
print('entries', len(entries), 'patterns', len(patterns), 'voice', len(voice), 'glyphs', len(glyphs), 'bytes', len(blob), 'crc %08X' % zlib.crc32(blob))
