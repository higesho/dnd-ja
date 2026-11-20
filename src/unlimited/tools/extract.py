# 翻訳する英文を抜き出す → text/units.json [[出どころ, 英文], ...]（重複なし）
#   EXE: CKIT.EXE の画面の文字列（C 文字列）
#   NAM: 名前の表（STRG.GLB と各デザインの STRGnnn.DAT: 怪物・壁・モジュール・呪文などの名前、6 ビット詰め 16 字）
#   DSN: 付属デザイン（Heirs to Skull Crag、チュートリアル）のイベントの文章（GEOnnn.DAT の STRG）と品物の名前（GAME001.DAT）
#   SCR: エディタの各ページの文字（DISK3/SCRIPT.GLB）
#   BTN: イベントの選択肢（"^HELP THEM^SLINK AWAY^" を ^ で分けた 1 つずつ。ゲームがボタンにする）
import sys, os, re, json, struct, glob
sys.path.insert(0, os.path.dirname(__file__))
import dump_text
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
G = os.path.join(ROOT, 'test', 'game', 'UA')
ORIG = os.path.abspath(os.path.join(ROOT, '..'))       # 手を付けていない元のファイル
EXE_DATA = 0x85000                                      # これより後ろが文字列のあるデータ

def exe_strings():
    e = open(os.path.join(ORIG, 'GAME', 'UA', 'CKIT.EXE'), 'rb').read()
    out = []
    for m in re.finditer(rb'(?<=\x00)[\x20-\x7e]{2,}(?=\x00)', e):
        if m.start() < EXE_DATA: continue
        s = m.group(0)
        t = re.sub(rb'%[-0-9.*]*[a-zA-Z]', b'', s)
        if re.search(rb'\.(tlb|glb|xmi|cmp|dat|qsv|qch|cfg|exe|adv|fon|voc|dsn|csv|cch|txt|bat)\b', s, re.I): continue
        if not re.search(rb'[A-Za-z]{2}', t): continue
        words = re.findall(rb'[A-Za-z]+', t)
        if not words: continue
        vowelish = sum(1 for w in words if re.search(rb'[AEIOUYaeiouy]', w))
        if vowelish < max(1, len(words) * 0.6): continue
        if re.search(rb'[\\{}\[\]|^`~]', s): continue
        out.append(('EXE:%05x' % m.start(), s.decode('latin1')))
    return out

def unpack16(b):
    v = dump_text.unpack6(b)
    return ''.join(chr(x + 0x40) if x <= 0x1f else chr(x) for x in v).split('@')[0]

def hlib(path):
    d = open(path, 'rb').read()
    n = struct.unpack_from('<H', d, 8)[0]
    offs = struct.unpack_from('<%dI' % (n + 1), d, 16)
    return [d[offs[i]:offs[i + 1]] for i in range(n)]

def name_records(e, skip=0):
    return [unpack16(e[i + 2:i + 14]) for i in range(skip, len(e) - 13, 14)]

def names():
    out = []
    for k, e in enumerate(hlib(os.path.join(ORIG, 'GAME', 'UA', 'DISK3', 'STRG.GLB'))[1:], 1):
        for i, s in enumerate(name_records(e)):
            if s.strip(): out.append(('NAM:glb%d:%03d' % (k, i), s))
    for dsn in ('HEIRS.DSN', 'TUTORIAL.DSN'):
        for f in sorted(glob.glob(os.path.join(ORIG, 'DESIGNS', 'UA', dsn, 'STRG*.DAT'))):
            for i, s in enumerate(name_records(open(f, 'rb').read(), 14)):
                if s.strip(): out.append(('NAM:%s:%s:%03d' % (dsn[:4], os.path.basename(f)[4:7], i), s))
    return out

def design():
    out = []
    for dsn in ('HEIRS.DSN', 'TUTORIAL.DSN'):
        d = os.path.join(ORIG, 'DESIGNS', 'UA', dsn)
        g = open(os.path.join(d, 'GAME001.DAT'), 'rb').read()
        for m in re.finditer(rb'[\x20-\x7e]{3,}', g):
            out.append(('DSN:%s:game:%03x' % (dsn[:4], m.start()), m.group().decode('latin1')))
        for f in sorted(glob.glob(os.path.join(d, 'GEO*.DAT'))):
            for i, l, t in dump_text.strings(f):
                src = '%s:%s:%03d' % (dsn[:4], os.path.basename(f)[3:6], i)
                if t.startswith('^') and t.count('^') >= 2:
                    for k, lab in enumerate(x for x in t.split('^') if x.strip()):
                        out.append(('BTN:%s:%d' % (src, k), lab))
                elif len(t.strip()) >= 2 and re.search('[A-Z]{2}', t):
                    out.append(('DSN:' + src, t))
    return out

def script_strings():
    """DISK3/SCRIPT.GLB（エディタの各ページの定義）の中の文字列。直前に「長さ + 2」のバイトがある"""
    d = open(os.path.join(ORIG, 'GAME', 'UA', 'DISK3', 'SCRIPT.GLB'), 'rb').read()
    out = []
    for m in re.finditer(rb'[\x20-\x7e]{2,}(?=\x00)', d):
        s = m.group().decode('latin1'); st = m.start()
        ok = d[st - 1] == len(s) + 2
        if not ok:      # 印字できる字が長さバイトの前に付いていたら、合う位置から取り直す
            for k in range(1, len(s) - 1):
                if ord(s[k - 1]) == len(s) - k + 2:
                    s = s[k:]; st += k; ok = True; break
        if ok and re.search(r'[A-Za-z]{2}', s):
            out.append(('SCR:%05x' % st, s))
    return out

if __name__ == '__main__':
    seen = set(); res = []
    for src, s in exe_strings() + names() + design() + script_strings():
        if s in seen: continue
        seen.add(s); res.append([src, s])
    os.makedirs(os.path.join(ROOT, 'text'), exist_ok=True)
    json.dump(res, open(os.path.join(ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    import collections
    c = collections.Counter(a[:3] for a, _ in res); ch = collections.Counter()
    for a, s in res: ch[a[:3]] += len(s)
    print(len(res), 'units', sum(len(s) for _, s in res), 'chars', dict(c), dict(ch))
