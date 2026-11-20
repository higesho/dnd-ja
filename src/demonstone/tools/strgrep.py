# python tools/strgrep.py FILE REGEX : ASCII and UTF-16 strings matching REGEX (case-insensitive)
import re, sys
d = open(sys.argv[1], 'rb').read()
rx = re.compile(sys.argv[2].encode(), re.I)
seen = set()
for m in re.finditer(rb'[\x20-\x7e]{4,}', d):
    t = m.group()
    if rx.search(t) and t not in seen:
        seen.add(t); print('%08x A %s' % (m.start(), t[:120].decode('latin1')))
for m in re.finditer(rb'(?:[\x20-\x7e]\x00){4,}', d):
    t = m.group().decode('utf-16le').encode()
    if rx.search(t) and t not in seen:
        seen.add(t); print('%08x W %s' % (m.start(), t[:120].decode('latin1')))
