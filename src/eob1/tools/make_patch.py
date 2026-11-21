# build/patch.bin（winmm.dll に埋め込む）を作る。
#   python -X utf8 tools/make_patch.py [ja]
# 入力: text/<dir>/*.json  … { "英語原文": "訳文", ... }
import sys, os, json, struct, re, zlib, glob

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..', 'GAME', 'EYE'))
BDF = os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf')
CB_BASE = 0x70
LOG = int(os.environ.get('EJP_LOG', '0'))

NO_START = set('、。，．・：；？！ー…‥」』）］｝〕〉》】ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々〜～!?.,)]:;\'"')
NO_END = set('「『（［｛〔〈《【(["')
FW = {chr(0xff01 + i): chr(0x21 + i) for i in range(94)}
for k in '！？（）％：、。「」・ー＿':
    FW.pop(k, None)

def load_bdf(path):
    glyphs, cur = {}, None
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

def norm_key(s, lower=True):
    """原文の照合キー: 制御文字・空白の並びを1つの空白に、前後の空白を除き、小文字に"""
    s = re.sub(r'[\x00-\x20]+', ' ', s).strip()
    return s.lower() if lower else s

SPEC = re.compile(r'%%|%[-+ #0-9.]*([sdxXuc])')

class Glyphs:
    def __init__(self):
        self.bdf = load_bdf(BDF)
        self.index = {}
        self.order = []
    def idx(self, ch):
        if ch not in self.index:
            cp = ord(ch)
            if cp not in self.bdf:
                raise KeyError('glyph missing: %r U+%04X' % (ch, cp))
            self.index[ch] = len(self.order)
            self.order.append(ch)
        return self.index[ch]

def enc_value(s, gl, pattern):
    """訳文 -> u16 列（ASCII はそのまま、改行 0x0d、和文字 0x8000|字形番号、引数 0xF000+n）"""
    out = []
    argn = 0
    i = 0
    while i < len(s):
        ch = s[i]
        if s.startswith('{strict}', i):           # 文字列の引数がすべて訳せるときだけ使うパターン
            out.append(0xF0FF); i += 8; continue
        m = re.match(r'\{t(\d+)\}', s[i:])     # {tN}: N 桁目（原作の字幅で）へ移動
        if m:
            out.append(0xE300 | int(m.group(1))); i += len(m.group(0)); continue
        if pattern:
            m = re.match(r'\{(\d)\}', s[i:])
            if m:
                out.append(0xF000 + int(m.group(1))); i += len(m.group(0)); continue
            m = SPEC.match(s, i)
            if m and m.group(0) != '%%':
                out.append(0xF000 + argn); argn += 1; i = m.end(); continue
            if m:
                out.append(ord('%')); i = m.end(); continue
        ch = FW.get(ch, ch)
        if ch == '　':
            ch = ' '
        if ch in '\r\n':
            out.append(0x0d)
        elif ord(ch) < 0x80:
            out.append(ord(ch))
        else:
            out.append(0x8000 | gl.idx(ch))
        i += 1
    return out

def enc_pattern_key(en):
    """%s=0x01 %d=0x02 %x=0x03 として照合キーを作る"""
    k = norm_key(en)
    out = bytearray()
    pos = 0
    for m in SPEC.finditer(k):
        out += k[pos:m.start()].encode('latin1')
        if m.group(0) == '%%':
            out += b'%'
        else:
            out.append({'s': 1, 'c': 1, 'd': 2, 'u': 2, 'x': 3, 'X': 3}[m.group(1)])
        pos = m.end()
    out += k[pos:].encode('latin1')
    return bytes(out)

def crc32(b):
    return zlib.crc32(b) & 0xffffffff

def sect(t, data):
    return struct.pack('<II', t if isinstance(t, int) else ord(t), len(data)) + data

# ---------------------------------------------------------------- code patches
def cb(n):
    return bytes([0xfe, 0x38]) + struct.pack('<H', CB_BASE + n)

def exe_patch():
    """EOB.EXE: 1f89:060a のメッセージ表示関数を、DLL が行を作り 1f89:03b2 で1行ずつ出す形に置き換える"""
    orig = open(os.path.join(GAME, 'EOB.EXE'), 'rb').read()
    F = 0x20c7a                     # 1f89:060a
    assert orig[F:F + 9] == bytes.fromhex('558bec81ecac005657')
    base = 0x60a
    code = bytearray()
    def here(): return base + 9 + len(code)
    code += cb(0)                   # PREP
    L = here()
    code += cb(1)                   # NEXT -> ax=1: 行あり, dx=改行フラグ
    code += b'\x0b\xc0'             # or ax,ax
    jz = len(code); code += b'\x74\x00'
    code += b'\x52\x16\x8d\x86\x54\xff\x50\x0e'   # push dx; push ss; lea ax,[bp-0xac]; push ax; push cs
    nxt = here() + 3
    code += b'\xe8' + struct.pack('<h', 0x3b2 - nxt)
    code += b'\x83\xc4\x06'
    nxt = here() + 2
    code += b'\xeb' + struct.pack('<b', L - nxt)
    done = here()
    code[jz + 1] = done - (base + 9 + jz + 2)
    code += b'\x5f\x5e\x8b\xe5\x5d\xcb'
    recs = [(F + 9, bytes(code))]
    # 影つき文字列 (イメージ 0x8b58) を DLL へ
    S = 0x2800 + 0x8b58
    assert orig[S:S + 5] == bytes.fromhex('558bec5657')
    recs.append((S, cb(3) + bytes([0xcb])))
    # 会話の選択ボタンの y を決める所（オーバーレイ 33d2:0462 の mov al,[8101]; cbw）を DLL へ:
    # 原作は y = 窓の y + (行+1) × 行の高さ + 4。本文は和文の行の高さで出しているので、DLL がその高さで数える
    B = 0x3ded2
    assert orig[B - 5:B + 18] == bytes.fromhex('a154fe4050a00181988bd058f7ea8b1666fe03d083c204')
    recs.append((B, cb(4)))
    # 再配置表の確認（書き換え範囲にかからないこと）
    nrel, rt = struct.unpack_from('<H', orig, 6)[0], struct.unpack_from('<H', orig, 0x18)[0]
    for i in range(nrel):
        o, s = struct.unpack_from('<HH', orig, rt + 4 * i)
        a = 0x2800 + s * 16 + o
        for off, b in recs:
            assert not (off <= a < off + len(b) or off <= a + 1 < off + len(b)), hex(a)
    return orig, recs

