# 訳した塊の確認: python -X utf8 tools/check_chunk.py NNN
import sys, os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
FMT = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?l?[sdcxuif]')
ALLOWED_ASCII = {'GP', 'HP', 'AC', 'OK', 'THAC0', 'THACO', 'MAX', 'MIN', 'NPC', 'LBS'}
FULL_KEEP = '！？（）：、。・ー'


def jbytes(t):
    n = 0
    for ch in t:
        o = ord(ch)
        if o < 0x80: n += 1
        elif 0xff01 <= o <= 0xff5e and ch not in FULL_KEEP: n += 1
        elif ch == '　': n += 1
        else: n += 2
    return n


def fmts(t):
    return [('%s' if f == '%.0s' else f) for f in FMT.findall(t)]


def check(en, ja):
    errs = []
    ids = [it['id'] for it in en['items']]
    for it in en['items']:
        i, e, kind = it['id'], it['en'], it['kind']
        if i not in ja: errs.append('%s: 訳がない' % i); continue
        t = ja[i]
        if not isinstance(t, str) or t == '':
            errs.append('%s: 空の訳（英語のまま = "="、何も出さない = "-"）' % i); continue
        if t == '=': continue
        if t == '-':
            if kind != 'ui': errs.append('%s: "-"（空）は ui の断片にだけ使える' % i)
            continue
        if '\n' in t or '\r' in t: errs.append('%s: 改行が入っている' % i)
        mx = it.get('max')
        tt = t
        if mx is not None and jbytes(tt) > mx:
            errs.append('%s: 長すぎる（%d バイト > %d バイト）: %s' % (i, jbytes(tt), mx, t))
        fe, fj = [f.replace('%.0s', '%s') for f in FMT.findall(e)], fmts(t)
        if fe != fj: errs.append('%s: 書式指定が原文と違う（原文 %s、訳 %s。同じものを同じ順に）' % (i, ' '.join(fe), ' '.join(fj)))
        pe, pj = sorted(re.findall(r'~[A-Z0-9]+~', e)), sorted(re.findall(r'~[A-Z0-9]+~', t))
        if pe != pj: errs.append('%s: ~…~ の置き換え記号が原文と違う（原文 %s、訳 %s）' % (i, ' '.join(pe), ' '.join(pj)))
        if '^' in e:
            k = e[e.index('^') + 1]
            if not t.startswith('^' + k): errs.append('%s: 近道キー: 先頭を「^%s」にする（原文 %s）' % (i, k, e))
            if t.count('^') != 1: errs.append('%s: ^ は 1 つだけ' % i)
        if kind == 'ui' and e.strip():
            if e[0] == ' ' and t[0] != ' ': errs.append('%s: 原文は空白で始まる（訳も空白で始める）' % i)
            if e[-1] == ' ' and t[-1] != ' ' and not e.rstrip().endswith(('?', ':')) and len(e) - len(e.rstrip()) == 1:
                errs.append('%s: 原文は空白で終わる（訳も空白で終える）' % i)
        body = FMT.sub('', t)
        body = re.sub(r'~[A-Z0-9]+~', '', body)
        if '^' in body: body = body.replace(body[body.index('^'):body.index('^') + 2], '')
        for w in re.findall(r'[A-Za-z][A-Za-z\'.-]*[A-Za-z0-9]|[A-Za-z]', body):
            if w.upper() in ALLOWED_ASCII: continue
            if re.search(r'(?<![A-Za-z])' + re.escape(w) + r'(?![A-Za-z])', e, re.I): continue
            errs.append('%s: 英字が残っている（固有名詞以外は日本語に）: %s' % (i, w)); break
        if kind == 'long' or (mx is None or mx > 14):
            for g, j in en['glossary'].items():
                if re.search(r'(?<![A-Za-z])' + re.escape(g) + r'(?![A-Za-z])', e, re.I) and j not in t and g.upper() not in t.upper():
                    if any(len(g2) > len(g) and g.upper() in g2.upper() and re.search(re.escape(g2), e, re.I) for g2 in en['glossary']):
                        continue
                    errs.append('%s: 用語 %s → %s が使われていない' % (i, g, j))
    for k in ja:
        if k not in ids: errs.append('%s: 余分な id' % k)
    return errs


def main():
    n = int(sys.argv[1])
    en = json.load(open(os.path.join(ROOT, 'text', 'chunks', 'c%03d.en.json' % n), encoding='utf-8'))
    p = os.path.join(ROOT, 'text', 'chunks', 'c%03d.ja.json' % n)
    if not os.path.exists(p): print('NG: %s がない' % p); sys.exit(1)
    try:
        ja = json.load(open(p, encoding='utf-8'))
    except Exception as e:
        print('NG: JSON として読めない:', e); sys.exit(1)
    errs = check(en, ja)
    if errs:
        print('NG')
        for x in errs: print(' ', x)
        sys.exit(1)
    print('OK', len(en['items']))


if __name__ == '__main__':
    main()
