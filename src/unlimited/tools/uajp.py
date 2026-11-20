# UA のデザインの文章に日本語を入れるための符号（保存形式）。src/uajp.h と同じ決まり。
#
# UA の文章は 6 ビット（64 種類の記号）で保存される。日本語 1 字を、英文でほとんど使われない記号で始まる 2〜3 記号で表す。
#   2 記号: ] < > = のどれか + 1 字   → 204 通り（ひらがな・カタカナ・主な約物）
#   3 記号: [ # $ のどれか + 2 字     → 漢字（JIS 第 1・第 2 水準）とそのほかの記号
# 後ろの字に使うのは 51 種類: ! " & ' ( ) * + , - . / 0-9 : ; ? A-Z
#   （先頭の記号と重ならないので、途中から読んでも字の区切りがわかる。% ^ @ \ _ 空白は使わない）
T = '!"&\'()*+,-./0123456789:;?' + 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
assert len(T) == 51
LEAD2 = ']<>='
LEAD3 = '[#$'
# 2 記号の面に入れる 1 区（記号）の点番号（順番を変えないこと）
P1 = [2, 3, 6, 9, 10, 28, 33, 36, 54, 55, 56, 57, 42, 43, 4, 5, 7, 8, 25, 29, 37, 38, 39, 40, 41, 58, 59, 26, 27, 11, 12,
      19, 20, 21, 22]
assert len(P1) == 35
NK = 6486            # 漢字の数（16〜84 区）
ASCII_OK = set(' !"#$%&\'()*+,-./0123456789:;<=>?ABCDEFGHIJKLMNOPQRSTUVWXYZ')

def kuten(ch):
    """Unicode 1 字 → (区, 点)。JIS X 0208 にない字は None"""
    try:
        b = ch.encode('euc_jp')
    except UnicodeEncodeError:
        return None
    if len(b) != 2 or b[0] < 0xa1 or b[1] < 0xa1:
        return None
    return b[0] - 0xa0, b[1] - 0xa0

def from_kuten(ku, ten):
    try:
        return bytes([0xa0 + ku, 0xa0 + ten]).decode('euc_jp')
    except UnicodeDecodeError:
        return None

def plane2_index(ku, ten):
    if ku == 4 and ten <= 83: return ten - 1
    if ku == 5 and ten <= 86: return 83 + ten - 1
    if ku == 1 and ten in P1: return 169 + P1.index(ten)
    return -1

def plane2_kuten(i):
    if i < 83: return 4, i + 1
    if i < 169: return 5, i - 83 + 1
    if i < 204: return 1, P1[i - 169]
    return None

def plane3_index(ku, ten):
    if 16 <= ku <= 84: return (ku - 16) * 94 + ten - 1
    if 1 <= ku <= 8: return NK + (ku - 1) * 94 + ten - 1
    return -1

def plane3_kuten(i):
    if i < NK: return 16 + i // 94, i % 94 + 1
    i -= NK
    if i < 8 * 94: return 1 + i // 94, i % 94 + 1
    return None

def encode(s):
    """日本語まじりの文 → UA に打ち込む記号の並び。入れられない字は ? にする"""
    out = []
    for ch in s:
        o = ord(ch)
        if ch in '\r\n\t　': ch = ' '
        elif 0xff01 <= o <= 0xff5e and chr(o - 0xfee0).upper() in ASCII_OK and ch not in '！？（）：；，．':
            ch = chr(o - 0xfee0)
        if ord(ch) < 0x80:
            ch = ch.upper()
            out.append(ch if ch in ASCII_OK else '?')
            continue
        kt = kuten(ch)
        if not kt:
            out.append('?'); continue
        i = plane2_index(*kt)
        if i >= 0:
            out.append(LEAD2[i // 51] + T[i % 51]); continue
        i = plane3_index(*kt)
        if i < 0:
            out.append('?'); continue
        out.append(LEAD3[i // 2601] + T[i // 51 % 51] + T[i % 51])
    return ''.join(out)

def decode(s):
    """記号の並び → 日本語まじりの文"""
    out = []
    i = 0
    s = s.upper()
    while i < len(s):
        c = s[i]
        if c in LEAD2 and i + 1 < len(s) and s[i + 1] in T:
            kt = plane2_kuten(LEAD2.index(c) * 51 + T.index(s[i + 1]))
            ch = from_kuten(*kt) if kt else None
            if ch:
                out.append(ch); i += 2; continue
        if c in LEAD3 and i + 2 < len(s) and s[i + 1] in T and s[i + 2] in T:
            kt = plane3_kuten(LEAD3.index(c) * 2601 + T.index(s[i + 1]) * 51 + T.index(s[i + 2]))
            ch = from_kuten(*kt) if kt else None
            if ch:
                out.append(ch); i += 3; continue
        out.append(c); i += 1
    return ''.join(out)

if __name__ == '__main__':
    import sys
    t = sys.argv[1] if len(sys.argv) > 1 else '勇者よ、ようこそ。ここは「イーグル・ピーク」の村だ！'
    e = encode(t)
    print(e, len(e), 'symbols for', len(t), 'chars')
    print(decode(e))
    assert decode(e) == t.upper()
