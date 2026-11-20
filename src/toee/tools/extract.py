# game text -> text/src.json [{"f": file, "k": kind, "id": field id, "en": text}]
# kinds: mes (UI/messages), dlgn (NPC line in a dialog), dlgp (PC reply), help (help.tab title/body)
import sys, os, re, json
sys.path.insert(0, os.path.dirname(__file__))
import vfs

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

MES_DIRS = ('mes/', 'tpmes/', 'co8mes/', 'movies/')
SKIP = {
    'mes/map_numbers.mes', 'mes/sound.mes', 'mes/sound_schemes.mes', 'mes/portrait.mes', 'mes/dialog.mes',
}


def has_text(t):
    return re.search(r'[A-Za-z]{2}', t) is not None


def fields(d):
    # .mes/.dlg comments start with // outside braces
    out, i, n = [], 0, len(d)
    while i < n:
        if d.startswith('//', i):
            j = d.find('\n', i)
            i = n if j < 0 else j
            continue
        if d[i] == '{':
            j = d.find('}', i)
            if j < 0: break
            out.append(d[i + 1:j])
            i = j + 1
            continue
        i += 1
    return out


def units():
    fs = vfs.files()
    res = []
    for path in sorted(fs):
        if path in SKIP: continue
        if path.endswith('.mes') and path.startswith(MES_DIRS):
            f = fields(vfs.read(path).decode('cp1252', 'replace'))
            for i in range(0, len(f) - 1, 2):
                t = f[i + 1].strip()
                if has_text(t): res.append({'f': path, 'k': 'mes', 'id': f[i].strip(), 'en': t})
        elif path.endswith('.dlg') and path.startswith('dlg/'):
            f = fields(vfs.read(path).decode('cp1252', 'replace'))
            for i in range(0, len(f) - 6, 7):
                num, m, fe, iq = f[i].strip(), f[i + 1].strip(), f[i + 2].strip(), f[i + 3].strip()
                kind = 'dlgp' if iq not in ('', '0') or not fe else 'dlgn'
                for t in (m, fe):
                    # PC lines like "A:", "B:", "E:" are engine commands
                    if has_text(t) and not re.fullmatch(r'[A-Z]:\s*', t):
                        res.append({'f': path, 'k': kind, 'id': num, 'en': t})
        elif path == 'mes/help.tab':
            # one entry per CRLF line; inside a text column \x0b is a line feed, every piece is translated alone
            for line in vfs.read(path).decode('cp1252', 'replace').split('\r\n'):
                cols = line.split('\t')
                if len(cols) < 6: continue
                for c in cols[4:]:
                    for seg in c.split('\x0b'):
                        if has_text(seg): res.append({'f': path, 'k': 'help', 'id': cols[0], 'en': seg.strip(' \r\n\t\x0c')})
    return res


if __name__ == '__main__':
    u = units()
    json.dump(u, open(os.path.join(ROOT, 'text', 'src.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    uniq = {x['en'] for x in u}
    print(len(u), 'units', len(uniq), 'unique', sum(len(t.split()) for t in uniq), 'words',
          len({x['f'] for x in u}), 'files')
