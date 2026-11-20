# Hillsfar: 文字列の復号と抽出
import struct, os
HERE = os.path.dirname(os.path.abspath(__file__))
MEM = os.path.join(HERE, '..', 'test', 'mem_menu.bin')
GAME = os.path.join(HERE, '..', '..', 'GAME', 'HILLSFAR')
DS = 0x291c0
_m = None
def mem():
    global _m
    if _m is None: _m = open(MEM, 'rb').read()
    return _m
def tables():
    m = mem()
    return m[0x33c64:0x33c74], m[0x33c74:0x33c74 + 128]
ARG = (2, 6, 9, 0x13)
def dec(b):
    """圧縮文字列（0 終端なし）→ 文字列（制御: \x02 等 + 引数 1 バイトをそのまま）"""
    t1, t2 = tables()
    out = bytearray(); i = 0
    while i < len(b):
        c = b[i]; i += 1
        if c & 0x80:
            out.append(t1[(c & 0x7f) >> 3]); out.append(t2[c & 0x7f])
        elif c in ARG and i < len(b):
            out.append(c); out.append(b[i]); i += 1
        else:
            out.append(c)
    return bytes(out)
def cstr(d, o):
    """o から 0 終端まで（制御コードの引数の 0 は終端にしない）"""
    i = o
    while i < len(d):
        c = d[i]
        if c == 0: break
        if c in ARG: i += 2
        else: i += 1
    return d[o:i]
def is_text(s):
    if len(s) < 2: return False
    t = bytearray(); i = 0
    while i < len(s):                     # 制御記号の引数は数えない
        if s[i] in ARG: i += 2; continue
        t.append(s[i]); i += 1
    s = bytes(t)
    if len(s) < 2: return False
    ok = sum(1 for c in s if 32 <= c < 127 or c in (13, 0x10))
    letters = sum(1 for c in s if chr(c).isalpha())
    return ok == len(s) and letters >= max(2, len(s) * 0.4)
def ds_strings():
    m = mem(); seg = m[DS:DS + 0x10000]
    res = []; i = 0
    while i < len(seg):
        if seg[i] == 0: i += 1; continue
        r = cstr(seg, i)
        d = dec(r)
        if is_text(d): res.append((i, d))
        else:                              # 前に表のデータがくっついた文字列: 後ろの部分から文字列らしい所を探す
            for k in range(1, len(r) - 3):
                if 65 <= r[k] <= 90 and (97 <= r[k + 1] <= 122 or r[k + 1] == 32) and not (97 <= r[k - 1] <= 122 or r[k - 1] in (32, 39, 44, 46) or r[k - 1] >= 0x80):
                    d2 = dec(r[k:])
                    if is_text(d2) and b' ' in d2: res.append((i + k, d2)); break
        i += len(r) + 1
    return res
def q_strings(name):
    d = open(os.path.join(GAME, name), 'rb').read()
    t = struct.unpack('<H', d[:2])[0]
    first = struct.unpack('<H', d[t:t + 2])[0]
    res = []
    for k in range((first - t) // 2):
        o = struct.unpack('<H', d[t + 2 * k:t + 2 * k + 2])[0]
        res.append((o, dec(cstr(d, o))))
    return res

def segments(s):
    """復号済み文字列 → 翻訳単位（改行・制御記号で区切り、前後の空白を除いたもの）"""
    out = []; i = 0; n = len(s)
    while i < n:
        c = s[i]
        if c in (0x0d, 0x10): i += 1; continue
        if c in ARG: i += 2; continue
        j = i
        while j < n and s[j] not in (0x0d, 0x10) and s[j] not in ARG: j += 1
        seg = s[i:j].strip(b' ')
        if seg: out.append(seg)
        i = j
    return out

def all_units():
    """[(出どころ, 断片)]（出現順、重複なし）"""
    import glob
    seen = set(); res = []
    def add(src, d):
        for seg in segments(d):
            if seg not in seen and any(chr(c).isalpha() for c in seg):
                seen.add(seg); res.append((src, seg))
    for off, d in ds_strings():
        add('ds:%04x' % off, d)
    for f in sorted(glob.glob(os.path.join(GAME, 'Q*.BIN'))):
        b = os.path.basename(f)
        for off, d in q_strings(b):
            add('%s:%04x' % (b, off), d)
    return res

import re as _re
_OKCH = _re.compile(rb"^[A-Za-z0-9 ,.'!?\-:;\"()/&%$#*+=<>]+$")
_EXT = _re.compile(rb'\.(CMP|ANM|MAP|BIN|OUT|PRE|EXE)$', _re.I)
def is_unit(s):
    if not _OKCH.match(s) or _EXT.search(s): return False
    if b' ' in s:
        w = s.split()
        return sum(1 for x in w if _re.match(rb"^[A-Za-z'][a-z']*[.,!?:;]*$", x)) >= max(1, len(w) // 2)
    return bool(_re.match(rb"^([A-Z][a-z'-]{1,}|[A-Z][A-Z/-]+[A-Z]|[A-Z][a-z]+-[A-Z]?[a-z]+)[.!?:]*$", s))
