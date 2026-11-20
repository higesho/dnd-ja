# Minimal WeiDU .tra parser: returns {id: [ (text, sound) , ... ]}  (1 entry = male/neutral, 2 = male+female)
import re
def _skip_ws(s, i):
    n = len(s)
    while i < n:
        if s[i].isspace(): i += 1
        elif s.startswith('//', i):
            j = s.find('\n', i); i = n if j < 0 else j + 1
        elif s.startswith('/*', i):
            j = s.find('*/', i + 2); i = n if j < 0 else j + 2
        else: break
    return i

def _read_str(s, i):
    for d in ('~~~~~', '~', '"', '%'):
        if s.startswith(d, i):
            j = s.find(d, i + len(d))
            if j < 0: raise ValueError('unterminated string at %d' % i)
            return s[i + len(d):j], j + len(d)
    return None, i

def parse(text):
    out = {}; i = 0; n = len(text)
    while True:
        i = _skip_ws(text, i)
        if i >= n: break
        m = re.compile(r'@\s*([-A-Za-z0-9_]+)\s*=\s*').match(text, i)
        if not m: raise ValueError('syntax error near: %r' % text[i:i + 60])
        k = m.group(1); i = m.end(); vals = []
        while True:
            i = _skip_ws(text, i)
            sv, j = _read_str(text, i)
            if sv is None: break
            i = j; snd = ''
            k2 = _skip_ws(text, i)
            if k2 < n and text[k2] == '[':
                e = text.find(']', k2); snd = text[k2 + 1:e]; i = e + 1
            vals.append((sv, snd))
        if not vals: raise ValueError('no string for @%d' % k)
        out[k] = vals
    return out

def read(path):
    b = open(path, 'rb').read()
    if b.startswith(b'\xef\xbb\xbf'): b = b[3:]
    try: t = b.decode('utf-8')
    except UnicodeDecodeError: t = b.decode('cp1252')
    return parse(t)
