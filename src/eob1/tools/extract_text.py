# 原文を text/src/*.json に取り出す（翻訳の元）
#   python -X utf8 tools/extract_text.py
import sys, os, re, json, struct
sys.path.insert(0, os.path.dirname(__file__))
import pak

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = pak.GAME
OUT = os.path.join(ROOT, 'text', 'src')

FILE_EXT = re.compile(r'\.(CPS|EGA|CMP|COL|DAT|PAL|VCN|VMP|ECN|EMP|INF|MAZ|PAK|EXE|SAV|TMP|VGA|FNT|OVL)$', re.I)

# EXE の中で、遊ぶ人に見える文章がある範囲（ファイル位置）
EXE_RANGES = [(0x22800, 0x22e70), (0x26270, 0x26b47), (0x26de0, 0x26e58), (0x27230, 0x27560),
              (0x28fe0, 0x29900), (0x29c00, 0x29fac), (0x2a280, 0x2a2be), (0x2a2be, 0x2a330),
              (0x2b67f, 0x2bacc)]
EXE_SKIP = {'PLAYFLD', 'INVENT', 'CHARGENA', 'CHARGEN', 'CHARGENB', 'PORTALA', 'PORTALB', 'XDEATH1', 'XDEATH2',
            'XDEATH3', 'OUTTAKE', 'adlib.dat', 'pcgamesn.dat', 'Font8.fnt', 'FONT6.FNT', 'homie', 'CPS', 'EGA',
            'Turbo C++ - Copyright 1990 Borland Intl.', 'Map compression failure: size of map = %d', '6jHP'}

def exe_strings():
    d = open(os.path.join(GAME, 'EOB.EXE'), 'rb').read()
    out, seen = [], set()
    for a, b in EXE_RANGES:
        for m in re.finditer(rb'[\x09\x0d\x20-\x7e]+(?=\x00)', d[a:b]):
            s = m.group().decode('latin1')
            if s in EXE_SKIP or FILE_EXT.search(s) or s.endswith('.ovl') or s.endswith('.col'):
                continue
            if not re.search(r'[A-Za-z]{2}', re.sub(r'%[-0-9]*[sdx]', '', s)):
                continue
            if s in seen:
                continue
            seen.add(s)
            out.append({'id': 'exe_%05x' % (a + m.start()), 'en': s})
        # 色の指定（0x06 + 1 バイト）をはさむ文（レベルアップ、アンデッド退散など）
        for m in re.finditer(rb'(?:\x06[\x01-\x0f]|[\x09\x0d\x20-\x7e])*\x06[\x01-\x0f](?:\x06[\x01-\x0f]|[\x09\x0d\x20-\x7e])*(?=\x00)', d[a:b]):
            s = re.sub(r'\x06[\x01-\x0f]', '', m.group().decode('latin1'))
            if not re.search(r'[A-Za-z]{3}', s) or s in seen:
                continue
            seen.add(s)
            out.append({'id': 'exe_%05x' % (a + m.start()), 'en': s})
    return out

def inf_strings():
    f = pak.all_files()
    out, seen = [], set()
    for i in range(1, 13):
        b = pak.cps(f['LEVEL%d.INF' % i][1])
        for m in re.finditer(rb'(?<=[\x00-\x1f\x7f-\xff])[\x20-\x7e\r]{3,}(?=\x00)', b):
            s = m.group().decode('latin1')
            if re.search(r'\.(maz|dat)$', s) or (re.fullmatch(r'[a-z0-9]+', s) and s not in ('museum',)):
                continue
            if s.lower() in seen:
                continue
            seen.add(s.lower())
            out.append({'id': 'inf%02d_%04x' % (i, m.start()), 'en': s})
    return out

def textdat():
    t = pak.all_files()['TEXT.DAT'][1]
    n = struct.unpack_from('<H', t, 0)[0] // 2
    offs = struct.unpack_from('<%dH' % n, t, 0)
    out = []
    for i, o in enumerate(offs):
        e = t.index(b'\0', o)
        out.append({'id': 'text_%02d' % i, 'en': t[o:e].decode('latin1')})
    return out

def items():
    it = pak.all_files()['ITEM.DAT'][1]
    n = struct.unpack_from('<H', it, 0)[0]
    p = 2 + n * 14
    nn = struct.unpack_from('<H', it, p)[0]
    p += 2
    out, seen = [], set()
    for i in range(nn):
        s = it[p + i * 35:p + i * 35 + 35].split(b'\0')[0].decode('latin1')
        if not s or s == 'NULL' or s.lower() in seen:
            continue
        seen.add(s.lower())
        out.append({'id': 'item_%02d' % i, 'en': s})
    return out

def main():
    os.makedirs(OUT, exist_ok=True)
    for name, fn in [('exe', exe_strings), ('inf', inf_strings), ('textdat', textdat), ('items', items)]:
        u = fn()
        json.dump(u, open(os.path.join(OUT, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
        print(name, len(u), sum(len(x['en']) for x in u), 'chars')

if __name__ == '__main__':
    main()
