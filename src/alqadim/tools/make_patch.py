# build/patch.bin（winmm.dll に埋め込むデータ）を作る
#   python -X utf8 tools/make_patch.py        （AQ_LOG=0 でログなし）
# 節:
#   'T' 訳の表（英語のハッシュ → 独自符号の日本語）
#   'F' 字形: u32 n, n*u16 符号, n*8 美咲 8x8, n*24 東雲 12x12（1 行 2 バイト）, n 旗（1=行頭禁則, 2=行末禁則）
#   'E' GENIER.EXE の書き換え: u32 元の大きさ, u32 個数, {u32 位置, u8 長さ, u8 種類(0=バイト列,1=コールバック), u8 番号} + 元 + 新
#   'B' かぎかっこ「」の字形: 8x8 を 2 つ, 12x12 を 2 つ
#   'S' SYSFILE.LIB の項目の差し替え: u32 個数, {u32 項目番号, u32 長さ, データ}
#   'P' u32 コールバック番号の始まり, u32 ログ
import sys, os, json, struct, glob, re
sys.path.insert(0, os.path.dirname(__file__))
from le import LE

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'GENIE')
CB_BASE = 0x70
LOG = int(os.environ.get('AQ_LOG', '1'))

NO_START = set('、。，．・：；？！ー…‥」』）］｝〕〉》】ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々〜～')
NO_END = set('「『（［｛〔〈《【')
TRAILS = [c for c in range(0xc0, 0xff) if c not in (0xd2, 0xd3, 0xfa)]     # 0xd2/0xd3 は「」、0xfa は選択肢の黒丸、0xff は描画用の枠

# GENIER.EXE の書き換え（リンク時の番地, 元のバイト列, 新しいバイト列 or ('cb', k)）
EPATCH = [
    (0x23c95, '8945fc8b45fc', ('cb', 0)),          # GPL の文字列を取り出した後: 訳があれば日本語に
    (0x1fef1, 'b82c010000', 'b800080000'),          # その作業領域 300 → 2048 バイト
    (0x43184, '53515256', ('cb', 1)),              # 1 字描画の入口
    (0x431b9, '8b559c01c2', ('cb', 2)),            # 1 字描画: フォントの位置が分かった所
    (0x43933, 'ff45a08b45a48945c4', ('cb', 3)),    # 折り返し: 1 字ごとの判定（窓を描く関数）
    (0x43bcf, 'ff45e48b45d48945cc', ('cb', 4)),    # 折り返し: 1 字ごとの判定（1 行の字数を数える関数）
]

# EXE の文字列の訳を入れる: 元の場所に入らないものは、ふだん使わないエラー文の場所へ移して参照先を書き換える
POOL_RANGES = [(0x61128, 0x61500), (0x61774, 0x61865)]
# extract.py の範囲のほかに差し替える所（訳は fixes.json / EXE_OVERRIDE）
EXE_EXTRA_RANGES = [(0x604b5, 0x604c1), (0x61067, 0x6106c)]
EXE_OVERRIDE = {
    0x604b6: ' はい。', 0x604bd: ' いいえ。',      # 確認の選択肢（頭に黒丸 0xFA）
    0x61021: '難易度：',                          # 名前を付ける画面の Level:（能力値画面の Level: とは別）
    0x61067: '指輪',
}
# 同じ文字列を指す参照のうち、この位置（参照元のリンク時の番地）の参照だけ別の訳に付け替える
EXE_RETARGET = {
    0x65368: 'レベル：',          # 能力値画面の Level:（名前を付ける画面の 0x6533c は「難易度：」のまま）
}


