# 訳を DLL のリソース（build/patch.bin）にまとめる
#   text/units.json（原文）+ text/ja/*.json（訳: 文章は id -> 段落のリスト、EXE・問いは id -> 文字列）+ text/fixes.json（手直し）
#
# 日本語の符号（ゲームの中を通る 1 バイトの並び）:
#   1 バイトの字 0x80-0xFE … EXE の文字列用（よく使う 127 字）
#   2 バイトの字 lead + trail … lead 0x01-0x1F（0x05 0x0A 0x0D 0x1A を除く）、trail 0x21-0x7F（* @ ~ を除く）
#   文章ファイル（DF.TXT / GAME.TXT）はゲームが 0x80 以上を 2 文字の辞書で展開するので、辞書の表（MAIN.EXE 0x200/0x210）を
#   日本語用に替え、よく出る 128 字は 1 バイト（展開されて lead + trail になる）、ほかは 2 バイトで書く。
import os, json, re, struct, sys
from collections import Counter
sys.path.insert(0, os.path.dirname(__file__))
import wwtext
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'DS')

LEADS = [x for x in range(1, 0x20) if x not in (0x05, 0x0a, 0x0d, 0x1a)]
TRAILS = [x for x in range(0x21, 0x80) if x not in (0x2a, 0x40, 0x7e)]
CB_BASE = 0x70
FORCE_SINGLE = '番目'


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


def fnv64(b):
    h = 14695981039346656037
    for c in b: h = ((h ^ c) * 1099511628211) & 0xffffffffffffffff
    return h


def load():
    units = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    ja = {}
    d = os.path.join(ROOT, 'text', 'ja')
    if os.path.isdir(d):
        for f in sorted(os.listdir(d)):
            if f.endswith('.json'): ja.update(json.load(open(os.path.join(d, f), encoding='utf-8')))
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p):
        for k, v in json.load(open(p, encoding='utf-8')).items():
            ja[k] = v
    # 固有名の表記をそろえる
    def norm(t):
        for a, b in NAMES: t = t.replace(a, b)
        return t
    for k, v in list(ja.items()):
        ja[k] = [norm(x) for x in v] if isinstance(v, list) else norm(v)
    return units, ja


NAMES = [('グンサー・ウス・ウィスタン', 'ガンサー・ウス゠ウィスタン'), ('グンサー', 'ガンサー'), ('カエルゴス', 'ケアゴス'), ('カーゴス', 'ケアゴス'),
         ('クロガルグ', 'クロガーグ'), ('ハイ・クレリスト峠', '大司教の塔の峠'), ('ハイ・クレリスト', '大司教'), ('＝', '゠')]


def is_jp(c): return ord(c) >= 0x80


# ---------------------------------------------------------------- 折り返し
NOSTART = set('、。，．・ー〜！？」』）ぁぃぅぇぉっゃゅょァィゥェォッャュョ…')
NOEND = set('「『（')


def wrap_jp(text, width, indent, first_indent):
    """1 字 1 桁。行のリスト（字下げの空白を含む）"""
    lines = []
    cur = ' ' * first_indent
    w = width
    for ch in text:
        if len(cur) >= w:
            # 行頭禁則: 次の字が行頭に来てはいけないなら前の行に押し込む（1 字まで）
            if ch in NOSTART and len(cur) <= w:
                cur += ch; continue
            if cur and cur[-1] in NOEND:
                last = cur[-1]; lines.append(cur[:-1]); cur = ' ' * indent + last
            else:
                lines.append(cur); cur = ' ' * indent
        cur += ch
    if cur.strip(): lines.append(cur)
    return lines


