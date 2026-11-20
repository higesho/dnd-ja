# list capitalised words / phrases of text/en/strings.json (glossary candidates)
import json, re, collections, os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
d = json.load(open(os.path.join(ROOT, 'text', 'en', 'strings.json'), encoding='utf8'))
c = collections.Counter()
for x in d:
    s = x['en']
    if s.isupper():
        continue
    for m in re.finditer(r"([A-Z][a-z'\-]+(?: (?:of |de |del |the )?[A-Z][a-z'\-]+)*)", s):
        w = m.group(1)
        st = m.start()
        before = s[max(0, st - 3):st]
        if st == 0 or re.search(r'[.!?"\x01] *$', before) and w.count(' ') == 0:
            # sentence start: count only multi-word or obviously proper
            c['?' + w] += 1
        else:
            c[w] += 1
print(len(d), sum(len(x['en']) for x in d))
words = [(k, v) for k, v in c.most_common() if not k.startswith('?')]
print(', '.join('%s(%d)' % kv for kv in words[:400]))
