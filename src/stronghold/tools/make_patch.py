# 訳を独自の符号にして STRONG.EXE の書き換え（文字列・コールバック）と字形をまとめ、build/patch.bin を作る
import os, sys, json, glob, re, struct, zlib, collections
sys.path.insert(0, os.path.dirname(__file__))
from mz import MZ
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
T = lambda *p: os.path.join(ROOT, *p)
DG = 0x15f00

def bdf(path):
    g, cur, rows, bbx, asc = {}, None, None, None, 7
    for line in open(path, encoding='latin1'):
        t = line.split()
        if not t: continue
        if t[0] == 'FONT_ASCENT': asc = int(t[1])
        elif t[0] == 'ENCODING': cur = int(t[1])
        elif t[0] == 'BBX': bbx = list(map(int, t[1:5]))
        elif t[0] == 'BITMAP': rows = []
        elif t[0] == 'ENDCHAR':
            w, h, ox, oy = bbx
            top = asc - (oy + h)
            fr = [0] * 8
            for i, r in enumerate(rows):
                y = top + i
                if 0 <= y < 8: fr[y] = (int(r[:2], 16) >> ox) & 0xff
            g[cur] = bytes(fr)
            rows = None
        elif rows is not None:
            rows.append(t[0])
    return g

units = json.load(open(T('text', 'units.json'), encoding='utf-8'))
ja = {}
for f in sorted(glob.glob(T('text', 'ja', '*.json'))): ja.update(json.load(open(f, encoding='utf-8')))
if os.path.exists(T('text', 'replace.json')):
    for a, b in json.load(open(T('text', 'replace.json'), encoding='utf-8')).items():
        if a.startswith('_'): continue
        for k in ja: ja[k] = re.sub(a, b, ja[k])
if os.path.exists(T('text', 'fixes.json')):
    for k, v in json.load(open(T('text', 'fixes.json'), encoding='utf-8')).items():
        if not k.startswith('_'): ja[k] = v

def norm(s):
    out = []
    for ch in s:
        o = ord(ch)
        if 0xff01 <= o <= 0xff5e and (ch.isalnum() or ch in '％＋－．，：；＝'): ch = chr(o - 0xfee0)   # 全角英数は半角（ゲームの字体）
        if ch == '　': ch = '  '
        out.append(ch)
    return ''.join(out)

dat_units = json.load(open(T('text', 'dat_units.json'), encoding='utf-8'))
dja = json.load(open(T('text', 'ja', 'dat.json'), encoding='utf-8')) if os.path.exists(T('text', 'ja', 'dat.json')) else {}
for _a, _b in (json.load(open(T('text', 'replace.json'), encoding='utf-8')).items() if os.path.exists(T('text', 'replace.json')) else []):
    if not _a.startswith('_'):
        for _k in dja: dja[_k] = re.sub(_a, _b, dja[_k])
dat_items = [(x, norm(dja[x['id']])) for x in dat_units if x['id'] in dja]
items = []
for u in units:
    if u['id'] in ja and ja[u['id']] is not None:
        items.append((u, norm(ja[u['id']])))

