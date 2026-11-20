# 訳す文を集める -> text/units.json
#   [{"id", "kind", "en", "max"(バイト数の上限。null = 制限なし), "where": [...]}]
#   kind: ui（EXE の文字列）, btn（ボタン）, name（データの固定欄の名前）, long（任務説明・噂・物語）, list（呪文の一覧）
import os, sys, re, json, glob, struct
sys.path.insert(0, os.path.dirname(__file__))
import sjdata as S

EXES = ['SJ.EXE', 'SF3.EXE', 'OTC.EXE']
# 表示しない文字列（ライブラリの内部名・エラー）
SKIP_EXE = re.compile(r'^(Borland C\+\+.*|Drw Bff.*|DA cntl struct|GUI Sys text|.*struct.*|Runtime overlay error|'
                      r'Divide error|Abnormal program termination|VDISK FAKE|PF|NA|TN|st|th|nd|rd|haha|'
                      r'JAN|FEB|MAY|AUG|OCT|NOV|DEC|JUNE|JULY|MARCH|APRIL|SEPT|NONE|SIR|M\'AAM|MADAME|HERO)$')
# 英語のまま残すもの（人名・地名・港の名前・船の種類。欄が短く日本語が入らないため、どこでも英語で統一する）
KEEP = set("""GRENDAL|LORIC BRIGHTSHIELD|MORAN SILVERLEAF|MELCAR THE WISE|NORY GREYBEARD|GROG STIFFBEARD|SIR AERON|
STEPHAN LONGLEGS|SOPA SWIFTFINGERS|ERICA RAVENHAIR|JASMINE BLOSSOM|JULIA VALADOR|MADAM MONA|LADY ARINA|EVA DULAC|
CYSTAL CLOVERLEAF|LATITIA LENDLOFT|ROBIN SWIFTSAIL|SPALDING HOREST|HOMER ILLIAD|SCOPES MOSEY|MORACK KLIK|GREVLIN DORV|
PETER THE BRAVE|MARKUS BELVIN|ELDRAN WHITEGLOVE|GRIMM FORMIST|SWIFTY NOBELLS|MIRAGE SPELLBINDER|CAROLINE CLOVERLEAF|
LILAC THE SWIFT|MAURINE THE MERRY|TISH YULDIRWICK|DANIELLE GOSHKIN|HOLLY GREEN|ANITA COULREN|KAYLA THE SWIFT|
MADAME GOLDTOUCH|ANADIA|COLIAR|TORIL|KARPRI|CHANDOS|GLYTH|GARDEN|H'CATHA|UMBERGUARD|HISSTA|HISSSTA|ATHENAR|WATERDEEP|
MINAGWBE|MINGABWE|CLIVE'S DEN|THE SPINDLE|WILDSPACE|GALLEON|NAUTILOID|MAN-O-WAR|MIND SPIDER|TRADESMAN|HAMMERSHIP|
DEATHSPIDER|SQUID-SHIP|WAR GALLEY|DRAGONFLY|QUEEN SPIDER|SHO LUNG TRADER|VERSION 1.1""".replace('\n', '').split('|'))
# ライブラリの内部の名前（メモリ不足などの報告に出る）・使われない文字列
LIB = re.compile(r'^(.*VMEM:.*|.*320x200.*|.*640x480.*|Uncomp Buff|DECOMP|Mouse save|SV Buff 2|Dia Struct|GUI .*|'
                 r'Sys butt|Change GUI str|DIA Subgadget|Need to open a file.*|.*glue.*|FH:|Node|Mel Version.*|'
                 r'Stock STR|S[Ee][Ll][Ll] str|SLT WP STR|Slct Wp|selct Offcr|SFCOMBAT|Ftemp|Couldn.t open file|'
                 r'.*NOT IMPLEMENTED)$')


ALWAYS = {'%s, now orbiting %s.', ' %d crew lost in the battle.'}     # 小文字だけだが画面に出るもの


def exe_ok(s):
    if s in ALWAYS: return True
    if len(s) < 3: return False
    if not re.search(r'[A-Za-z]{2}', s): return False
    if not re.search(r'[A-Z]', s): return False                      # 小文字だけ = 内部の名前
    if re.fullmatch(r'[\w~*%.]+\.[A-Za-z]{2,3}', s): return False      # ファイル名
    if s.startswith(('~', '`', '<', '#', '!', '*')): return False
    if re.search(r'[A-Z][a-z]+[A-Z]', s) and ' ' not in s: return False
    if SKIP_EXE.match(s) or LIB.match(s) or s.strip() in KEEP: return False
    return True


