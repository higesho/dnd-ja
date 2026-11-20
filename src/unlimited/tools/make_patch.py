# build/patch.bin（winmm.dll に埋め込むデータ）を作る
#   python -X utf8 tools/make_patch.py      （UA_LOG=0 でログなし）
import sys, os, json, struct, glob
sys.path.insert(0, os.path.dirname(__file__))
from mp_helpers import *
import uajp

class Codec:
    """訳文 → DLL の中の符号（日本語 1 字 = (0x80 + 区, 0x80 + 点)）"""
    def __init__(self):
        self.map = {}
    def encode(self, s):
        out = bytearray()
        for ch in s:
            o = ord(ch)
            if 0xff01 <= o <= 0xff5e and ch not in '！？（）％：、。「」・ー':
                ch = chr(o - 0xfee0); o = ord(ch)
            if ch == '　':
                ch = ' '; o = 0x20
            if o < 0x80:
                out.append(o); continue
            kt = uajp.kuten(ch)
            if not kt:
                print('JIS にない字', ch); out.append(0x3f); continue
            out += bytes([0x80 + kt[0], 0x80 + kt[1]])
        return bytes(out)


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BDF = os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf')
CB_BASE = 0x70
LOG = int(os.environ.get('UA_LOG', '1'))

def load_translations():
    """英語 → 訳の表を作る。優先順（後のものが勝つ）:
       text/reuse_dqk.json（同じエンジンの Dark Queen of Krynn で訳した画面の文字列）
       → text/chunks/c*.ja.json（今回訳したもの）→ text/names.json と用語集（名前の表）→ text/fixes.json（直し）"""
    tr = {}
    global MENU, PATS
    MENU = set(); PATS = []
    def menu_words(en, t):
        # メニュー（"Play Demo Quit" のように大文字で始まる語を空白で並べた文字列）: ゲームが空白で区切って
        # ボタンを作るので、訳の語数が同じなら語ごとの対応も足す。旗 4 = ボタンの並びを組み立てている間は訳さない。
        # "Appr."（略語の点）や "Quick@Delay"（@ で 2 つの語を切り替える）も語として扱う
        # UA のエディタは "Open" "Cancel" のような 1 語のボタンを sprintf（"%s %s"）で並べてからボタンにする。
        # そこで訳すと頭の英字がなくなり、ボタンが消える。大文字で始まる 1 語は、組み立て中は訳さない（旗 4）
        if len(en.strip()) <= 12 and en.strip().isalpha() and en.strip()[:1].isupper():
            MENU.add(en); MENU.add(en.strip())
        en_w = en.split()
        def ok(w):
            return all(x[:1].isupper() and x.rstrip('.').isalpha() for x in w.split('@'))
        if len(en_w) >= 2 and all(ok(w) for w in en_w):
            ja_w = t.replace('　', ' ').split()
            MENU.add(en)
            for w in en_w:
                MENU.add(w); MENU.update(w.split('@'))
            if len(ja_w) == len(en_w):
                for a, b in zip(en_w, ja_w):
                    ea, jb = a.split('@'), b.split('@')
                    if len(ea) == len(jb):
                        for x, y in zip(ea, jb): tr.setdefault(x, y)
                    else:
                        tr.setdefault(a, b)
            else:
                pass
    p = os.path.join(ROOT, 'text', 'reuse_dqk.json')
    if os.path.exists(p):
        for en, t in json.load(open(p, encoding='utf-8')).items():
            if t and t != '=':
                tr[en] = t; menu_words(en, t)
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.json'))):
        en = json.load(open(f.replace('.ja.json', '.en.json'), encoding='utf-8'))
        ja = json.load(open(f, encoding='utf-8'))
        for it in en['items']:
            t = ja.get(it['id'])
            if not t or t == '=':
                continue
            e = it['en']
            tr[e] = t
            if it['src'].startswith('EXE'):
                menu_words(e, t)
            # イベントの選択肢（"HELP THEM" など）はゲームがボタンにする。"Help Them" の形でも出るかもしれないので足す
            if it['src'].startswith('BTN') or (it['src'].startswith('DSN') and len(e) <= 30 and e.isupper() and len(e.split()) <= 4 and '.' not in e and '"' not in e):
                en_w = e.split()
                forms = {e, e.strip(), ' '.join(w.capitalize() for w in en_w), e.strip().capitalize()}
                for v in forms:
                    tr.setdefault(v, t)
                if it['src'].startswith('BTN'):
                    MENU.update(forms)
            # ^（仲間の名前に置き換わる）を 1 つ含む文: 名前が入った形でも照合できるように、前後の部分を控える
            if it['src'].startswith('DSN') and e.count('^') == 1 and t.count('^') == 1 and not e.startswith('^'):
                PATS.append((e, t))
    for name in ('names.json', 'monsters.json'):          # 名前の表（怪物・壁・呪文の効果・モジュールなど）
        p = os.path.join(ROOT, 'text', name)
        if os.path.exists(p):
            for k, v in json.load(open(p, encoding='utf-8')).items():
                tr[k] = v
                for form in (k.rstrip(), k.title(), k.rstrip().title(), k.lower(), k.rstrip().lower(), k.rstrip().capitalize()):
                    tr.setdefault(form, v)
    us = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    gl = {}
    for name in ('glossary_base.json', 'glossary.json'):
        p = os.path.join(ROOT, 'text', name)
        if os.path.exists(p):
            gl.update({k.lower(): v for k, v in json.load(open(p, encoding='utf-8')).items()})
    for src, s in us:                                      # 名前の表のうち、用語集に訳があるもの
        if src.startswith('NAM') and s not in tr and s.strip().lower() in gl and len(gl[s.strip().lower()]) <= len(s):
            for form in (s, s.rstrip(), s.title(), s.rstrip().title()):
                tr.setdefault(form, gl[s.strip().lower()])
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p):
        fx = json.load(open(p, encoding='utf-8'))
        tr.update(fx)
        for en, t in fx.items():          # メニューの並びを直したら、語ごとの対応も直す（直しが勝つ）
            en_w, ja_w = en.split(), t.replace('　', ' ').split()
            if t != '=' and len(en_w) >= 3 and len(en_w) == len(ja_w) and all(x[:1].isupper() and x.rstrip('.').isalpha() for w in en_w for x in w.split('@')):
                MENU.add(en)
                for x, y in zip(en_w, ja_w):
                    ex, jy = x.split('@'), y.split('@')
                    for u, v in (zip(ex, jy) if len(ex) == len(jy) else [(x, y)]):
                        tr[u] = v; MENU.add(u)
    return {k: v for k, v in tr.items() if v != '='}