# 符号: よく使う 128 字は 1 バイト、残りは 2 バイト
freq = collections.Counter(ch for _, j in items + dat_items for ch in j if ord(ch) >= 0x80)
freq.update('答え（）：')
FORCE1 = '戦魔僧盗ドエハマインダーク'           # 職業名の頭（"%s-%c" で 1 バイトだけ取られる）は 1 バイトの符号にする
for ch in FORCE1: freq[ch] += 100000
LEADS = [c for c in range(0x02, 0x20) if c not in (0x09, 0x0a, 0x0d, 0x1a, 0x1b)]
code = {}
for i, (ch, _) in enumerate(freq.most_common()):
    if i < 128: code[ch] = bytes([0x80 + i])
    else:
        k = i - 128
        if k >= len(LEADS) * 128: raise SystemExit('too many chars')
        code[ch] = bytes([LEADS[k // 128], 0x80 + k % 128])
def enc(s):
    b = bytearray()
    for ch in s:
        if ord(ch) < 0x80: b += ch.encode('latin-1')
        else: b += code[ch]
    return bytes(b)

m = MZ()
d = m.d
patches = []          # (file off, orig, new)
over = []
for u, j in items:
    en = u['en'].encode('latin-1')
    off = DG + u['off']
    assert d[off:off + len(en)] == en, u['id']
    b = enc(j)
    if len(b) > len(en):
        over.append((u['id'], len(en), len(b), u['en'], j))
        continue
    patches.append((off, en, b + bytes(len(en) - len(b))))
if over:
    print('overflow %d:' % len(over))
    for o in over: print('  %s %d>%d %r -> %s' % (o[0], o[2], o[1], o[3][:50], o[4]))

# 1 字の文字列（抜き出しの対象外）: 複数形の s は日本語では付けない
for off, en, j in ((0x6e75, b's', b''), (0x50b1, b'Auto-build    \x7f', enc('自動建設') + b'    \x7f')):   # 自動建設の入（0x7f = 灯りの印）
    assert d[DG + off:DG + off + len(en) + 1] == en + b'\x00'
    patches.append((DG + off, en, j + bytes(len(en) - len(j))))

# コールバック（文字のオーバーレイほか）
CB = 0x70
def cbp(off, orig_hex, k):
    o = bytes.fromhex(orig_hex)
    assert d[off:off + len(o)] == o, hex(off)
    patches.append((off, o, bytes([0xfe, 0x38, CB + k, 0]) + b'\x90' * (len(o) - 4)))
cbp(0x2235c, '558bec83ec0c', 0)                      # 1 字描く
for a in (0x225cf, 0x2274d, 0x22778, 0x227e3, 0x6bf8c, 0x6bfb7):
    cbp(a, '268a87a100', 1)                          # 字の幅
cbp(0x2260d, '268038207505', 2)                      # 折り返しの判定
cbp(0x2864d, '1e681207', 3)                          # コピープロテクトの答え

# 字形
font = bdf(T('font', 'misaki_gothic_2nd.bdf'))
KIN_HEAD = set('、。，．）」』！？ー…ぁぃぅぇぉっゃゅょァィゥェォッャュョ・：')
KIN_TAIL = set('（「『')
gl = []
miss = []
for ch, c in code.items():
    if ord(ch) not in font: miss.append(ch); continue
    fl = (1 if ch in KIN_HEAD else 0) | (2 if ch in KIN_TAIL else 0)
    v = c[0] if len(c) == 1 else (c[0] << 8) | c[1]
    gl.append((v, fl, font[ord(ch)]))
if miss: print('no glyph:', ''.join(miss))

datf = os.path.join(ROOT, '..', 'GAME', 'STRONG', 'STRONG.DAT')
db = struct.pack('<II', os.path.getsize(datf), len(dat_items))
for x, j in dat_items:
    t = enc(j.replace('\n', '\r\n')) + (b'\x1a' if x['eof'] else b'')
    db += struct.pack('<II', x['field'], len(t)) + t

def sect(t, b): return t.encode() + struct.pack('<I', len(b)) + b
gb = struct.pack('<I', len(gl)) + b''.join(struct.pack('<HBB', v, fl, 0) + bits for v, fl, bits in gl)
lb = bytearray(32)
for c in LEADS: lb[c >> 3] |= 1 << (c & 7)
eb = struct.pack('<II', len(d), len(patches)) + b''.join(struct.pack('<IH', o, len(a)) + a + b for o, a, b in patches)
ab = enc('（答え：') + b'\0' + enc('）') + b'\0'
body = sect('G', gb) + sect('L', bytes(lb)) + sect('E', eb) + sect('A', ab) + sect('D', db)
blob = b'SGJ1' + struct.pack('<I', 5) + body
open(T('build', 'patch.bin'), 'wb').write(blob)
json.dump({ch: c.hex() for ch, c in code.items()}, open(T('build', 'codes.json'), 'w', encoding='utf-8'), ensure_ascii=False)
print('strings %d/%d, chars %d (1-byte %d), patches %d, bytes %d' % (len(items) - len(over), len(units), len(code), min(128, len(code)), len(patches), len(blob)))
