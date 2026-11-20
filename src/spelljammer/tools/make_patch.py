# 訳と字形から build/patch.bin（DLL に入れるパッチデータ）を作る
import os, sys, struct, json, glob, re
sys.path.insert(0, os.path.dirname(__file__))
import sjdata as S
import extract as X

ROOT = S.ROOT
CB_BASE = 0x70
TRAILS = list(range(0xc0, 0xff))
NO_START = set('、。，．・：；？！ー…‥」』）］｝〕〉》】ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々〜～')
NO_END = set('「『（［｛〔〈《【')
PUNCT = set('、。！？」』）')
FULL_KEEP = '！？（）：、。・ー'


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


class Codec:
    """日本語の字に 2 バイトの独自符号（1 バイト目 0x80-0xbf, 2 バイト目 0xc0-0xfe）を割り当てる"""
    def __init__(self):
        self.map = {}
    def code(self, ch):
        if ch not in self.map:
            n = len(self.map)
            assert n < 64 * len(TRAILS), 'too many glyphs'
            self.map[ch] = bytes([0x80 + n // len(TRAILS), TRAILS[n % len(TRAILS)]])
        return self.map[ch]
    @staticmethod
    def norm(s):
        out = ''
        for ch in s:
            o = ord(ch)
            if 0xff01 <= o <= 0xff5e and ch not in FULL_KEEP: ch = chr(o - 0xfee0)
            elif ch == '　': ch = ' '
            elif ch == '〜': ch = '～'
            out += ch
        return out
    def encode(self, s):
        out = bytearray()
        for ch in self.norm(s):
            if ord(ch) < 0x80: out.append(ord(ch))
            else: out += self.code(ch)
        return bytes(out)


def blen(s):
    return sum(1 if ord(c) < 0x80 else 2 for c in Codec.norm(s))


def add_breaks(s, budget=None, punct_only=False):
    """日本語の字の間に空白（折り返しの区切り。日本語のあとの空白は幅 0 で描く）を入れる。
    budget（バイト数の上限）があれば、入る分だけ（句読点のあとを先に、残りはなるべく均等に）"""
    s = Codec.norm(s)
    cand = []
    for i in range(1, len(s)):
        a, b = s[i - 1], s[i]
        if ord(a) < 0x80 or ord(b) < 0x80: continue
        if b in NO_START or a in NO_END: continue
        if punct_only and a not in PUNCT: continue
        cand.append(i)
    if not cand: return s
    room = None if budget is None else budget - blen(s)
    if room is not None and room <= 0: return s
    if room is None or room >= len(cand):
        pick = set(cand)
    else:
        first = [i for i in cand if s[i - 1] in PUNCT]
        pick = set(first[:room])
        rest = [i for i in cand if i not in pick]
        k = room - len(pick)
        if k > 0 and rest:
            step = len(rest) / k
            pick |= {rest[int(j * step)] for j in range(k)}
    out = ''
    for i, ch in enumerate(s):
        if i in pick: out += ' '
        out += ch
    return out


def btn_label(t):
    """「^X」の近道キーの字のあとに英字が続くときは空白を入れ、日本語が続くときは空白を詰める"""
    t = Codec.norm(t)
    if t.startswith('^') and len(t) > 2:
        rest = t[2:].lstrip()
        if t[2:3] == '.': return t                      # ^1. 秩序・善
        t = t[:2] + (' ' if rest[:1].isascii() and rest[:1].isalnum() else '') + rest
    return t


def fnv64(b):
    h = 14695981039346656037
    for c in b: h = ((h ^ c) * 1099511628211) & 0xffffffffffffffff
    return h


def sect(t, d):
    return struct.pack('<II', ord(t), len(d)) + d


def load_translations():
    """{英語: 日本語（'' = 何も出さない）}。'=' のものは入れない"""
    tr = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.json'))):
        ja = json.load(open(f, encoding='utf-8'))
        en = {it['id']: it['en'] for it in json.load(open(f.replace('.ja.json', '.en.json'), encoding='utf-8'))['items']}
        for i, t in ja.items():
            if i in en and t != '=': tr[en[i]] = '' if t == '-' else t
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p):
        for k, v in json.load(open(p, encoding='utf-8')).items():
            if v == '=': tr.pop(k, None)
            else: tr[k] = '' if v == '-' else v
    return tr


def dfile(name, typ, orig, payload, check_hash=True):
    return (name.encode().ljust(16, b'\x00') + struct.pack('<BBBB', typ, 0, 0, 0) +
            struct.pack('<IQI', len(orig), fnv64(orig) if check_hash else 0, len(payload)) + payload)


def inplace(entries):
    """entries: [(位置, 元の bytes, 新しい bytes)]"""
    out = struct.pack('<I', len(entries))
    for off, o, n in entries:
        assert len(n) <= len(o)
        out += struct.pack('<IHHQ', off, len(o), len(n), fnv64(o)) + n
    return out


def fit(codec, en, ja, mx, breaks=True):
    """訳を mx バイト以内の bytes にする。入らなければ None"""
    if blen(ja) > mx: return None
    t = add_breaks(ja, mx, punct_only=True) if breaks and blen(ja) > 24 else Codec.norm(ja)
    if en.endswith(' ') and not t.endswith(' '):
        t = t + ' ' * (mx - blen(t)) if en.rstrip().endswith(':') or len(en) - len(en.rstrip()) > 1 else t + ' '
        if blen(t) > mx: t = t.rstrip() + ' ' * min(1, mx - blen(t.rstrip()))
    return codec.encode(t)


def string_refs(d):
    """DGROUP の中の番地への参照: {番地: [その番地が書かれているファイル上の位置]}
    （push ds / push 番地、mov dx,ds / mov ax,番地、mov [bp+x],番地 / mov [bp+x+2],ds、mov [a],番地 / mov [a+2],ds、
    DGROUP の中のファーポインタ（番地, DGROUP の段））"""
    ds, end = S.mz_dgroup(d)
    hdr = struct.unpack_from('<H', d, 8)[0] * 16
    ip, cs = struct.unpack_from('<HH', d, 0x14)
    dseg = struct.unpack_from('<H', d, hdr + cs * 16 + ip + 1)[0]
    refs = {}
    pats = [(rb'\x1e\x68(..)', 2), (rb'\x8c\xda\xb8(..)', 3), (rb'\xc7\x46.(..)\x8c\x5e', 3), (rb'\xc7\x06..(..)\x8c\x1e', 4)]
    for p, k in pats:
        for m in re.finditer(p, d, re.S):
            refs.setdefault(struct.unpack('<H', m.group(1))[0], []).append(m.start() + k)
    for i in range(ds, end - 3):
        o, sg = struct.unpack_from('<HH', d, i)
        if sg == dseg: refs.setdefault(o, []).append(i)
    return refs


def exe_entries(name, d, units, tr, codec, stats):
    """EXE の文字列の書き換え。ほかの所から文字列の途中（後ろの部分）を指しているもの（英語版は "FEMALE" の後ろ 4 字を
    "MALE" として、"CLERIC" の最後の字を属性の略号 "C" として使っている）は、その部分の訳を空いた所に置き、
    指している所を書き換える"""
    ds, _ = S.mz_dgroup(d)
    refs = string_refs(d)
    plan = {}
    for u in units:
        if u['en'] not in tr: continue
        for w in u['where']:
            if not w.startswith(name + ':'): continue
            off = int(w.split(':')[1], 16)
            o = d[ds + off:ds + off + len(u['en'])]
            assert o == u['en'].encode('latin1'), (w, o)
            jb = fit(codec, u['en'], tr[u['en']], len(o))
            if jb is None: stats['long'] += 1; print('長すぎる', w, u['en'], tr[u['en']]); continue
            plan[off] = [o, jb]
    inner = {off: [k for k in range(1, len(o)) if off + k in refs] for off, (o, jb) in plan.items()}
    slots = {off: bytearray(jb) + bytes(len(o) - len(jb)) for off, (o, jb) in plan.items()}
    free = []        # [DGROUP 内の位置, 長さ]
    for off, (o, jb) in plan.items():
        if inner[off]: continue
        p = len(jb) + 1
        q = len(o)
        if q - p >= 2 and not any(off + x in refs for x in range(p, q + 1)):
            free.append([off + p, q - p])
    extra = []
    for off, ks in sorted(inner.items()):
        if not ks: continue
        o, jb = plan[off]
        moves = []
        for k in ks:
            t = o[k:].decode('latin1')
            jt = None
            if t in tr and blen(tr[t]) <= len(t): jt = codec.encode(Codec.norm(tr[t]))
            if jt is None and k == 4 and o.startswith(b'THE '): jt = jb       # "THE %s IS KILLED." の "%s IS KILLED." = 同じ訳
            if jt is None:
                jt = t.encode('latin1')
                if re.search(r'[A-Za-z]{2}', t): print('後ろの部分は英語のまま', name, repr(o), '->', repr(t))
            if jt == jb:                                                        # 訳が同じなら文字列の頭を指せばよい
                moves.append((k, off, None)); continue
            if len(jt) < len(jb) and jb.endswith(jt) and (len(jb) - len(jt)) % 1 == 0:   # 訳の後ろの部分と同じ
                moves.append((k, off + len(jb) - len(jt), None)); continue
            need = len(jt) + 1
            spot = next((f for f in free if f[1] >= need), None)
            if spot is None: moves = None; break
            moves.append((k, spot[0], jt))
            spot[0] += need; spot[1] -= need
        if moves is None:
            print('後ろの部分を置く所がない（英語のまま）', name, hex(off), o)
            del slots[off]
            continue
        for k, at, jt in moves:
            if jt is not None:
                host = max(h for h in slots if h <= at)
                pos = at - host
                slots[host][pos:pos + len(jt) + 1] = jt + b'\x00'
            for r in refs[off + k]:
                extra.append((r, d[r:r + 2], struct.pack('<H', at)))
            stats.setdefault('moved', 0); stats['moved'] += 1
    ents = [(ds + off, plan[off][0], bytes(s)) for off, s in slots.items()] + extra
    stats['ok'] += len(slots)
    return ents


def main():
    codec = Codec()
    g8 = bdf8(os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf'))
    tr = load_translations()
    units = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
    dfiles = []
    stats = {'ok': 0, 'long': 0}
    # ---- EXE の文字列
    for name in X.EXES:
        d = S.read(os.path.join(S.GAME, name))
        dfiles.append(dfile(name, 1, d, inplace(exe_entries(name, d, units, tr, codec, stats)), check_hash=False))
    # ---- 名前（固定欄）
    byfile = {}
    for fn, p, s in X.name_fields():
        if s in tr:
            jb = fit(codec, s, tr[s], len(s), breaks=False)
            if jb is None: print('長すぎる', fn, s, tr[s]); continue
            byfile.setdefault(fn, []).append((p, s.encode('latin1'), jb))
    for fn, ents in byfile.items():
        d = S.read(os.path.join(S.ART, fn))
        dfiles.append(dfile(fn, 1, d, inplace(ents)))
    # ---- 呪文の一覧（行ごと）
    for fn in ['MUSPELLS.DAT', 'CLSPELLS.DAT']:
        d = S.read(os.path.join(S.ART, fn))
        lines = d.decode('latin1').split('\r\n')
        nl = [codec.encode(tr.get(l, l)) if l.strip() else l.encode('latin1') for l in lines]
        dfiles.append(dfile(fn, 2, d, b'\r\n'.join(nl)))
    # ---- 任務の説明
    for f in sorted(glob.glob(os.path.join(S.ART, 'MISS*.TXT'))):
        d = S.read(f)
        en = S.miss_text(d).decode('latin1')
        if en not in tr: continue
        dfiles.append(dfile(os.path.basename(f), 2, d, S.miss_build(codec.encode(add_breaks(tr[en])))))
    # ---- 噂（256 バイトのレコード。XOR）
    for f in sorted(glob.glob(os.path.join(S.ART, '*.RUM'))):
        d = S.read(f)
        ents = []
        for i, r in enumerate(S.rum_records(d)):
            en = r.rstrip(b' \x00').decode('latin1')
            if en not in tr: continue
            jb = codec.encode(add_breaks(tr[en], 255))
            if len(jb) > 255: print('噂が長すぎる', os.path.basename(f), i); continue
            ents.append((i * 256, r, jb))
        dfiles.append(dfile(os.path.basename(f), 4, d, inplace(ents)))
    # ---- 物語（STORY.LIB）
    d = S.read(os.path.join(S.GAME, 'STORY.LIB'))
    ents = []
    for e in S.story_entries(d):
        es = e.decode('latin1')
        if '@' in es:
            head, body = es[:es.index('@') + 1], es[es.index('@') + 1:]
            key = re.sub(r' {2,}', ' ', body.replace('\n', ' ').strip())
            if key in tr:
                e = head.encode('latin1') + codec.encode(add_breaks(tr[key])) + b'\n'
        ents.append(e)
    dfiles.append(dfile('STORY.LIB', 2, d, S.story_build(ents)))
    # ---- ボタン
    btn = {}
    for u in units:
        if u['kind'] == 'btn' and u['en'] in tr and any(w.startswith('BTN:') for w in u['where']):
            btn[u['en']] = codec.encode(btn_label(tr[u['en']]))
    for k, v in tr.items():                       # ファイルごとの訳（fixes.json の "PLAY.BTN|E^XIT"）
        if '|' in k and k.split('|')[0].upper().endswith('.BTN'):
            btn[k.split('|')[0].upper() + '|' + k.split('|', 1)[1]] = codec.encode(btn_label(v))
    bt = struct.pack('<I', len(btn))
    for en, jb in btn.items():
        e = en.encode('latin1')
        bt += struct.pack('<QHH', fnv64(e), len(e), len(jb)) + jb
    csect = sect('C', codec.encode('（答え：') + b'\x00' + codec.encode('）') + b'\x00')
    # ---- 字形
    sects = [sect('D', struct.pack('<I', len(dfiles)) + b''.join(dfiles)), sect('B', bt), csect]
    codes, b8 = [], b''
    for ch, cd in sorted(codec.map.items(), key=lambda kv: kv[1]):
        a = g8.get(ord(ch))
        if a is None: print('8x8 の字形がない', ch, hex(ord(ch))); a = bytes(8)
        codes.append(struct.unpack('>H', cd)[0]); b8 += a
    sects.append(sect('F', struct.pack('<I', len(codes)) + struct.pack('<%dH' % len(codes), *codes) + b8))
    sects.append(sect('P', struct.pack('<I', CB_BASE)))
    blob = b'SJJ1' + struct.pack('<II', 1, len(sects)) + b''.join(sects)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin', len(blob), 'bytes,', len(tr), 'translations,', stats, len(btn), 'buttons,', len(codes), 'glyphs')


if __name__ == '__main__':
    main()
