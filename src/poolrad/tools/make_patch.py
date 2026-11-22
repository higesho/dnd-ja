# build/patch.bin（winmm.dll に埋め込むデータ）を作る
#   python -X utf8 tools/make_patch.py
import sys, os, json, struct, zlib, glob, re
sys.path.insert(0, os.path.dirname(__file__))
from mp_helpers import *
import exepack

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BDF = os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf')
CB_BASE = 0x70
CB_PRINT, CB_WPRINT, CB_DRAW, CB_MENU = CB_BASE, CB_BASE + 1, CB_BASE + 2, CB_BASE + 3
CB_PAUSE = CB_BASE + 4
LOG = int(os.environ.get('PRJ_LOG', '1'))
GAMECFG = json.load(open(os.path.join(ROOT, 'game.json'), encoding='utf-8'))
H = lambda k: int(GAMECFG[k], 16)

# メインメニュー（ゲームが頭の 1 字をホットキーの色で別に描き、残りを続けて描く）
SPLIT_FIRST = GAMECFG.get('split_first') or ['Create New Character', 'Drop Character', 'Modify Character', 'Train Character', 'View Character',
               'Add Character to Party', 'Remove Character from Party', 'Load Saved Game', 'Save Current Game',
               'BEGIN Adventuring', 'Exit to DOS']

def load_translations():
    """text/chunks/cNN.ja.json と index.json から {英語のバイト列: (訳, 旗)} を作る。
    旗 1 = 1 行表示で中央寄せし直す。メニュー項目は頭に  を付けた英語で、訳の頭にホットキーの英字を付ける"""
    table = {}
    center = set()
    cf = os.path.join(ROOT, 'text', 'center.json')
    if os.path.exists(cf):
        center = set(json.load(open(cf, encoding='utf-8')))
    index = json.load(open(os.path.join(ROOT, 'text', 'chunks', 'index.json'), encoding='utf-8'))
    ja_all = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.json'))):
        ja_all.update(json.load(open(f, encoding='utf-8')))
    fixes = os.path.join(ROOT, 'text', 'fixes.json')          # 実機で見て直した訳（英語: 訳）
    fix = json.load(open(fixes, encoding='utf-8')) if os.path.exists(fixes) else {}
    by_en = {}
    for i, info in index.items():
        ja = ja_all.get(i)
        if not ja:
            continue
        en = info['en'].strip()
        if ja == '-':            # つないだ文の訳を前の項目に入れたので、この断片は何も出さない
            table[en.encode('latin1')] = ('', 0)
            continue
        if info['kind'] == 'menu':
            ja = fix.get('menu:' + en, ja)
            hk = next((c for c in en if c.isupper()), en[0])
            table[b'\x01' + en.encode('latin1')] = (hk + ja, 0)
            continue
        ja = fix.get(en, ja)
        by_en[en] = ja
        table[en.encode('latin1')] = (ja, 1 if en in center else 0)
    for en, ja in fix.items():
        if en.startswith('menu:'):
            w = en[5:]
            hk = next((c for c in w if c.isupper()), w[0])
            table[b'\x01' + w.encode('latin1')] = (hk + ja, 0)
            continue
        table[en.encode('latin1')] = (ja, 1 if en in center else 0)
        by_en[en] = ja
    table[b'@ANSWER'] = ('（答え：%s）', 0)
    table[b'@NEXT'] = ('Enterで次へ', 0)      # Enter 待ちの目印（8 字）
    # メインメニューの残りの部分
    for en in SPLIT_FIRST:
        if en in by_en:
            table.setdefault(en[1:].encode('latin1'), (by_en[en], 0))
    # 怪物名の複数形
    # 表（アイテム名の部品など）の複数形
    data = json.load(open(os.path.join(ROOT, 'text', 'en', 'data.json'), encoding='utf-8'))['items']
    for it in data:
        en = it['en'].strip()
        if en in by_en and re.fullmatch(r"[A-Za-z][A-Za-z' -]*[a-z]", en):
            for pl in (en + 's', en + 'es'):
                table.setdefault(pl.encode('latin1'), (by_en[en], 0))
    names = json.load(open(os.path.join(ROOT, 'text', 'en', 'names.json'), encoding='utf-8'))['items']
    for it in names:
        en = it['en'].strip()
        if en in by_en:
            for pl in (en + 'S', en + 'ES', en[:-1] + 'IES' if en.endswith('Y') else None, en[:-3] + 'MEN' if en.endswith('MAN') else None):
                if pl:
                    table.setdefault(pl.encode('latin1'), (by_en[en], 0))
    return sorted(table.items())

