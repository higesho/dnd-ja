# 原文の抽出: text/en/*.json（{"source": ..., "strings": [{"id", "en"}]}）
#   python -X utf8 tools/extract_text.py
import sys, os, re, json, struct, glob
sys.path.insert(0, os.path.dirname(__file__))
from wwlib import read_cps, GAME

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'text', 'en')

def dec(b):
    return b.decode('latin1')

# ------------------------------------------------------------- START.EXE
FILES = {os.path.splitext(f)[0].upper() for f in os.listdir(GAME)} | {'LEVEL', 'INF', 'CPS', 'VCN', 'VMP', 'INTRO', 'MAP',
         'BORDER', 'DRANX', 'KHELDRAN', 'OUTTAKE', 'OUTPORTS', 'CHARGENA', 'CHARGENB', 'CHARGEN', 'INVENT', 'PLAYFLD',
         'FINALE1', 'FINALE2', 'DREAM', 'PORTALA', 'PORTALB', 'EST', 'EDT', 'VDISK', 'STR', 'INT', 'WIS', 'DEX', 'CON', 'CHA'}
KEEP_UPPER = {'STR', 'INT', 'WIS', 'DEX', 'CON', 'CHA', 'MAP'}   # 画面に出る略語は残す
SKIP_RANGES = [(0, 0x25000), (0x25e00, 0x25f80),            # コード・キー配列
               (0x27820, 0x279a0),                          # コピープロテクトの答え（入力語）
               (0x279a0, 0x28200),                          # 同梱キャラの名前（固有名のまま）
               (0x2d300, 0x2e3c0),                          # ランタイム
               (0x341ee, 0x345c3), (0x3472a, 0x34870),      # ファイル・メモリの致命的エラー
               (0x34b00, 0x3d6d0)]

def exe_strings():
    d = open(os.path.join(ROOT, 'orig', 'START.EXE'), 'rb').read()
    out = []
    for m in re.finditer(rb'[\x01-\x7e]{2,}(?=\x00)', d):
        s, off = m.group(), m.start()
        # 直前の 1 バイトが文字なら途中から拾っている
        while s and s[0] < 0x20 and s[0] not in (0x0d, 0x06, 0x02):
            s = s[1:]; off += 1
        if any(a <= off < b for a, b in SKIP_RANGES):
            continue
        t = dec(s)
        if sum(c.isalpha() for c in t) < 2:
            continue
        if any(ord(c) < 0x20 and c not in '\r\x06\x02\n' for c in t):
            continue
        good = sum(c.isalnum() or c in " .,!?'\"-:%/()\r\x06\x02\n+*;" for c in t)
        if good < len(t) * 0.9:
            continue
        core = t.strip()
        if re.fullmatch(r'[A-Z0-9_]+(\.[A-Za-z0-9]{1,3})?', core) and (core.split('.')[0] in FILES or '.' in core) and core not in KEEP_UPPER:
            continue
        if re.fullmatch(r'[%a-zA-Z0-9_]*\.(cps|CPS|PAL|EGA|SND|ADL|dcr|sav|SAV|DAT|ovl|TXT|EXE|FNT|%s)', core) or core in ('%s.%s', '%s%d.%s'):
            continue
        c = t.strip()
        if len(c) <= 4 and '%' not in c and (not c.replace("'", '').isalpha() or not re.search('[aeiouyAEIOUY]', c)) and c not in ('HP', 'AC'):
            continue                                                  # 短い記号の並び（コードの断片）
        if t.startswith('+') and d[off - 1] == 0x16:                 # 直前のポインタ表（xxxx:2b16）の 1 バイト       # 前のデータの 1 バイトが混じったもの
            t = t[1:]; off += 1
        out.append({'id': 'exe:%05x' % off, 'en': t})
    return out

# ------------------------------------------------------------- LEVELn.INF
def inf_strings(path):
    d, _ = read_cps(path)
    o = struct.unpack_from('<H', d, 0)[0]
    end = struct.unpack_from('<H', d, o)[0]
    # 台本の先頭語 = 文字列表までの距離。末尾 end までが NUL 区切りの文字列。
    best = None
    for x in range(o + 3, min(end, o + 0x800)):
        s0 = x + struct.unpack_from('<H', d, x)[0]
        if not (x < s0 < end) or d[s0 - 1] in range(0x20, 0x7f):
            continue
        reg = d[s0:end]
        if reg and reg[-1] == 0 and all(c < 0x7f for c in reg):
            if best is None or s0 < best[1]:
                best = (x, s0)
    if best is None:
        raise SystemExit('string table not found: ' + path)
    x, s0 = best
    strs = d[s0:end].split(b'\0')[:-1]
    return [dec(s) for s in strs], x, s0

# ------------------------------------------------------------- TEXT.DAT
def textdat():
    d = open(os.path.join(GAME, 'TEXT.DAT'), 'rb').read()
    n = struct.unpack_from('<H', d, 0)[0] // 2
    offs = list(struct.unpack_from('<%dH' % n, d, 0)) + [len(d)]
    return [dec(d[offs[i]:offs[i + 1]].split(b'\0')[0]) for i in range(n)]

# ------------------------------------------------------------- ITEM.DAT
def item_names():
    d = open(os.path.join(GAME, 'ITEM.DAT'), 'rb').read()
    n = struct.unpack_from('<H', d, 0)[0]
    p = 2 + n * 14
    m = struct.unpack_from('<H', d, p)[0]
    return [dec(d[p + 2 + 35 * i:p + 2 + 35 * (i + 1)].split(b'\0')[0]) for i in range(m)]

def main():
    os.makedirs(OUT, exist_ok=True)
    def save(name, items):
        json.dump({'source': name, 'strings': items}, open(os.path.join(OUT, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
        print(name, len(items))
    save('START.EXE', exe_strings())
    for f in sorted(glob.glob(os.path.join(GAME, 'LEVEL*.INF')), key=lambda p: int(re.findall(r'\d+', os.path.basename(p))[0])):
        strs, x, s0 = inf_strings(f)
        nm = os.path.basename(f).upper()
        save(nm, [{'id': '%s:%d' % (nm, i), 'en': s} for i, s in enumerate(strs)])
    save('TEXT.DAT', [{'id': 'TEXT.DAT:%d' % i, 'en': s} for i, s in enumerate(textdat())])
    save('ITEM.DAT', [{'id': 'ITEM.DAT:%d' % i, 'en': s} for i, s in enumerate(item_names())])

if __name__ == '__main__':
    main()
