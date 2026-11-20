# build/patch.bin（winmm.dll に埋め込むデータ）を作る
#   python -X utf8 tools/make_patch.py
import sys, os, json, struct, zlib, glob, re
sys.path.insert(0, os.path.dirname(__file__))
import wwlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'DARKMOON')
BDF = os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf')
CB_BASE = 0x70
CB_PRINT, CB_WRAP, CB_WPRINT, CB_FETCH = CB_BASE, CB_BASE + 1, CB_BASE + 2, CB_BASE + 3
LOG = int(os.environ.get('EJP_LOG', '1'))

NO_START = set('、。，．・：；？！ー…‥」』）］｝〕〉》】ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々〜～')
NO_END = set('「『（［｛〔〈《【')

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

def cb(n):
    return bytes([0xfe, 0x38, n & 0xff, n >> 8])

class Codec:
    """日本語の文字に 2 バイトの独自符号を割り当てる（1バイト目 0x81-0xfe, 2バイト目 0x80-0xfe）"""
    def __init__(self):
        self.map = {}
    def code(self, ch):
        if ch not in self.map:
            n = len(self.map)
            self.map[ch] = bytes([0x81 + n // 127, 0x80 + n % 127])
        return self.map[ch]
    def encode(self, s):
        out = bytearray()
        for ch in s:
            o = ord(ch)
            if 0xff01 <= o <= 0xff5e and ch not in '！？（）％：、。「」・ー':
                ch = chr(o - 0xfee0); o = ord(ch)
            if ch == '　':
                ch = ' '; o = 0x20
            if o < 0x80:
                out.append(o)
            else:
                out += self.code(ch)
        return bytes(out)

SPEC = re.compile(r'%%|%[-+ #0]*[0-9]*(?:\.[0-9]+)?l?[dsuxic]')

def encode_ja(codec, ja):
    """訳文の %s/%d を順番どおり %1,%2.. に（%2$s のような番号指定も可）"""
    out, k = [], 0
    pos = 0
    for m in re.finditer(r'%%|%([1-9])\$[-0-9]*l?[dsuxic]|%[-+ #0]*[0-9]*l?[dsuxic]', ja):
        out.append(codec.encode(ja[pos:m.start()]))
        if m.group(0) == '%%':
            out.append(b'%%')
        elif m.group(1):
            out.append(b'%' + m.group(1).encode())
        else:
            k += 1
            out.append(b'%' + str(k).encode())
        pos = m.end()
    out.append(codec.encode(ja[pos:]))
    return b''.join(out)

# オープニング・エンディングの文: ゲームが改行で切って 1 行ずつ中央寄せで描くので、訳も行ごとに対応させる
CENTERED = (0x28260, 0x29960)
CR = chr(13)

def load_translations():
    """text/ja/*.json（{英語: 訳}）を全部まとめる。値は (訳, 旗)"""
    table = {}
    cent = set()
    for s in json.load(open(os.path.join(ROOT, 'text', 'en', 'START.EXE.json'), encoding='utf-8'))['strings']:
        if CENTERED[0] <= int(s['id'][4:], 16) < CENTERED[1]:
            cent.add(s['en'])
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'ja', '*.json'))):
        for en, ja in json.load(open(f, encoding='utf-8')).items():
            if ja is None or ja == '':
                continue
            if en in cent:
                el = en.split(CR)
                jl = [x.strip() for x in ja.split(CR) if x.strip()]
                if len(jl) > len(el):
                    jl = jl[:len(el) - 1] + [''.join(jl[len(el) - 1:])]
                jl += [''] * (len(el) - len(jl))
                for e, j in zip(el, jl):
                    table[e.encode('latin1')] = (j, 1)
                continue
            table[en.encode('latin1')] = (ja, 0)
    return sorted(table.items())

def fnv64(b):
    h = 14695981039346656037
    for c in b:
        h = ((h ^ c) * 1099511628211) & 0xffffffffffffffff
    return h

SPECRE = re.compile(rb'%([-+ #0.l]*)([0-9]*)([a-zA-Z])')

def pattern_tokens(en):
    """書式（%s など）を含む英語なら部品の並び [(0, 固定バイト列) | (1, 変換, 幅, 左寄せ)] を返す。含まなければ None"""
    toks, lit, i, nspec, nlit = [], b'', 0, 0, 0
    while i < len(en):
        if en[i:i + 2] == b'%%':
            lit += b'%'; i += 2; continue
        m = SPECRE.match(en, i) if en[i:i + 1] == b'%' else None
        if m:
            if lit:
                toks.append((0, lit)); lit = b''
            toks.append((1, m.group(3).decode(), int(m.group(2) or 0), 1 if b'-' in m.group(1) else 0))
            nspec += 1
            i = m.end(); continue
        if en[i] > 0x20:
            nlit += 1
        lit += en[i:i + 1]; i += 1
    if lit:
        toks.append((0, lit))
    if nspec and (nlit >= 1 or nspec >= 2):
        return toks
    return None