def esect(fname, orig, patches):
    body = struct.pack('<II', len(orig), zlib.crc32(orig))
    for off, b in sorted(patches):
        body += struct.pack('<II', off, len(b)) + b
    return sect('E', name16(fname) + body)

def main():
    codec = Codec()
    gl = load_bdf(BDF)
    sects = []
    exe = open(os.path.join(ROOT, 'orig', 'START.EXE'), 'rb').read()
    packed = exepack.is_packed(exe)
    if packed:
        img, rel, regs = exepack.unpack(exe)
    else:                                  # 圧縮なし: イメージはヘッダの後ろ、再配置表は MZ のもの
        hdr = struct.unpack_from('<H', exe, 8)[0] * 16
        img = exe[hdr:]
        n, at = struct.unpack_from('<HH', exe, 6)[0], struct.unpack_from('<H', exe, 0x18)[0]
        rel = [struct.unpack_from('<HH', exe, at + 4 * k) for k in range(n)]
    relpos = set()
    for off, seg in rel:
        a = seg * 16 + off
        relpos.update((a, a + 1))
    p = []
    def patch(off, old, new):
        old = bytes.fromhex(old)
        assert img[off:off + len(old)] == old, hex(off)
        assert len(new) <= len(old)
        assert not any(off + k in relpos for k in range(len(new))), 'reloc at %x' % off
        p.append((off, new))
    patch(H('print_hook'), '8a8600ff', cb(CB_PRINT))            # 1 行表示: mov al,[bp-100]
    patch(H('wprint_hook'), '8a8600ff', cb(CB_WPRINT))          # ワードラップ表示: mov al,[bp-100]
    patch(H('draw_func') + 1, '89e583ec12', cb(CB_DRAW) + b'\x90')  # 1 文字描画: mov bp,sp / sub sp,12
    if 'pause_hook' in GAMECFG:                                 # mov al,[速度] / xor ah,ah / mov dx,225 / mul dx / push ax / call Delay
        sv = H('speed_var')
        patch(H('pause_hook'), 'a0%02x%02x30e4bae100f7e250' % (sv & 0xff, sv >> 8),
              cb(CB_PAUSE) + bytes.fromhex('e304' 'f4' 'ebf7' '90' '50'))
    if packed:
        sects.append(esect('START.EXE', exe, p))
    else:
        sects.append(xsect('START.EXE', exe, [(off + hdr, b) for off, b in p]))

    ovr = open(os.path.join(ROOT, 'orig', 'GAME.OVR'), 'rb').read()
    po = []
    def opatch(off, old, new):
        old = bytes.fromhex(old)
        assert ovr[off:off + len(old)] == old, hex(off)
        assert len(new) <= len(old)
        po.append((off, new + bytes([0x90]) * (len(old) - len(new))))
    mv = H('menu_var')
    opatch(H('menu_hook'), 'c606%02x%02x00' % (mv & 0xff, mv >> 8), cb(CB_MENU))   # 横並びメニュー: mov byte [var],0
    sects.append(xsect('GAME.OVR', ovr, po))

    table = load_translations()
    tb = struct.pack('<I', len(table))
    for en, (ja, flags) in table:
        if en == b'@ANSWER':
            jb = encode_ja(codec, ja)
        else:
            jb = codec.encode(ja.replace('%', '%%'))     # %A などのゲームの記号は文字のまま
        toks = None
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

    codes, bits, flags = [], b'', b''
    for ch, cd in sorted(codec.map.items(), key=lambda kv: kv[1]):
        g = gl.get(ord(ch))
        if g is None:
            print('グリフなし', ch); g = bytes(8)
        codes.append(struct.unpack('>H', cd)[0]); bits += g
        flags += bytes([(1 if ch in NO_START else 0) | (2 if ch in NO_END else 0)])
    sects.append(sect('F', struct.pack('<I', len(codes)) + struct.pack('<%dH' % len(codes), *codes) + bits + flags))
    sects.append(sect('P', struct.pack('<9I', CB_BASE, LOG, H('font_var'), H('cursor_var'), H('wprint_x1'), H('wprint_x2'),
                                       H('print_x'), H('menu_var'), GAMECFG.get('copy_answer', 0))
                      + GAMECFG['dirname'].encode().ljust(32, b'\0')
                      + (struct.pack('<3I', H('print_ip'), H('speed_var'), H('state_var')) + bytes.fromhex(GAMECFG['pause_skip'])
                         if 'pause_hook' in GAMECFG else b'')))
    blob = b'PRJ1' + struct.pack('<II', 1, len(sects)) + b''.join(sects)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin', len(blob), 'bytes,', len(table), 'entries,', len(codes), 'glyphs')

if __name__ == '__main__':
    main()
