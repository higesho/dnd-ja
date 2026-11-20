# list frames that carry subtitle text (lower area y>=120) and render them
import sys, os, json
sys.path.insert(0, os.path.dirname(__file__)); import gff
from PIL import Image
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
res = []
for f in ['INTRO', 'DARK', 'LICH', 'FINALE']:
    d = open(os.path.join(gff.GAME, f + '.GFF'), 'rb').read(); t = gff.toc(d)
    P = gff.pal(d, t, 1) if 'PAL ' in t else None
    for tag in ['BMP ', 'BMA ']:
        for i, o, l in t.get(tag, []):
            ch = d[o:o + l]
            for k, fo in enumerate(gff.frames(ch)):
                w, h, px = gff.decode_frame(ch, fo)
                lo = [px[y * w + x] for y in range(120, h) for x in range(w)]
                cols = set(v for v in lo if v is not None)
                if 11 in cols or (len(cols) > 1):
                    res.append((f, tag.strip(), i, k, sorted(cols)))
                    im = Image.new('L', (w, 80))
                    im.putdata([255 if v == 11 else (0 if v is None else 60) for v in lo])
                    im.save(os.path.join(ROOT, 'build', 'gff', '%s_%s%d_%d.png' % (f, tag.strip(), i, k)))
for r in res: print(r)
