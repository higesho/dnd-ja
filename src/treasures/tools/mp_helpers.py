import re, struct, zlib
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

