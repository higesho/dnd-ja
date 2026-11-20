# DK.EXE の画面に出る文字列を集める -> text/units.json [{"id", "en", "va", "note"?}]
import os, re, json, pefile
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))

PATH = re.compile(r'(\\|/[a-z]|\.(smk|pcx|pcz|img|wav|lvl|mon|c|dat|sav|SMK|PCX|PCZ|WAV|LVL|MON)\b)')
SKIP = re.compile(r'(overflow|level file|Error |error |'
                  r'sortPolys|touchedAdorns|^Not a valid|runtime error|^- |Assertion|<program|\bwanted\b|Allocation|'
                  r'^Cannot (open|read|create|find)|^Can\'t find|Out of memory|^Invalid (tile|palette)|^bad art|'
                  r'maxPoints|maxQuads|maxTile|maxMonst|maxAdorn|maxSpell|Universe too large|level file =|'
                  r'^File (is out|version)|^image wrong|DOMAIN|SING|TLOSS|R60\d\d|^Microsoft|'
                  r'^GetLastActivePopup|^GetActiveWindow|^MessageBoxA|^user32|^kernel32|Sun.on|Jan.eb|^This isn|^EAX:)', re.I)


def strings():
    p = pefile.PE(os.path.join(GAME, 'DK.EXE'))
    d = bytes(p.__data__)
    out = []
    for s in p.sections:
        if s.Name.rstrip(b'\0') not in (b'.data', b'.rdata'): continue
        data = d[s.PointerToRawData:s.PointerToRawData + s.SizeOfRawData]
        for m in re.finditer(rb'(?<=[\x00-\x1f\x7f-\xff])[\x20-\x7e]{2,}(?=\x00)', data):
            out.append((0x400000 + s.VirtualAddress + m.start(), m.group().decode('latin1')))
    return out


JUNK = set('''Melee|<fBBf<|Xllll|RightGain0|LeftGain0|MS Sans Serif|DeathKeep v1.0|Can't deal with that many drives!|EAX: %x EBX: %x EDX: %x|no monster info file|Levels|Shell_TrayWnd|AppMenu|AppIcon|AstralLevel|:FindKey|Portal|EndStart|Find|Astral|.Death|.Combat|.Fanfare|RestZone|.Pause|Intrmiss|TitleSrn|Lion|DirectSoundCreate FAILED|DSOUND.DLL|Wrong PCX version in %s|Wrong PCX version|(null)|Program:|1#QNAN|1#INF|1#IND|1#SNAN|Unable to register the main window class|DeathKeep|Can't open %s|Tentamort'''.split('|'))


def ok(s):
    t = s.strip()
    if s in JUNK or t in JUNK: return False
    if t.startswith('#') or (len(t) == 3 and t.endswith('F') and not t.isupper()) or (len(t) == 3 and t.endswith('D') and t[1] in 'hgfed'): return False
    if len(t) < 3 or not re.search(r'[A-Za-z]{3}', t): return False
    if SKIP.search(t) or PATH.search(t): return False
    if re.fullmatch(r'[A-Z0-9_]+', t): return False                    # 内部の名前（MAGICMISSILE など）
    if re.fullmatch(r'[a-z0-9_.]+', t): return False
    if re.fullmatch(r'[A-Za-z]+[A-Z][a-z]+[A-Z]?[a-z0-9]*', t) and ' ' not in t and len(t) > 12: return False
    if re.search(r'[^\x20-\x7e]', t): return False
    if re.search(r'[`~^|{}]', t): return False
    return True


def main():
    seen, units = {}, []
    for va, s in strings():
        if not ok(s) or s in seen: continue
        seen[s] = 1
        units.append({'id': 'u%04d' % len(units), 'en': s, 'va': '%08x' % va})
    for e in (' on.', ' off.', '%s (%d)', 'AC:', 'HP:', '1st', '2nd', '3rd', '4th', '5th', '6th', '7th', '8th', '9th', 'Tab'):
        if e not in seen:
            seen[e] = 1
            units.append({'id': 'u%04d' % len(units), 'en': e, 'va': 'extra'})
    import glob
    for f in sorted(glob.glob(os.path.join(GAME, 'INFO', '*.MON'))):
        m = re.search(r'^#NAME\s+(.+?)\s*$', open(f, encoding='latin1').read(), re.M)
        if m and m.group(1) not in seen:
            seen[m.group(1)] = 1
            units.append({'id': 'u%04d' % len(units), 'en': m.group(1), 'va': 'mon', 'note': 'monster name (' + os.path.basename(f) + ')'})
    json.dump(units, open(os.path.join(ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(len(units), 'units', sum(len(u['en']) for u in units), 'chars')


if __name__ == '__main__':
    main()
