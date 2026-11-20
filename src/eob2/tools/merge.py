# 訳を統合して text/ja/all.json（{英語原文: 訳文}）を作り、検査結果を text/merge_report.txt に書く
#   python -X utf8 tools/merge.py
#   手直しは text/fixes.json に {"英語本文（翻訳者向け表記）": "訳"} で追加する
import os, json, glob, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from textfmt import split_edges, to_tok, from_tok, tokens_of, specs_of

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
CH = os.path.join(ROOT, 'text', 'chunks')
CR, LF = chr(13), chr(10)

JP = r'[぀-ヿ㐀-鿿！-｠、。「」『』（）・…ー〜]'

# 職業名はシリーズ（EOB1・EOB3）とそろえて漢字にする
CLASS_JA = [('ファイター', '戦士'), ('パラディン', '聖騎士'), ('メイジ', '魔術師'), ('クレリック', '僧侶'), ('シーフ', '盗賊')]

def normalize(t):
    for a, b in CLASS_JA:
        t = t.replace(a, b)
    t = re.sub(r'(?<=[士ー師侶賊])／(?=[戦レ聖魔僧盗])', '/', t)   # 複合職は半角スラッシュ（EOB1・EOB3 と同じ）
    t = t.replace('...', '…').replace('　', '  ').replace('。」', '」').replace('。』', '』')
    t = re.sub(r'(?<=%s) +(?=%s)' % (JP, JP), '', t)
    t = re.sub(r'(?<=%s) +(?=[0-9A-Za-z%%])' % JP, '', t)
    t = re.sub(r'(?<=[0-9A-Za-z.!?]) +(?=%s)' % JP, '', t)
    return t

NO_START = set('、。，．・：；？！ー…‥」』）］｝〕〉》】ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々〜～!?.,)]:;')
NO_END = set('「『（［｛〔〈《【([')
UNIT = re.compile(r"%[-+ #0-9]*l?[dsuxic]|[A-Za-z0-9+'.-]+|\{[^}]*\}|.", re.S)

def uw(u):
    """表示幅（半角単位）"""
    if u.startswith('%'):
        return 8 if u[-1] == 's' else 3
    if u.startswith('{'):
        return 0
    return sum(1 if ord(c) < 0x80 else 2 for c in u)

def wrap(t, width):
    """幅 width（半角単位）で折り返す（禁則つき）"""
    lines, cur, w = [], [], 0
    for u in UNIT.findall(t):
        if u == ' ' and not cur:
            continue
        if w + uw(u) > width and cur:
            if u in NO_START and w + uw(u) <= width + 2:   # 句読点は 1 字まではみ出してよい
                cur.append(u); w += uw(u)
                continue
            carry = []
            while cur and cur[-1] in NO_END:
                carry.insert(0, cur.pop())
            lines.append(''.join(cur).rstrip())
            cur, w = carry, sum(uw(x) for x in carry)
            if u == ' ':
                continue
        cur.append(u); w += uw(u)
    if cur:
        lines.append(''.join(cur).rstrip())
    return lines

def relayout(core_en, t):
    """英語が手で改行された狭い枠（Print で描く説明文など）なら、訳も同じ幅で改行し直す"""
    el = core_en.split(CR)
    width = max(len(x) for x in el)
    if len(el) < 2 or width > 40 or not re.search(JP, t):
        return t
    width = width * 3 // 2             # 原作は 6 ドット文字、日本語化後は半角 4 ドット単位なので同じ幅は 1.5 倍
    if all(sum(uw(u) for u in UNIT.findall(ln)) <= width for ln in t.split(LF)):
        return t                       # 訳者の改行のままで枠に収まる
    indent = all(x.startswith(' ') for x in el[1:] if x)   # 1 行目の字下げは外枠（pre）側にある
    paras = re.split(LF + r'\s*' + LF, t) if CR + CR in core_en else [t]
    out = []
    for k, ptxt in enumerate(paras):
        if k:
            out.append('')
        w0 = width - (1 if indent else 0)
        lines = wrap(ptxt.replace(LF, ''), w0)
        for w in range(w0 - 1, max(4, w0 - 8), -1):      # 最終行が 1〜2 字だけにならないよう幅を少し詰める
            if len(lines) < 2 or sum(uw(u) for u in UNIT.findall(lines[-1])) > 4:
                break
            alt = wrap(ptxt.replace(LF, ''), w)
            if len(alt) == len(lines):
                lines = alt
        for ln in lines:
            out.append((' ' if indent and out else '') + ln)
    return LF.join(out)

def main():
    index = json.load(open(os.path.join(CH, 'index.json'), encoding='utf-8'))
    ja = {}
    for f in sorted(glob.glob(os.path.join(CH, 'c*.ja.json'))):
        ja.update(json.load(open(f, encoding='utf-8')))
    rv = os.path.join(ROOT, 'text', 'review_fixes.json')      # 見直しでの直し（塊の id ごと）
    if os.path.exists(rv):
        ja.update(json.load(open(rv, encoding='utf-8')))
    fixes_p = os.path.join(ROOT, 'text', 'fixes.json')
    fixes = json.load(open(fixes_p, encoding='utf-8')) if os.path.exists(fixes_p) else {}
    out, report, missing, cores = {}, [], 0, {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'en', '*.json'))):
        for s in json.load(open(f, encoding='utf-8'))['strings']:
            en = s['en']
            pre, core, suf = split_edges(en)
            if not core.strip():
                continue
            tok = to_tok(core)
            t = fixes.get(tok)
            if t is None:
                cid = index.get(core)
                t = ja.get(cid) if cid else None
            if t is None:
                missing += 1
                continue
            if t == '':
                continue            # 訳さない（表示されない文字列など）
            t = normalize(t)
            t = relayout(core, t)
            if tokens_of(t) != tokens_of(tok):
                report.append('制御記号が違う %s\n  EN %r\n  JA %r' % (s['id'], tok, t))
            se, sj = specs_of(tok), specs_of(t)
            numbered = re.search(r'%[1-9]\$', t)
            if not numbered and [x[-1] for x in se] != [x[-1] for x in sj]:
                report.append('書式が違う %s\n  EN %r\n  JA %r' % (s['id'], tok, t))
            if re.match(r' {3,}', pre):
                pre = '  ' + pre.lstrip(' ')
            out[en] = pre + from_tok(t) + suf
            if core != en and core not in out:
                cores[core] = from_tok(t)        # 前後を外した本文だけでも引けるように
    for c, t in cores.items():
        out.setdefault(c, t)
    os.makedirs(os.path.join(ROOT, 'text', 'ja'), exist_ok=True)
    json.dump(out, open(os.path.join(ROOT, 'text', 'ja', 'all.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    open(os.path.join(ROOT, 'text', 'merge_report.txt'), 'w', encoding='utf-8').write(
        '訳あり %d / 未訳 %d\n\n' % (len(out), missing) + '\n'.join(report) + '\n')
    print('訳あり', len(out), '未訳', missing, '指摘', len(report))

if __name__ == '__main__':
    main()
