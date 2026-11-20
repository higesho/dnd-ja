# 原文 <-> 翻訳者向け表記 の変換
#   \r -> 改行, \x01 -> {P}, \x12 -> {W}, \x06 X -> {C:XX}, \x02 X -> {F:XX}, \n(0x0a) -> {LF}
# 文頭・文末の空白/改行/制御は「外枠」として保存し、訳者には本文だけを渡す
import re

EDGE = re.compile(r'^((?:[ \r\x01\x12]|[\x06\x02].)*)(.*?)((?:[ \r\x01\x12]|[\x06\x02].)*)$', re.S)

def split_edges(s):
    m = EDGE.match(s)
    return m.group(1), m.group(2), m.group(3)

def to_tok(core):
    out = []
    i = 0
    while i < len(core):
        c = core[i]
        if c in '\x06\x02' and i + 1 < len(core):
            out.append('{%s:%02X}' % ('C' if c == '\x06' else 'F', ord(core[i + 1])))
            i += 2
            continue
        out.append({'\r': '\n', '\x01': '{P}', '\x12': '{W}', '\n': '{LF}'}.get(c, c))
        i += 1
    return ''.join(out)

TOK = re.compile(r'\{([CF]):([0-9A-Fa-f]{2})\}|\{P\}|\{W\}|\{LF\}|\n')

def from_tok(t):
    def rep(m):
        g = m.group(0)
        if m.group(1):
            return ('\x06' if m.group(1) == 'C' else '\x02') + chr(int(m.group(2), 16))
        return {'{P}': '\x01', '{W}': '\x12', '{LF}': '\n', '\n': '\r'}[g]
    return TOK.sub(rep, t)

def tokens_of(t):
    return sorted(m.group(0).upper() for m in TOK.finditer(t) if m.group(0) != '\n')

def specs_of(t):
    return re.findall(r'%[-+ #0-9]*l?[dsuxic]', t)