def exe_string_patches(L, table, codec):
    import re as _re
    img = bytes(L.img[1]); base = 0x60000
    units = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    pool = []
    for a, b in POOL_RANGES:
        for m in _re.finditer(rb'[\x20-\x7e]+\x00', img[a - base:b - base]):
            t = m.group()[:-1]
            if len(t) >= 10 and (b' ' in t or b'%' in t):
                s0, s1 = a - base + m.start(), a - base + m.end()
                if pool and all(c == 0 for c in img[pool[-1][1]:s0]): pool[-1][1] = s1     # 隣り合う文はつなげる
                else: pool.append([s0, s1])
    tgt_fix = {}
    for k, f in enumerate(L.fix):
        if f[3] == 1: tgt_fix.setdefault(f[4], []).append(k)
    fpos = {k: (pos, sz) for k, pos, sz in L.fixpos}
    retarget = {}          # fixup の添字 → 訳（EXE_RETARGET）。これらは下の移動では書き換えない
    for src_va, t in EXE_RETARGET.items():
        ks = [k for k, f in enumerate(L.fix) if f[0] == 1 and f[1] == src_va - base and f[3] == 1]
        assert len(ks) == 1, hex(src_va)
        retarget[ks[0]] = t
    for k in retarget:
        tgt_fix[L.fix[k][4]].remove(k)
    out = []        # (ファイル上の位置, 元, 新)
    def put_img(o, data):
        va = base + o
        for i in range(0, len(data), 200):
            fo = L.file_off(va + i)
            out.append((fo, L.d[fo:fo + len(data[i:i + 200])], data[i:i + 200]))
    # 範囲の中の文字列をすべて（同じ英語が 2 か所にあるものも）差し替える。EXE_OVERRIDE は場所ごとの訳
    import extract
    todo = []
    for lo, hi in extract.EXE_RANGES + EXE_EXTRA_RANGES:
        for m in _re.finditer(rb'[\x20-\x7e]+', img[lo - base:hi - base]):
            en = m.group().decode('latin1'); va = lo + m.start()
            t = EXE_OVERRIDE.get(va, table.get(en))
            if t: todo.append((va, en, t))
    for va, en, t in todo:
        a = va - base
        pre = b''
        if a > 0 and img[a - 1] == 0xfa: a -= 1; pre = b'\xfa'
        e = pre + en.encode('latin1')
        assert img[a:a + len(e)] == e, hex(va)
        j = pre + codec.encode(t) + b'\x00'
        room = len(e) + 1
        while a + room < len(img) and img[a + room] == 0 and (a + room) not in tgt_fix: room += 1
        if len(j) <= room:
            put_img(a, j + bytes(room - len(j)))
            continue
        blk = next((b for b in pool if b[1] - b[0] >= len(j)), None)
        if blk is None or a not in tgt_fix:
            print('EXE の訳が入らない:', en, table[en]); continue
        na = blk[0]; blk[0] += len(j)
        put_img(na, j)
        for k in tgt_fix[a]:
            pos, sz = fpos[k]
            out.append((pos, L.d[pos:pos + sz], na.to_bytes(sz, 'little')))
    lv = os.environ.get('AQ_TESTLEVEL')        # 試験用: 新しいゲームの最初の場面（表の 13 番 OPENER）を別の場面にする
    if lv:
        ks = [k for k, f in enumerate(L.fix) if f[0] == 1 and f[1] == 0x1bc6 + 13 * 4 and f[3] == 1]
        pos, sz = fpos[ks[0]]
        tgt = L.fix[ks[0]][4]
        new = struct.unpack_from('<I', bytes(L.img[1]), 0x1bc6 + int(lv) * 4)[0] - base
        out.append((pos, L.d[pos:pos + sz], new.to_bytes(sz, 'little')))
        print('TEST: start level 13 ->', lv, hex(new))
    for k, t in retarget.items():
        j = codec.encode(t) + b'\x00'
        blk = next(b for b in pool if b[1] - b[0] >= len(j))
        na = blk[0]; blk[0] += len(j)
        put_img(na, j)
        pos, sz = fpos[k]
        out.append((pos, L.d[pos:pos + sz], na.to_bytes(sz, 'little')))
    return out


