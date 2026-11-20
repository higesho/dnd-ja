# Check a translated chunk:  python tools/check_chunk.py NAME [NAME..]
#   text/chunks/NAME.json (English units) vs text/ja/NAME.json ([{"id":..,"ja":..}])
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SPEC = re.compile(r'%[-+#0]*\d*(?:\.\d+)?[dsiuxXf]|\$[A-Z][A-Z_]+')


def check(name):
    src = json.load(open(os.path.join(ROOT, 'text', 'chunks', name + '.json'), encoding='utf-8'))
    p = os.path.join(ROOT, 'text', 'ja', name + '.json')
    if not os.path.exists(p):
        print(name, 'NO OUTPUT'); return 1
    try:
        out = json.load(open(p, encoding='utf-8'))
    except Exception as e:
        print(name, 'BAD JSON', e); return 1
    ja = {}
    for u in out:
        if not isinstance(u, dict) or 'id' not in u or 'ja' not in u:
            print(name, 'bad entry', str(u)[:80]); continue
        ja[u['id']] = u['ja']
    errs = 0
    for u in src:
        i, en = u['id'], u['en']
        if i not in ja:
            print(name, i, 'MISSING'); errs += 1; continue
        j = ja[i]
        if not isinstance(j, str) or not j.strip():
            print(name, i, 'empty / not a string'); errs += 1; continue
        if sorted(SPEC.findall(en)) != sorted(SPEC.findall(j)):
            print(name, i, 'SPEC', SPEC.findall(en), '->', SPEC.findall(j), '|', j[:60]); errs += 1
        if en.count('\n') != j.count('\n'):
            print(name, i, 'NEWLINES %d->%d' % (en.count('\n'), j.count('\n')), '|', j[:60]); errs += 1
        if en.count('|') != j.count('|'):
            print(name, i, "'|' count %d->%d" % (en.count('|'), j.count('|')), '|', j[:60]); errs += 1
        if re.search(r'[｡-ﾟ]', j):
            print(name, i, 'half-width kana', '|', j[:60]); errs += 1
        if re.search(r'[A-Za-z]{4,}', j) and re.search(r'[぀-ヿ一-鿿]', j) is None and re.search(r'[a-z]', en) and len(en) > 3:
            print(name, i, 'not translated?', '|', j[:60]); errs += 1
        try:
            j.replace('～', '〜').replace('－', '−').replace('—', '―').replace('∥', '‖').replace('…', '').replace('’', '').encode('cp932')
        except UnicodeEncodeError as e:
            print(name, i, 'unusual character:', repr(j[e.start:e.end])); errs += 1
    extra = set(ja) - {u['id'] for u in src}
    if extra: print(name, 'unknown ids', sorted(extra)[:10])
    print(name, 'OK' if not errs else '%d problems' % errs, '(%d/%d)' % (len(ja), len(src)))
    return errs


if __name__ == '__main__':
    bad = 0
    for n in sys.argv[1:]: bad += check(n) > 0
    sys.exit(1 if bad else 0)