def collect_exe(units):
    for name in EXES:
        for dsoff, fo, s in S.exe_strings(name):
            # 前の文字列の残り（'.' など）が頭に付いているものは外す
            m = re.match(r'^(?:[^A-Za-z0-9%\'"( ^]+|[a-z]{4,})(?=[A-Z^])', s)
            if m:
                k = len(m.group())
                dsoff += k; fo += k; s = s[k:]
            if not exe_ok(s): continue
            kind = 'btn' if '^' in s else 'ui'
            units.add(kind, s, len(s), '%s:%04x' % (name, dsoff))


def collect_btn(units):
    for f in sorted(glob.glob(os.path.join(S.ART, '*.BTN'))):
        d = S.read(f)
        for p, q, s in S.btn_parse(d):
            s = s.decode('latin1')
            if s[:1] in '~`': continue
            if re.fullmatch(r'\^[A-Z0-9][:]?( [-+][A-Z])?', s): continue      # ^A, ^B:, ^A -W
            units.add('btn', s, None, 'BTN:' + os.path.basename(f))


def collect_long(units):
    for f in sorted(glob.glob(os.path.join(S.ART, 'MISS*.TXT'))):
        t = S.miss_text(S.read(f)).decode('latin1')
        units.add('long', t, None, 'MISS:' + os.path.basename(f))
    seen = set()
    for f in sorted(glob.glob(os.path.join(S.ART, '*.RUM'))):
        for i, r in enumerate(S.rum_records(S.read(f))):
            t = r.rstrip(b' \x00').decode('latin1')
            if not t: continue
            units.add('long', t, 255, 'RUM:%s:%d' % (os.path.basename(f), i))
    for i, e in enumerate(S.story_entries(S.read(os.path.join(S.GAME, 'STORY.LIB')))):
        e = e.decode('latin1')
        if '@' not in e: continue
        body = e[e.index('@') + 1:].replace('\n', ' ').strip()
        body = re.sub(r' {2,}', ' ', body)
        if body.lower().startswith('junk'): continue
        units.add('long', body, None, 'STORY:%d' % i)


# 固定欄の名前: (ファイル, レコード長, [欄の位置]). 元と同じバイト数以内で上書きする（欄の大きさが分からないため）
NAME_FIELDS = [('ITEMS.ITM', 136, [0x4a, 0x69]), ('MUSPELL.ITM', 136, [0x4a]), ('CLERICSP.ITM', 136, [0x4a]),
               ('CARGO.CRG', 93, [30, 61]), ('MONSTERS.CHR', 524, [300])]


def name_fields():
    """[(ファイル, 位置, 文字列)]"""
    out = []
    for fn, rs, offs in NAME_FIELDS:
        d = S.read(os.path.join(S.ART, fn))
        for r in range(0, len(d), rs):
            for o in offs:
                p = r + o
                e = d.index(b'\x00', p)
                s = d[p:e]
                if not re.fullmatch(rb"[A-Z][A-Z0-9 ,'.()+#/!-]{1,}", s): continue
                s = s.decode('latin1')
                if re.fullmatch(r'(MU|CLERIC) SPELL #\d+|NOT USED', s): continue
                out.append((fn, p, s))
    return out


def collect_names(units):
    for fn, p, s in name_fields():
        units.add('name', s, len(s), 'NAME:%s:%x' % (fn, p))
    for fn in ['MUSPELLS.DAT', 'CLSPELLS.DAT']:
        for l in S.read(os.path.join(S.ART, fn)).decode('latin1').split('\r\n'):
            if l.strip(): units.add('list', l, None, 'LIST:' + fn)


class Units:
    def __init__(self):
        self.by = {}
    def add(self, kind, en, mx, where):
        u = self.by.get(en)
        if not u:
            u = self.by[en] = {'kind': kind, 'en': en, 'max': mx, 'where': []}
        else:
            if mx is not None: u['max'] = mx if u['max'] is None else min(u['max'], mx)
            if kind == 'ui' and u['kind'] == 'name': u['kind'] = 'ui'
        u['where'].append(where)


def main():
    units = Units()
    collect_exe(units)
    collect_btn(units)
    collect_names(units)
    collect_long(units)
    order = {'btn': 0, 'ui': 1, 'name': 2, 'list': 3, 'long': 4}
    out = sorted(units.by.values(), key=lambda u: (order[u['kind']], u['where'][0]))
    for i, u in enumerate(out): u['id'] = 'u%04d' % i
    json.dump(out, open(os.path.join(S.ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    from collections import Counter
    c = Counter(u['kind'] for u in out)
    print(len(out), dict(c), 'chars', sum(len(u['en']) for u in out))


if __name__ == '__main__':
    main()
