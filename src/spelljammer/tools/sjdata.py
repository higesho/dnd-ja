# Spelljammer のファイル形式（読み書き）
import os, re, struct
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'SPELLJAM')
ART = os.path.join(GAME, 'ART')
KEY = bytes.fromhex('a1091afb6b551c9a')        # 噂・任務説明・STR.DAT の暗号（8 バイトの繰り返し XOR。文の先頭から）


def xor(d):
    return bytes(b ^ KEY[i % 8] for i, b in enumerate(d))


def read(path):
    return open(path, 'rb').read()


def mz_dgroup(d):
    """Borland の EXE: データ領域（DGROUP）のファイル上の位置と、読み込む像の終わり"""
    hdr = struct.unpack_from('<H', d, 8)[0] * 16
    last, pages = struct.unpack_from('<HH', d, 2)
    end = (pages - 1) * 512 + last if last else pages * 512
    ip, cs = struct.unpack_from('<HH', d, 0x14)
    ent = hdr + cs * 16 + ip
    assert d[ent] == 0xba
    return hdr + struct.unpack_from('<H', d, ent + 1)[0] * 16, end


def exe_strings(name):
    """[(DGROUP 内の位置, ファイル上の位置, 文字列)]"""
    d = read(os.path.join(GAME, name))
    ds, end = mz_dgroup(d)
    out = []
    for m in re.finditer(rb'[\x20-\x7e]{2,}\x00', d[ds:end]):
        out.append((m.start(), ds + m.start(), m.group()[:-1].decode('latin1')))
    return out


# ---- ボタン（*.BTN）: レコード = 0x14 バイト + u16 w（0 でなければ +2 バイト）+ 0x26 バイト + u16 長さ + 文字列
def btn_parse(d):
    recs, p = [], 0
    while p < len(d):
        w = struct.unpack_from('<H', d, p + 0x14)[0]
        q = p + 0x16 + (2 if w else 0) + 0x26
        n = struct.unpack_from('<H', d, q)[0]
        recs.append((p, q, d[q + 2:q + 2 + n]))
        p = q + 2 + n
    assert p == len(d)
    return recs


def btn_build(d, f):
    """f(ラベル) -> 新しいラベル（bytes）"""
    out, last = b'', 0
    for p, q, s in btn_parse(d):
        ns = f(s)
        out += d[p:q] + struct.pack('<H', len(ns)) + ns
    return out


# ---- 噂（*.RUM）: 256 バイトのレコード x 80（空白で埋める）, 全体を XOR
def rum_records(d):
    p = xor(d)
    return [p[i:i + 256] for i in range(0, len(p), 256)]


# ---- 任務説明（MISS*.TXT）: u16 n, XOR(本文 + NUL)（n バイト）, 2 バイト
def miss_text(d):
    n = struct.unpack_from('<H', d, 0)[0]
    t = xor(d[2:2 + n])
    return t[:t.index(b'\x00')]


def miss_build(text):
    t = text + b'\x00'
    return struct.pack('<H', len(t)) + xor(t) + b'\x00\x00'


# ---- STORY.LIB: u32 位置 x 100（使わない所は 0xffffffff）, 項目 = u16 長さ + 本文
def story_entries(d):
    offs = [o for o in struct.unpack_from('<100I', d, 0) if o != 0xffffffff]
    ents = []
    for o in offs:
        n = struct.unpack_from('<H', d, o)[0]
        ents.append(d[o + 2:o + 2 + n])
    return ents


def story_build(ents):
    out = bytearray(b'\xff' * 400)
    p = 400
    body = b''
    for i, e in enumerate(ents):
        struct.pack_into('<I', out, i * 4, p + len(body))
        body += struct.pack('<H', len(e)) + e
    return bytes(out) + body
