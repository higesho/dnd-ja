# Merge text/chunks/cNN.ja.txt + text/fixes.json -> text/ja/strings.json ({id: ja}) and check them.
import json, glob, os, re
ROOT = os.path.join(os.path.dirname(__file__), '..')
FMT = re.compile(r'%%|%[-+ #0-9.]*l?[a-zA-Z]')

def unesc(s):
    out = []; i = 0
    while i < len(s):
        if s[i] == '\\' and i + 1 < len(s):
            out.append({'n': '\n', '\\': '\\'}.get(s[i + 1], s[i + 1])); i += 2
        else:
            out.append(s[i]); i += 1
    return ''.join(out)

def check(en, ja):
    errs = []
    if FMT.findall(en) != FMT.findall(ja):
        errs.append('format %s != %s' % (FMT.findall(en), FMT.findall(ja)))
    if en.count('\n') != ja.count('\n'):
        errs.append('newlines %d != %d' % (en.count('\n'), ja.count('\n')))
    if en.strip() and not ja.strip():
        errs.append('empty')
    if '"' in ja:
        errs.append('double quote')
    if re.search(r'[～０-９Ａ-Ｚａ-ｚ｡-ﾟ]', ja):
        errs.append('bad char')
    return errs

def main():
    en = {x['id']: x for x in json.load(open(os.path.join(ROOT, 'text', 'en', 'strings.json'), encoding='utf-8'))}
    tr = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.txt'))):
        for line in open(f, encoding='utf-8-sig'):
            m = re.match(r'^\[(\d+)\] ?(.*?)\r?$', line.rstrip('\n'))
            if m:
                tr[int(m.group(1))] = unesc(m.group(2))
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(fx):
        for k, v in json.load(open(fx, encoding='utf-8')).items():
            tr[int(k)] = v
    report = []
    for i, x in sorted(en.items()):
        if i in tr:
            e = check(x['en'], tr[i])
            if e:
                report.append('[%d] %s | EN: %r | JA: %r' % (i, '; '.join(e), x['en'], tr[i]))
    missing = [i for i, x in en.items() if i not in tr and x['en'].strip()]
    os.makedirs(os.path.join(ROOT, 'text', 'ja'), exist_ok=True)
    json.dump({str(k): v for k, v in sorted(tr.items())}, open(os.path.join(ROOT, 'text', 'ja', 'strings.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    open(os.path.join(ROOT, 'text', 'merge_report.txt'), 'w', encoding='utf-8').write('\n'.join(report))
    print('translated %d / %d, untranslated %d, issues %d' % (len(tr), len(en), len(missing), len(report)))

if __name__ == '__main__':
    main()
