# Merge text/chunks/cNN.ja.txt + text/replace.json + text/fixes.json -> text/ja/strings.json ({id: ja}) and check them.
import json, glob, os, re
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')

def unesc(s):
    out = []; i = 0
    while i < len(s):
        if s[i] == '\\' and i + 1 < len(s):
            out.append({'n': '\n', '\\': '\\'}.get(s[i + 1], s[i + 1])); i += 2
        else:
            out.append(s[i]); i += 1
    return ''.join(out).replace('<P>', '\x01').replace('<T>', '\x09')

def check(en, ja):
    errs = []
    for ch in '[|]\x01\x09\n':
        if en.count(ch) != ja.count(ch):
            errs.append('count %r %d != %d' % (ch, en.count(ch), ja.count(ch)))
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
    up = os.path.join(ROOT, 'text', 'reuse.json')          # identical strings already translated for Menzoberranzan
    if os.path.exists(up):
        tr.update({int(k): v for k, v in json.load(open(up, encoding='utf-8')).items()})
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.txt'))):
        for line in open(f, encoding='utf-8-sig'):
            m = re.match(r'^\[(\d+)\] ?(.*?)\r?$', line.rstrip('\n'))
            if m:
                tr[int(m.group(1))] = unesc(m.group(2))
    rp = os.path.join(ROOT, 'text', 'replace.json')
    if os.path.exists(rp):
        for old, new in json.load(open(rp, encoding='utf-8')):
            for i in tr:
                tr[i] = tr[i].replace(old, new)
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(fx):
        for k, v in json.load(open(fx, encoding='utf-8')).items():
            tr[int(k)] = v
    report = []
    for i, x in sorted(en.items()):
        if i in tr:
            e = check(x['en'], tr[i])
            if e:
                report.append('[%d] %s | EN: %r | JA: %r' % (i, '; '.join(e), x['en'][:200], tr[i][:200]))
    missing = [i for i, x in en.items() if i not in tr and x['en'].strip() and x['en'].strip().lower() != 'empty']
    os.makedirs(os.path.join(ROOT, 'text', 'ja'), exist_ok=True)
    json.dump({str(k): v for k, v in sorted(tr.items())}, open(os.path.join(ROOT, 'text', 'ja', 'strings.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    open(os.path.join(ROOT, 'text', 'merge_report.txt'), 'w', encoding='utf-8').write('\n'.join(report))
    print('translated %d / %d, untranslated %d, issues %d' % (len(tr), len(en), len(missing), len(report)))

if __name__ == '__main__':
    main()
