# build/patch.bin（winmm.dll に埋め込むデータ）を作る
#   python -X utf8 tools/make_patch.py      （DS_LOG=0 でログなし）
import sys, os, json, struct, glob
sys.path.insert(0, os.path.dirname(__file__))
from mp_helpers import *

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BDF = os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf')
EXE = os.path.join(ROOT, 'test', 'game', 'RAVAGER', 'DSUN.EXE')
CB_BASE = 0x70
LOG = int(os.environ.get('DS_LOG', '1'))
HDR = 0x5200          # DSUN.EXE の MZ ヘッダの大きさ（イメージ上の位置 + HDR = ファイル上の位置）

# DSUN.EXE の書き換え（イメージ上の位置, 元のバイト列, 新しいバイト列 or ('cb', k)）
EPATCH = [
    (0x098f0, '268b165b02', ('cb', 0)),       # GPL の文を復号した後（mov dx,es:[25b]）
    (0x33935, '558bec83ec1c', ('cb', 1)),     # 1 字描画の入口
    (0x30dd1, '837edc007413', ('cb', 2)),     # 折り返し: 語の区切り
    (0x30d12, '3b46ee7703', ('cb', 3)),       # 折り返し: はみ出しの判定
    (0x33ad9, '558bec83ec1e', ('cb', 4)),     # 位置指定の printf
    (0x30bb9, '8a460698', '0fb64606'),        # 字幅: movsx → movzx（mov al,[bp+6]; cbw → movzx ax,byte [bp+6]）
    (0x33a6d, '268a0798', '260fb607'),        # 文字列の幅: mov al,es:[bx]; cbw → movzx ax,byte es:[bx]
    (0x30322, '558bec83ec1a', ('cb', 5)),     # 枠の中の printf（戦闘の欄など）
    (0x2150e, '558bec81ec3002', ('cb', 6)),   # sprintf
    (0x118f1, '558bec1e', ('cb', 7)),         # 描画ライブラリ: 1 字描画
    (0x118ce, '558bec1e', ('cb', 7)),         # 描画ライブラリ: 字幅
    (0x31150, '558bec83ec08', ('cb', 8)),     # 窓に文章を入れる（呪文の説明など）
]

# 会話の窓（NARRATE）は、スクリプトから来た文字列が "MAP" / "BOOK" / "CLOSE" / "END" と一致するか、"DEBUG" で始まるかを比べて
# 命令として扱う（DSUN.EXE の 4a3bf 付近の語）。これらを訳すと命令と分からなくなり、会話が進まなくなる（1 作目 v1.0 の不具合）
NEVER = ('MAP', 'BOOK', 'CLOSE', 'DEBUG', 'END')

def build_epatch():
    exe = open(EXE, 'rb').read()
    out = struct.pack('<II', len(exe), len(EPATCH))
    for img, orig, new in EPATCH:
        off = img + HDR
        o = bytes.fromhex(orig)
        assert exe[off:off + len(o)] == o, (hex(img), exe[off:off + len(o)].hex())
        if isinstance(new, tuple):
            n = bytes(len(o)); kind, k = 1, new[1]
        else:
            n = bytes.fromhex(new); kind, k = 0, 0
            assert len(n) == len(o)
        out += struct.pack('<IBBB', off, len(o), kind, k) + o + n
    return out

def load_translations():
    tr = {}
    p = os.path.join(ROOT, 'text', 'ds1_reuse.json')          # 1 作目と同じ英文は 1 作目の訳を使う
    if os.path.exists(p):
        tr.update(json.load(open(p, encoding='utf-8')))
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.json'))):
        en = json.load(open(f.replace('.ja.json', '.en.json'), encoding='utf-8'))
        ja = json.load(open(f, encoding='utf-8'))
        for it in en['items']:
            t = ja.get(it['id'])
            if t and t != '=':
                if t != '-':
                    t = t.strip()
                    lead = len(it['en']) - len(it['en'].lstrip(' '))
                    if lead >= 2: t = ' ' * lead + t          # 選択肢の頭の空白（字下げ）を残す
                tr[it['en']] = t
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p):
        tr.update(json.load(open(p, encoding='utf-8')))
    for k in NEVER:                       # ゲームが命令として文字列を比べる語（訳すと会話が止まる）
        tr.pop(k, None)
    return {k: v for k, v in tr.items() if v != '='}

def main():
    codec = Codec()
    gl = load_bdf(BDF)
    sects = []
    table = load_translations()
    tb = struct.pack('<I', len(table))
    for en, ja in table.items():
        e = en.encode('latin1')
        jb = codec.encode(ja) if ja != '-' else b''
        tb += struct.pack('<BBHHQ', 0, 0, len(jb), len(e), fnv64(e)) + jb
    sects.append(sect('T', tb))
    codes, bits, flags = [], b'', b''
    for ch, cd in sorted(codec.map.items(), key=lambda kv: kv[1]):
        g = gl.get(ord(ch))
        if g is None:
            print('グリフなし', ch); g = bytes(8)
        codes.append(struct.unpack('>H', cd)[0]); bits += g
        flags += bytes([(1 if ch in NO_START else 0) | (2 if ch in NO_END else 0)])
    sects.append(sect('F', struct.pack('<I', len(codes)) + struct.pack('<%dH' % len(codes), *codes) + bits + flags))
    sects.append(sect('E', build_epatch()))
    sects.append(sect('P', struct.pack('<2I', CB_BASE, LOG)))
    blob = b'DSJ1' + struct.pack('<II', 1, len(sects)) + b''.join(sects)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin', len(blob), 'bytes,', len(table), 'entries,', len(codes), 'glyphs')

if __name__ == '__main__':
    main()
