"""Pool of Radiance: Ruins of Myth Drannor (SNEG 版) のテキスト形式の読み書き。

対応形式:
  - sfDat.dat           : 難読化された文字列表（1文字=2バイト）
  - sneg_ui_strings.dat : UI 文字列（SUIS 形式, 値は UTF-8）
  - *.str               : 改行区切りの文字列
  - *.por               : タブ区切りのダイアログ（ID / 台詞 / 話者 / ト書き）
"""
import struct

ENGLISH_CP = "cp1252"
JA_CP = "cp932"


# ---------------------------------------------------------------- sfDat.dat
# 形式: u32 件数 n, u32 総文字数, u32 オフセット[n]（文字単位）, 本体 u16[総文字数]
# 本体の各 u16 = sext8(平文バイト) XOR key16
#   K = (0xBB + 文字列番号) & 0xFF,  key16 = (K << 8) | ((K - 0x66) & 0xFF)
def _key16(i):
    k = (0xBB + i) & 0xFF
    return (k << 8) | ((k - 0x66) & 0xFF)


def read_sfdat(path):
    """bytes のリスト（エンコード済みの生文字列）を返す。"""
    d = open(path, "rb").read()
    n, total = struct.unpack_from("<II", d, 0)
    offs = list(struct.unpack_from("<%dI" % n, d, 8))
    base = 8 + 4 * n
    vals = struct.unpack_from("<%dH" % total, d, base)
    out = []
    for i in range(n):
        a = offs[i]
        b = offs[i + 1] if i + 1 < n else total
        key = _key16(i)
        out.append(bytes((v ^ key) & 0xFF for v in vals[a:b]))
    return out


def write_sfdat(path, strings):
    """strings: bytes のリスト（エンコード済み）。"""
    offs, body, pos = [], [], 0
    for i, s in enumerate(strings):
        offs.append(pos)
        key = _key16(i)
        for c in s:
            sx = c | 0xFF00 if c >= 0x80 else c
            body.append(sx ^ key)
        pos += len(s)
    d = struct.pack("<II", len(strings), pos)
    d += struct.pack("<%dI" % len(offs), *offs)
    d += struct.pack("<%dH" % len(body), *body)
    open(path, "wb").write(d)


# ------------------------------------------------------- sneg_ui_strings.dat
# 形式: "SUIS", u32 版(1), u16 len + 言語コード, u32 件数,
#       各項目 [u16 len + key, u32 コードページ(65001), u16 len + 値]
def read_ui(path):
    u = open(path, "rb").read()
    assert u[:4] == b"SUIS"
    p = 4
    ver = struct.unpack_from("<I", u, p)[0]; p += 4
    ln = struct.unpack_from("<H", u, p)[0]; p += 2
    lang = u[p:p + ln].decode(); p += ln
    cnt = struct.unpack_from("<I", u, p)[0]; p += 4
    items = []
    for _ in range(cnt):
        kl = struct.unpack_from("<H", u, p)[0]; p += 2
        k = u[p:p + kl].decode("utf-8"); p += kl
        cp = struct.unpack_from("<I", u, p)[0]; p += 4
        vl = struct.unpack_from("<H", u, p)[0]; p += 2
        v = u[p:p + vl].decode("utf-8"); p += vl
        items.append((k, cp, v))
    assert p == len(u)
    return ver, lang, items


def write_ui(path, ver, lang, items):
    out = bytearray(b"SUIS")
    out += struct.pack("<I", ver)
    lb = lang.encode()
    out += struct.pack("<H", len(lb)) + lb
    out += struct.pack("<I", len(items))
    for k, cp, v in items:
        kb, vb = k.encode("utf-8"), v.encode("utf-8")
        out += struct.pack("<H", len(kb)) + kb
        out += struct.pack("<I", cp)
        out += struct.pack("<H", len(vb)) + vb
    open(path, "wb").write(bytes(out))


# ------------------------------------------------------------- .str / .por
def read_lines(path):
    """改行で分割した bytes の行リスト（行末の CR は除去）と、末尾改行の有無を返す。"""
    d = open(path, "rb").read()
    trailing = d.endswith(b"\n")
    lines = d.split(b"\n")
    if trailing:
        lines = lines[:-1]
    return [l[:-1] if l.endswith(b"\r") else l for l in lines], trailing


def write_lines(path, lines, trailing=True, eol=b"\r\n"):
    d = eol.join(lines)
    if trailing:
        d += eol
    open(path, "wb").write(d)


def read_por(path):
    """[[フィールド(bytes), ...], ...] を返す。"""
    lines, trailing = read_lines(path)
    return [l.split(b"\t") for l in lines], trailing


def write_por(path, rows, trailing=True):
    write_lines(path, [b"\t".join(r) for r in rows], trailing)