def bdf8(path):
    """美咲（Unicode）: {code: 8 bytes}"""
    g = {}; cur = None
    L = open(path, encoding='latin1').read().split('\n'); i = 0
    while i < len(L):
        l = L[i]
        if l.startswith('ENCODING'): cur = int(l.split()[1])
        elif l.startswith('BBX'): w, h, xo, yo = map(int, l.split()[1:])
        elif l == 'BITMAP':
            rows = [0] * 8; top = 7 - yo - h
            for r in range(h):
                v = int(L[i + 1 + r], 16) >> xo
                if 0 <= top + r < 8: rows[top + r] = v & 0xff
            g[cur] = bytes(rows); i += h
        i += 1
    return g


def bdf12(path):
    """東雲 12（JIS X 0208 の符号）: {unicode: 24 bytes (2 bytes per row, bit15 = left)}"""
    g = {}; cur = None
    L = open(path, encoding='latin1').read().split('\n'); i = 0
    while i < len(L):
        l = L[i]
        if l.startswith('ENCODING'): cur = int(l.split()[1])
        elif l.startswith('BBX'): w, h, xo, yo = map(int, l.split()[1:])
        elif l == 'BITMAP':
            rows = [0] * 12; top = 12 - 2 - yo - h     # FONTBOUNDINGBOX 12 12 0 -2: 基線は下から 2
            nb = (w + 7) // 8
            for r in range(h):
                v = int(L[i + 1 + r], 16) << (16 - nb * 8) >> xo
                if 0 <= top + r < 12: rows[top + r] = v & 0xffff
            try:
                ch = bytes([(cur >> 8) | 0x80, (cur & 0xff) | 0x80]).decode('euc_jp')
                g[ord(ch)] = b''.join(struct.pack('>H', x) for x in rows)
            except Exception:
                pass
            i += h
        i += 1
    return g


