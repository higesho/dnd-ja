# build/patch.bin（winmm.dll に埋め込むデータ）を作る
#   python -X utf8 tools/make_patch.py      （DQ_LOG=0 でログなし）
import sys, os, json, struct, glob
sys.path.insert(0, os.path.dirname(__file__))
from mp_helpers import *

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BDF = os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf')
CB_BASE = 0x70
LOG = int(os.environ.get('DQ_LOG', '1'))

def load_translations():
    """text/ja.json（英語の断片 → 訳）と text/fixes.json（直し）"""
    tr = {}
    global MENU
    MENU = set()
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.json'))):
        en = json.load(open(f.replace('.ja.json', '.en.json'), encoding='utf-8'))
        ja = json.load(open(f, encoding='utf-8'))
        for it in en['items']:
            t = ja.get(it['id'])
            if t and t != '=':
                tr[it['en']] = t
                # ECL の選択肢（"PALACE" など）はゲームが "Palace" の形にしてボタンにする
                if it['src'].startswith('ECL') and len(it['en']) <= 30 and it['en'].isupper() and len(en_w := it['en'].split()) <= 4:
                    for v in (' '.join(w.capitalize() for w in en_w), it['en'].capitalize()):
                        tr.setdefault(v, t)
                    # 選択肢の語は sprintf でボタンの並びに足される（"%s%s" で "Leave" など）。そこで訳すと頭の英字が消えて
                    # ボタンが壊れる（v1.3 まで）。旗 4 = 組み立て中は訳さず、描くときに訳す
                    if all(w.isalpha() for w in en_w):
                        MENU.update({it['en'], ' '.join(w.capitalize() for w in en_w), it['en'].capitalize()})
            # メニュー（"Play Demo Quit" のように大文字で始まる語を空白で並べた文字列）: ゲームが空白で区切って
            # ボタンを作るので、訳の語数が同じなら語ごとの対応も足す
            en_w = it['en'].split(' ')
            if t and t != '=' and it['src'].startswith('EXE') and len(en_w) >= 2 and all(w[:1].isupper() and w.isalpha() for w in en_w):
                ja_w = t.replace('　', ' ').split()
                MENU.add(it['en'])
                MENU.update(en_w)
                if len(ja_w) == len(en_w):
                    for a, b in zip(en_w, ja_w):
                        tr.setdefault(a, b)
    p = os.path.join(ROOT, 'text', 'ja.json')
    if os.path.exists(p):
        tr.update(json.load(open(p, encoding='utf-8')))
    p = os.path.join(ROOT, 'text', 'monsters.json')       # 怪物の名前（DISK2/MONCHA.GLB）
    if os.path.exists(p):
        for k, v in json.load(open(p, encoding='utf-8')).items():
            tr[k] = v
            tr.setdefault(k.title(), v)
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p):
        tr.update(json.load(open(p, encoding='utf-8')))
    return {k: v for k, v in tr.items() if v != '='}

def main():
    codec = Codec()
    gl = load_bdf(BDF)
    sects = []
    table = load_translations()
    if os.environ.get('DQ_EMPTY') == '1':      # テスト用: 訳なし（英語のまま。フックとキー入力は動く）
        table = {}; MENU.clear()
    tb = struct.pack('<I', len(table))
    for en, ja in table.items():
        e = en.encode('latin1')
        jb = codec.encode(ja) if ja != '-' else b''
        tb += struct.pack('<BBHHQ', 0, 4 if en in MENU else 0, len(jb), len(e), fnv64(e)) + jb   # 旗 4: メニューのボタンの語
    sects.append(sect('T', tb))
    codes, bits, flags = [], b'', b''
    for ch, cd in sorted(codec.map.items(), key=lambda kv: kv[1]):
        g = gl.get(ord(ch))
        if g is None:
            print('グリフなし', ch); g = bytes(8)
        codes.append(struct.unpack('>H', cd)[0]); bits += g
        flags += bytes([(1 if ch in NO_START else 0) | (2 if ch in NO_END else 0)])
    sects.append(sect('F', struct.pack('<I', len(codes)) + struct.pack('<%dH' % len(codes), *codes) + bits + flags))
    sects.append(sect('P', struct.pack('<2I', CB_BASE, LOG)))
    blob = b'DQJ1' + struct.pack('<II', 1, len(sects)) + b''.join(sects)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin', len(blob), 'bytes,', len(table), 'entries,', len(codes), 'glyphs')

if __name__ == '__main__':
    main()