# ---------------------------------------------------------------- 符号
class Codes:
    def __init__(self, exe_text, file_text):
        cnt_exe = Counter(c for c in exe_text if is_jp(c))
        cnt_file = Counter(c for c in file_text if is_jp(c))
        self.single = {}          # 字 -> 0x80..0xFE
        for c in FORCE_SINGLE: cnt_exe[c] += 10000          # コピープロテクトの序数（「11番目」を 4 バイトに）
        for i, (c, _) in enumerate(cnt_exe.most_common(127)): self.single[c] = 0x80 + i
        self.dict = {}            # 字 -> (辞書の番号 0..127, lead, trail)
        self.two = {}             # 字 -> (lead, trail)
        used = set()
        self.t1 = LEADS[:16]
        self.t2 = [0x21] * 128
        for i, (c, _) in enumerate(cnt_file.most_common(128)):
            g, k = divmod(i, 8)
            lead, trail = self.t1[g], TRAILS[k]
            self.dict[c] = (i, lead, trail)
            self.two[c] = (lead, trail)
            self.t2[i] = trail
            used.add((lead, trail))
        allc = sorted(set(cnt_exe) | set(cnt_file))
        it = ((l, t) for l in LEADS for t in TRAILS if (l, t) not in used)
        for c in allc:
            if c not in self.two:
                self.two[c] = next(it)

    def enc_exe(self, s):
        out = bytearray()
        for c in s:
            if not is_jp(c): out.append(ord(c))
            elif c in self.single: out.append(self.single[c])
            else: out += bytes(self.two[c])
        return bytes(out)

    def enc_file(self, s):
        out = bytearray()
        for c in s:
            if not is_jp(c):
                if c in '*~': raise SystemExit('reserved char in text: %r' % s)
                out.append(ord(c))
            elif c in self.dict: out.append(0x80 + self.dict[c][0])
            else: out += bytes(self.two[c])
        return bytes(out)

    def glyph_table(self, font):
        codes = []
        for c, b in self.single.items(): codes.append((b, c))
        for c, (l, t) in self.two.items(): codes.append(((l << 8) | t, c))
        miss = sorted({c for _, c in codes if ord(c) not in font})
        if miss: print('no glyph:', ''.join(miss))
        codes.sort()
        out = struct.pack('<I', len(codes)) + b''.join(struct.pack('<H', k) for k, _ in codes)
        out += b''.join(font.get(ord(c), bytes(8)) for _, c in codes)
        return out


# ---------------------------------------------------------------- 組み立て
def cells(s): return len(s)


