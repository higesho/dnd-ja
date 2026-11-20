# Check translated chunks:  python tools/check_chunk.py NAME [NAME..]
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TOK = re.compile(r'@pcname@|@[0-9t]|\[[A-Z_]{2,}\]|\[TAG_[A-Za-z0-9_]+\]|\[CMD_[A-Z_]+\]|%[-+#0]*\d*(?:\.\d+)?[dsiuxXf]')


def toks(s):
    return sorted(TOK.findall(s))


def check(name):
    src = json.load(open(os.path.join(ROOT, 'text', 'chunks', name + '.json'), encoding='utf-8'))
    p = os.path.join(ROOT, 'text', 'ja', name + '.json')
    if not os.path.exists(p):
        print(name, 'NO OUTPUT'); return 1
    try:
        out = json.load(open(p, encoding='utf-8'))
    except Exception as e:
        print(name, 'BAD JSON', e); return 1
    ja = {u['id']: u['ja'] for u in out if isinstance(u, dict) and 'id' in u and 'ja' in u}
    errs = 0
    for u in src:
        i, en = u['id'], u['en']
        j = ja.get(i)
        if not isinstance(j, str) or not j.strip():
            print(name, i, 'MISSING'); errs += 1; continue
        if toks(en) != toks(j):
            print(name, i, 'TOKENS', toks(en), '->', toks(j), '|', j[:50]); errs += 1
        if en.count('~') != j.count('~'):
            print(name, i, "'~' count %d->%d" % (en.count('~'), j.count('~')), '|', j[:50]); errs += 1
        if en.count('|') != j.count('|'):
            print(name, i, "'|' count %d->%d" % (en.count('|'), j.count('|')), '|', j[:50]); errs += 1
        if en.count('\n') != j.count('\n'):
            print(name, i, 'NEWLINES %d->%d' % (en.count('\n'), j.count('\n')), '|', j[:50]); errs += 1
        if '{' in j or '}' in j or '\t' in j:
            print(name, i, 'braces/tab not allowed', '|', j[:50]); errs += 1
        if re.search(r'[｡-ﾟ]', j):
            print(name, i, 'half-width kana', '|', j[:50]); errs += 1
        if len(en) > 3 and re.search(r'[a-z]{3}', en) and not re.search(r'[぀-ヿ一-鿿]', j):
            print(name, i, 'not translated?', '|', j[:50]); errs += 1
        try:
            j.replace('～', '〜').replace('－', '−').replace('—', '―').replace('∥', '‖').encode('euc_jp')
        except UnicodeEncodeError as e:
            print(name, i, 'character not in EUC-JP:', repr(j[e.start:e.end])); errs += 1
    print(name, 'OK' if not errs else '%d problems' % errs, '(%d/%d)' % (len(ja), len(src)))
    return errs


if __name__ == '__main__':
    bad = 0
    for n in sys.argv[1:]: bad += check(n) > 0
    sys.exit(1 if bad else 0)