class Codec:
    """日本語の字に 2 バイトの独自符号（1 バイト目 0x80-0xbf, 2 バイト目 TRAILS）を割り当てる"""
    def __init__(self):
        self.map = {}
    def code(self, ch):
        if ch not in self.map:
            n = len(self.map)
            assert n < 64 * len(TRAILS), 'too many glyphs'
            self.map[ch] = bytes([0x80 + n // len(TRAILS), TRAILS[n % len(TRAILS)]])
        return self.map[ch]
    def encode(self, s):
        out = bytearray()
        for ch in s:
            o = ord(ch)
            if ch == '「': out.append(0x7b); continue          # 描画で 0xd2（「の字形）になる
            if ch == '」': out.append(0x7d); continue
            if 0xff01 <= o <= 0xff5e and ch not in '！？（）：、。・ー':
                ch = chr(o - 0xfee0); o = ord(ch)
            if ch == '　': ch = ' '; o = 0x20
            if ch == '—': ch = '―'; o = ord(ch)
            if o < 0x80: out.append(o)
            else: out += self.code(ch)
        return bytes(out)


def wrap_ja(t, n):
    """日本語を 1 行 n 字で折り返す（行頭禁則の字は前の行にぶら下げる）"""
    lines, cur = [], ''
    for ch in t:
        if len(cur) >= n and ch not in NO_START and (not cur or cur[-1] not in NO_END):
            lines.append(cur); cur = ''
        cur += ch
    if cur: lines.append(cur)
    return lines


def intro_entry(codec):
    d = json.load(open(os.path.join(ROOT, 'text', 'intro.json'), encoding='utf-8'))
    out = b''
    for para in d['paras']:
        for l in wrap_ja(para, d.get('width', 24)):
            out += codec.encode(l) + bytes([13, 10])
        out += bytes([13, 10])
    # ゲームは元の長さ（706 バイト）ぶん表示するので、短いときは改行で埋める（埋めないと続きのメモリが出る）
    import lib
    sysb = open(os.path.join(GAME, 'ENGINE', 'SYSFILE.LIB'), 'rb').read()
    olen = lib.entries(sysb)[17][1]
    while len(out) < olen: out += bytes([13, 10])
    return out + bytes(4)


def fnv64(b):
    h = 14695981039346656037
    for c in b: h = ((h ^ c) * 1099511628211) & 0xffffffffffffffff
    return h


def sect(t, d):
    return struct.pack('<II', ord(t), len(d)) + d


def build_epatch(table, codec):
    L = LE()
    exe = L.d
    extra = exe_string_patches(L, table, codec)
    out = struct.pack('<II', len(exe), len(EPATCH) + len(extra))
    for fo, o, n in extra:
        out += struct.pack('<IBBB', fo, len(o), 0, 0) + o + n
    for va, orig, new in EPATCH:
        off = L.file_off(va)
        o = bytes.fromhex(orig)
        assert exe[off:off + len(o)] == o, (hex(va), exe[off:off + len(o)].hex())
        assert not [f for f in L.fix if f[0] == 0 and va - 4 < f[1] + 0x10000 < va + len(o)], hex(va)
        if isinstance(new, tuple):
            n = bytes(len(o)); kind, k = 1, new[1]
        else:
            n = bytes.fromhex(new); kind, k = 0, 0
            assert len(n) == len(o)
        out += struct.pack('<IBBB', off, len(o), kind, k) + o + n
    return out


def load_translations():
    tr = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.json'))):
        en = json.load(open(f.replace('.ja.json', '.en.json'), encoding='utf-8'))
        ja = json.load(open(f, encoding='utf-8'))
        for it in en['items']:
            t = ja.get(it['id'])
            if t and t != '=': tr[it['en']] = t
    rp = os.path.join(ROOT, 'text', 'replace.json')      # 訳者ごとの表記ゆれを 1 つに
    if os.path.exists(rp):
        reps = json.load(open(rp, encoding='utf-8'))
        for k in tr:
            for a, b in reps.items():
                if a in tr[k]: tr[k] = tr[k].replace(a, b)
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p): tr.update(json.load(open(p, encoding='utf-8')))
    return {k: v for k, v in tr.items() if v != '='}


def main():
    codec = Codec()
    g8 = bdf8(os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf'))
    g12 = bdf12(os.path.join(ROOT, 'font', 'shnmk12.bdf'))
    table = load_translations()
    sects = []
    epatch = build_epatch(table, codec)
    intro = intro_entry(codec)            # 字形の表（'F'）を作る前に、使う字をすべて符号にしておく
    cp = codec.encode('（答え：') + b'\x00' + codec.encode('）') + b'\x00'
    tb = struct.pack('<I', len(table))
    for en, ja in table.items():
        e = en.encode('latin1')
        jb = codec.encode(ja)
        tb += struct.pack('<BBHHQ', 0, 0, len(jb), len(e), fnv64(e)) + jb
    sects.append(sect('T', tb))
    codes, b8, b12, flags = [], b'', b'', b''
    for ch, cd in sorted(codec.map.items(), key=lambda kv: kv[1]):
        a = g8.get(ord(ch)); b = g12.get(ord(ch))
        if a is None: print('8x8 なし', ch); a = bytes(8)
        if b is None: print('12x12 なし', ch); b = bytes(24)
        codes.append(struct.unpack('>H', cd)[0]); b8 += a; b12 += b
        flags += bytes([(1 if ch in NO_START else 0) | (2 if ch in NO_END else 0)])
    sects.append(sect('F', struct.pack('<I', len(codes)) + struct.pack('<%dH' % len(codes), *codes) + b8 + b12 + flags))
    sects.append(sect('B', g8[ord('「')] + g8[ord('」')] + g12[ord('「')] + g12[ord('」')]))
    sects.append(sect('E', epatch))
    sects.append(sect('C', cp))
    sects.append(sect('S', struct.pack('<III', 1, 17, len(intro)) + intro))
    sects.append(sect('P', struct.pack('<2I', CB_BASE, LOG)))
    blob = b'AQJ1' + struct.pack('<II', 1, len(sects)) + b''.join(sects)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin', len(blob), 'bytes,', len(table), 'entries,', len(codes), 'glyphs')


if __name__ == '__main__':
    main()