def sect(t, data):
    return struct.pack('<II', ord(t) if isinstance(t, str) else t, len(data)) + data

def name16(n):
    return n.encode().ljust(16, b'\0')

def xsect(fname, orig, patches):
    body = struct.pack('<II', len(orig), zlib.crc32(orig))
    for off, b in sorted(patches):
        body += struct.pack('<II', off, len(b)) + b
    return sect('X', name16(fname) + body)

def ssect(fname, orig, kind, splices):
    body = struct.pack('<IIII', len(orig), zlib.crc32(orig), kind, len(splices))
    for a, e, b in sorted(splices):
        body += struct.pack('<III', a, e, len(b)) + b
    return sect('S', name16(fname) + body)

def main():
    codec = Codec()
    gl = load_bdf(BDF)
    sects = []
    exe = open(os.path.join(ROOT, 'orig', 'START.EXE'), 'rb').read()
    p = []
    # Window_Print の入口: 書式と引数から文を組み立てて訳す（lea ax,[bp+0a] / mov [bp-12],ss の代わり）
    assert exe[0x24f8b:0x24f91] == bytes.fromhex('8d460a8c56ee')
    p.append((0x24f8b, cb(CB_WPRINT) + bytes([0x90, 0x90])))
    # Window_Print の文字取得: DLL が組み立てた文から 1 バイトずつ返す
    assert exe[0x255b4:0x255b9] == bytes.fromhex('558bec4c4c')
    p.append((0x255b4, cb(CB_FETCH) + bytes([0xcb])))
    # Window_Print の改行位置: コールバックで決めて 25467 へ
    assert exe[0x2542a:0x2542e] == bytes.fromhex('8b3654f2')
    p.append((0x2542a, cb(CB_WRAP) + bytes([0xeb, 0x25467 - 0x25430]) + bytes([0x90]) * (0x25467 - 0x25430)))
    sects.append(xsect('START.EXE', exe, p))

    mga = open(os.path.join(GAME, 'MGA.OVL'), 'rb').read()
    assert mga[0x2636:0x2639] == bytes.fromhex('558bec')
    sects.append(xsect('MGA.OVL', mga, [(0x2636, cb(CB_PRINT) + b'\xcb')]))

    f6 = open(os.path.join(GAME, 'FONT6.FNT'), 'rb').read()
    sects.append(xsect('FONT6.FNT', f6, [(0x102, bytes([8, 4]))]))
    f8 = open(os.path.join(GAME, 'FONT8.FNT'), 'rb').read()
    sects.append(xsect('FONT8.FNT', f8, [(0x102, bytes([8, 4]))]))

    # 翻訳表
    table = load_translations()
    tb = struct.pack('<I', len(table))
    for en, (ja, flags) in table:
        jb = encode_ja(codec, ja)
        toks = pattern_tokens(en)
        tb += struct.pack('<BBHHQ', 1 if toks else 0, flags, len(jb), len(en), fnv64(en))
        if toks:
            tb += struct.pack('<H', len(toks))
            for t in toks:
                if t[0] == 0:
                    tb += struct.pack('<BHQ', 0, len(t[1]), fnv64(t[1]))
                else:
                    tb += struct.pack('<BBBB', 1, ord(t[1]), t[2], t[3])
        tb += jb
    sects.append(sect('T', tb))

    # フォント
    codes, bits, flags = [], b'', b''
    for ch, cd in sorted(codec.map.items(), key=lambda kv: kv[1]):
        g = gl.get(ord(ch))
        if g is None:
            print('グリフなし', ch); g = bytes(8)
        codes.append(struct.unpack('>H', cd)[0]); bits += g
        flags += bytes([(1 if ch in NO_START else 0) | (2 if ch in NO_END else 0)])
    sects.append(sect('F', struct.pack('<I', len(codes)) + struct.pack('<%dH' % len(codes), *codes) + bits + flags))
    half = b''
    for c in range(128):
        half += gl.get(c, bytes(8)) if 0x20 <= c < 0x7f else bytes(8)
    sects.append(sect('H', half))
    sects.append(sect('P', struct.pack('<II', CB_BASE, LOG)))
    blob = b'EJP2' + struct.pack('<II', 1, len(sects)) + b''.join(sects)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    json.dump({k: v.hex() for k, v in codec.map.items()}, open(os.path.join(ROOT, 'build', 'charmap.json'), 'w', encoding='utf-8'), ensure_ascii=False)
    print('patch.bin', len(blob), 'bytes,', len(codes), 'glyphs')

if __name__ == '__main__':
    main()