def main():
    codec = Codec()
    gl = load_bdf(BDF)
    sects = []
    table = load_translations()
    if os.environ.get('UA_EMPTY') == '1':      # テスト用: 訳なし（英語のまま。フックとキー入力は動く）
        table = {}; MENU.clear()
    tb = struct.pack('<I', len(table))
    for en, ja in table.items():
        e = en.encode('latin1')
        jb = codec.encode(ja) if ja != '-' else b''
        tb += struct.pack('<BBHHQ', 0, 4 if en in MENU else 0, len(jb), len(e), fnv64(e)) + jb   # 旗 4: メニューのボタンの語
    sects.append(sect('T', tb))
    # 'N' 節: ^（仲間の名前）を含む文。u32 個数, 項目 {u16 前の長さ, u16 後ろの長さ, u64 前のハッシュ, u64 後ろのハッシュ, u16 訳の長さ, 訳（^ が名前の位置）}
    nb = struct.pack('<I', len(PATS))
    for en, ja in PATS:
        e = en.rstrip(' ').encode('latin1')
        pre, suf = e.split(b'^')
        jb = codec.encode(ja)
        nb += struct.pack('<HHQQH', len(pre), len(suf), fnv64(pre), fnv64(suf), len(jb)) + jb
    sects.append(sect('N', nb))
    long = [(en, ja) for en, ja in table.items() if len(ja) > len(en) + 0 and len(en) < 60 and '%' not in en]
    if long:
        print('訳が英語より長い（その場に書けず英語のまま出る）:', len(long))
        for en, ja in long[:40]: print('   %r -> %s' % (en, ja))
    # 字形は JIS X 0208 の全部（デザインの文章にはどの字も入りうる）。DLL の中の符号 = (0x80 + 区, 0x80 + 点)
    codes, bits, flags = [], b'', b''
    for ku in range(1, 85):
        for ten in range(1, 95):
            ch = uajp.from_kuten(ku, ten)
            g = gl.get(ord(ch)) if ch else None
            if g is None:
                continue
            codes.append(((0x80 + ku) << 8) | (0x80 + ten)); bits += g
            flags += bytes([(1 if ch in NO_START else 0) | (2 if ch in NO_END else 0)])
    sects.append(sect('F', struct.pack('<I', len(codes)) + struct.pack('<%dH' % len(codes), *codes) + bits + flags))
    sects.append(sect('P', struct.pack('<2I', CB_BASE, LOG)))
    blob = b'DQJ1' + struct.pack('<II', 1, len(sects)) + b''.join(sects)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin', len(blob), 'bytes,', len(table), 'entries,', len(codes), 'glyphs')

if __name__ == '__main__':
    main()
