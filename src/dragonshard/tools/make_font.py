# Noto Serif JP / Noto Sans JP (SIL OFL) -> static instances subset to build/chars.txt
#   build/serif.ttf (DSJP/3, "DSJP Serif", wght 600)   build/sans.ttf (DSJP/4, "DSJP Sans", wght 500)
import os
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
B = os.path.join(ROOT, 'build')
chars = open(os.path.join(B, 'chars.txt'), encoding='utf-8').read()
for src, wght, fam, out in ((r'C:\Windows\Fonts\NotoSerifJP-VF.ttf', 600, 'DSJP Serif', 'serif.ttf'),
                            (r'C:\Windows\Fonts\NotoSansJP-VF.ttf', 500, 'DSJP Sans', 'sans.ttf')):
    f = TTFont(src)
    f = instancer.instantiateVariableFont(f, {'wght': wght})
    opt = subset.Options()
    opt.name_IDs = ['*']; opt.name_languages = ['*']; opt.notdef_outline = True; opt.hinting = False
    opt.layout_features = []; opt.drop_tables += ['DSIG']
    s = subset.Subsetter(opt)
    s.populate(unicodes=[ord(c) for c in chars] + list(range(0x20, 0x7f)))
    s.subset(f)
    for rec in f['name'].names:
        if rec.nameID in (1, 4, 16):
            rec.string = fam
        elif rec.nameID == 6:
            rec.string = fam.replace(' ', '')
    f.save(os.path.join(B, out))
    print(out, os.path.getsize(os.path.join(B, out)), 'bytes')