def fit_exe(u, t, codes):
    """EXE の文字列を元と同じバイト数にする。入らなければ None
       - キー一覧（"Fly down & left     Keypad 7"）: 訳の後ろを空白で埋め、キーの名前を元と同じ桁から
       - 前後に空白のある欄（メッセージ欄・メニュー）: 桁で中央に寄せる（入らなければ左に寄せる）
       - 後ろだけ空白の欄: 後ろを空白で埋める
       - それ以外（ゼロ終わり）: 後ろを 0 で埋める"""
    en, L = u['en'], u['len']
    b = codes.enc_exe(t)
    w = len(t)                               # 桁数（1 字 1 桁）
    m = re.match(r'(.*?\S)(\s{2,})(\S.*)$', en)
    if m and 0xa866 <= u['off'] <= 0xabe9:
        key = m.group(3).encode()
        col = len(en) - len(m.group(3))
        sp = max(1, col - w)
        while sp > 1 and len(b) + sp + len(key) > L: sp -= 1
        out = b + b' ' * sp + key
        return out + b' ' * (L - len(out)) if len(out) <= L else None
    if len(b) > L: return None
    lead = len(en) - len(en.lstrip(' '))
    trail = len(en) - len(en.rstrip(' '))
    if lead:
        left = max(0, (L - w) // 2)
        while left and left + len(b) > L: left -= 1
        out = b' ' * left + b
        return out + b' ' * (L - len(out))
    if trail or u['term'] != 0:
        return b + b' ' * (L - len(b))
    return b + bytes(L - len(b))


def build_text_file(fname, units, ja, codes):
    """文章ファイルを作る（訳のない項目は英語のまま）"""
    t = wwtext.decode(open(os.path.join(GAME, fname), 'rb').read())
    ents = t.split('~')
    byid = {u['entry']: u for u in units}
    out = []
    for i, e in enumerate(ents):
        u = byid.get(i)
        tr = ja.get(u['id']) if u else None
        if not u or not tr:
            out.append(e.encode('latin1'))
            continue
        if isinstance(tr, str): tr = [p for p in tr.split('\n') if p.strip()]
        width = max(u['width'], 36)
        if width > 39: width = 39
        lines = []
        for k, para in enumerate(tr):
            # 段落の 1 行目は 1 字下げ（英語で字下げのある項目だけ）
            pi = u['indent'] + 1 if u['first_indent'] > u['indent'] else u['indent']
            lines += wrap_jp(para.strip(), width, u['indent'], pi)
        body = '\r\n'.join(l + '@' for l in lines)
        head = u['head'] + '\r\n' if u['head'] else ''
        if not e.startswith(u['head']): head = ''
        lead_nl = '\r\n' if e.startswith('\r\n') and not u['head'] else ''
        s = lead_nl + head + body + '\r\n' + (u['tail'] + '\r\n' if u['tail'] else '')
        out.append(codes.enc_file(s))
    data = b'~'.join(out)
    return data


def main():
    units, ja = load()
    font = bdf(os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf'))
    font[0x30a0] = bytes([0, 0, 0, 0x3c, 0, 0x3c, 0, 0])      # ゠（美咲の＝はニに見えるので）
    # 1 バイトの字はメッセージ欄（1 バイト 1 桁で描く欄）の字を優先する
    bar = ''.join(ja.get(u['id'], '') for u in units['exe'] if 0x7853 <= u['off'] < 0x7dd3)
    exe_text = ''.join(ja.get(u['id'], '') for u in units['exe'] + units['copy']) + ja.get('_answer', '（答え：）')
    file_text = ''.join(''.join(v) if isinstance(v, list) else v for k, v in ja.items() if k[:2] in ('df', 'gm'))
    codes = Codes(exe_text, file_text)
    sections = []
    sections.append((b'G', codes.glyph_table(font)))
    sections.append((b'P', struct.pack('<I', CB_BASE)))
    pre, post = ja.get('_answer_pre', '（答え：'), ja.get('_answer_post', '）')
    sections.append((b'A', codes.enc_exe(pre) + b'\0' + codes.enc_exe(post) + b'\0'))
    # ---- 'D' 節
    files = []
    main_exe = open(os.path.join(GAME, 'MAIN.EXE'), 'rb').read()
    ent = []
    def put(off, new, olen):
        old = main_exe[off:off + olen]
        if len(new) > olen: raise SystemExit('too long at %x: %d > %d' % (off, len(new), olen))
        ent.append(struct.pack('<IHHQ', off, olen, len(new), fnv64(old)) + new)
    # 辞書の表
    put(0x200, bytes(codes.t1), 16)
    put(0x210, bytes(codes.t2), 128)
    over = []
    for u in units['exe']:
        t = ja.get(u['id'])
        if t is None: continue
        b = fit_exe(u, t, codes)
        if b is None: over.append((u['id'], u['en'], t, len(codes.enc_exe(t)), u['len'])); continue
        put(u['off'], b, u['len'])
    same = {}
    for u in units['copy']:
        if u['id'] in ja: same.setdefault(u['en'], ja[u['id']])
    for u in units['copy']:
        t = ja.get(u['id'], same.get(u['en']))
        if t is None: continue
        b = codes.enc_exe(t.replace('\\n', '\n'))
        if len(b) > u['len']: over.append((u['id'], u['en'], t, len(b), u['len'])); continue
        enc = bytes((5 - c) & 0xff for c in b)
        put(u['off'], enc + bytes(u['len'] - len(enc)), u['len'])       # 余りは 0（そこで文が終わる）
    if over:
        for o in over: print('TOO LONG', o)
    files.append(('MAIN.EXE', 1, main_exe, struct.pack('<I', len(ent)) + b''.join(ent)))
    for fname, key in (('DF.TXT', 'df'), ('GAME.TXT', 'game')):
        data = build_text_file(fname, units[key], ja, codes)
        orig = open(os.path.join(GAME, fname), 'rb').read()
        print(fname, len(orig), '->', len(data))
        if len(data) > 0x8000: raise SystemExit(fname + ' over 32KB')
        files.append((fname, 2, orig, data))
    d = struct.pack('<I', len(files))
    for name, typ, orig, payload in files:
        d += name.encode().ljust(16, bytes(1)) + struct.pack('<B3xIQI', typ, len(orig), fnv64(orig), len(payload)) + payload
    sections.append((b'D', d))
    blob = b'DSJ1' + struct.pack('<I', len(sections))
    for t, s in sections:
        blob += struct.pack('<I', ord(t)) + struct.pack('<I', len(s)) + s
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin', len(blob), 'bytes;', len(codes.single), 'single', len(codes.dict), 'dict', len(codes.two), 'two-byte')


if __name__ == '__main__':
    main()
