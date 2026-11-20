# build/subs.bin: ムービーの日本語字幕（text/subs/en_clean.json の時刻 + text/subs/ja.json の訳）を src/subs.c 用に
#   'DKSB', nvid, {w, h, frames, ncue, off}, cue {start_ms, end_ms, text_off}, UTF-8 文字列（行の区切りは \n）
import os, json, struct
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
MAXC = 36            # 1 行の全角文字数（8 ドット x 36 = 288 / 320）
NOSTART = set('、。，．・ー〜！？」』）ぁぃぅぇぉっゃゅょァィゥェォッャュョ…')
NOEND = set('「『（')


def wrap(s):
    s = s.strip()
    if len(s) <= MAXC: return s
    best = None
    kata = lambda c: '゠' <= c <= 'ヿ'
    for i in range(1, len(s)):
        if s[i] in NOSTART or s[i - 1] in NOEND: continue
        if i > MAXC or len(s) - i > MAXC: continue
        if kata(s[i - 1]) and kata(s[i]): continue
        score = abs(len(s) / 2 - i) - (8 if s[i - 1] in '、。' else 3 if s[i - 1] in 'のをにはがともでへ' else 0)
        if best is None or score < best[0]: best = (score, i)
    if not best: raise SystemExit('cannot wrap: ' + s)
    return s[:best[1]] + '\n' + s[best[1]:]


def load():
    en = json.load(open(os.path.join(ROOT, 'text', 'subs', 'en_clean.json'), encoding='utf-8'))
    ja = json.load(open(os.path.join(ROOT, 'text', 'subs', 'ja.json'), encoding='utf-8'))
    p = os.path.join(ROOT, 'text', 'subs', 'fixes.json')
    fx = json.load(open(p, encoding='utf-8')) if os.path.exists(p) else {}
    vids = []
    for name, v in en.items():
        t = list(ja[name])
        for k, s in fx.get(name, {}).items(): t[int(k)] = s
        if len(t) != len(v['cues']): raise SystemExit('%s: %d cues, %d ja' % (name, len(v['cues']), len(t)))
        cues = []
        for k, ((s, e, _), j) in enumerate(zip(v['cues'], t)):
            st = max(0.0, s - 0.1)
            en_ = max(e + 0.5, s + 1.8)
            if k + 1 < len(v['cues']): en_ = min(en_, v['cues'][k + 1][0] - 0.1)
            cues.append((int(st * 1000), int(en_ * 1000), wrap(j)))
        vids.append((name, v['w'], v['h'], v['frames'], cues))
    return vids


def build():
    vids = load()
    # 同じ大きさ・コマ数のムービーは字幕も同じ（R3IDD/E/H）なので 1 つにまとめる
    seen, uniq = set(), []
    for v in vids:
        k = v[1:4]
        if k in seen: continue
        seen.add(k); uniq.append(v)
    n = len(uniq)
    base = 8 + 20 * n
    ntot = sum(len(v[4]) for v in uniq)
    off_str = base + 12 * ntot
    head, tabs, strs = b'', b'', b''
    for name, w, h, fr, cues in uniq:
        head += struct.pack('<5I', w, h, fr, len(cues), base + len(tabs))
        for s, e, t in cues:
            tabs += struct.pack('<3I', s, e, off_str + len(strs))
            strs += t.encode('utf-8') + b'\0'
    return b'DKSB' + struct.pack('<I', n) + head + tabs + strs, ''.join(c[2] for v in uniq for c in v[4])


if __name__ == '__main__':
    b, chars = build()
    open(os.path.join(ROOT, 'build', 'subs.bin'), 'wb').write(b)
    print('subs.bin', len(b))