def ovl_patch():
    """MCGA.OVL: 文字列描画 (seg:2543) を DLL の描画に置き換える"""
    orig = open(os.path.join(GAME, 'MCGA.OVL'), 'rb').read()
    F = 0x200 + 0x2543
    assert orig[F:F + 8] == bytes.fromhex('558bec505351571e')
    return orig, [(F, cb(2) + b'\xcb')]

def xsect(name, orig, recs):
    d = name.encode().ljust(16, b'\0') + struct.pack('<II', len(orig), crc32(orig))
    for off, b in recs:
        d += struct.pack('<II', off, len(b)) + b
    return sect('X', d)

def ksect(pakname, entries):
    """PAK の中身の差し替え: 名前, 元の大きさ, crc, 件数, {名前16, 長さ, データ}"""
    orig = open(os.path.join(GAME, pakname), 'rb').read()
    d = pakname.encode().ljust(16, b'\0') + struct.pack('<III', len(orig), crc32(orig), len(entries))
    for name, b in entries:
        d += name.encode().ljust(16, b'\0') + struct.pack('<I', len(b)) + b
    return sect('K', d)

# ---------------------------------------------------------------- main
def main():
    tdir = sys.argv[1] if len(sys.argv) > 1 else 'ja'
    gl = Glyphs()
    # 記号類は先に入れておく（DLL が使う）
    for ch in '・…「」？！':
        gl.idx(ch)
    exact, pats = {}, {}
    files = sorted(glob.glob(os.path.join(ROOT, 'text', tdir, '*.json')))
    for fn in files:
        d = json.load(open(fn, encoding='utf-8'))
        for en, ja in d.items():
            if en.startswith('_') or ja is None or ja == '':
                continue
            if SPEC.search(en) and re.search(r'%[-+ #0-9.]*[sdxXuc]', en):
                pats[enc_pattern_key(en)] = (en, ja)
            else:
                k = norm_key(en).encode('latin1')
                # 大文字小文字だけ違う原文で訳が違うときは、区別する表（先頭 0x01）にも入れる
                cs = b'' + norm_key(en, False).encode('latin1')
                exact[cs] = (en, ja)
                if k not in exact:
                    exact[k] = (en, ja)
    tdata = bytearray(struct.pack('<I', len(exact)))
    for k, (en, ja) in sorted(exact.items()):
        v = enc_value(ja, gl, False)
        tdata += struct.pack('<H', len(k)) + k + struct.pack('<H', len(v)) + struct.pack('<%dH' % len(v), *v)
    qdata = bytearray(struct.pack('<I', len(pats)))
    # 長いキーから順に試す（同じ長さなら、文字列の引数 %s が少ないものを先に: "%3d of %-3d" を "%s of %s" より先に）
    for k, (en, ja) in sorted(pats.items(), key=lambda kv: (-len(kv[0]), kv[0].count(1))):
        v = enc_value(ja, gl, True)
        qdata += struct.pack('<H', len(k)) + k + struct.pack('<H', len(v)) + struct.pack('<%dH' % len(v), *v)
    # 字形
    n = len(gl.order)
    fdata = bytearray(struct.pack('<I', n))
    for ch in gl.order:
        fdata += gl.bdf[ord(ch)]
    for ch in gl.order:
        fdata.append((1 if ch in NO_START else 0) | (2 if ch in NO_END else 0))
    eo, er = exe_patch()
    oo, orr = ovl_patch()
    secs = [sect('P', struct.pack('<II', CB_BASE, LOG)), sect('F', bytes(fdata)), sect('T', bytes(tdata)),
            sect('Q', bytes(qdata)), xsect('EOB.EXE', eo, er), xsect('MCGA.OVL', oo, orr)]
    tc = os.path.join(ROOT, 'build', 'TEXT.CMP')
    if os.path.exists(tc):
        secs.append(ksect('EOBDATA1.PAK', [('TEXT.CMP', open(tc, 'rb').read())]))
    blob = b'EJP1' + struct.pack('<II', 1, len(secs)) + b''.join(secs)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin %d bytes: %d exact, %d patterns, %d glyphs' % (len(blob), len(exact), len(pats), n))

if __name__ == '__main__':
    main()
