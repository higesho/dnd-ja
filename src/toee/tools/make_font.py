# Noto Sans JP (Medium) / Noto Serif JP (SemiBold), SIL OFL -> static instances subset to build/chars.txt
#   build/sans.ttf (TJP/3, family "TJP Sans")   build/serif.ttf (TJP/4, family "TJP Serif")
# Without build/chars.txt every JIS X 0208 character is kept (development builds).
import os
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
B = os.path.join(ROOT, 'build')
p = os.path.join(B, 'chars.txt')
if os.path.exists(p):
    chars = set(open(p, encoding='utf-8').read())
else:
    chars = set()
    for hi in range(0xa1, 0xff):
        for lo in range(0xa1, 0xff):
            try: chars.add(bytes([hi, lo]).decode('euc_jp'))
            except UnicodeDecodeError: pass
chars |= set(chr(c) for c in range(0x20, 0x7f)) | set('…・―‥～－')
for src, wght, fam, out in ((r'C:\Windows\Fonts\NotoSansJP-VF.ttf', 500, 'TJP Sans', 'sans.ttf'),
                            (r'C:\Windows\Fonts\NotoSerifJP-VF.ttf', 600, 'TJP Serif', 'serif.ttf')):
    f = instancer.instantiateVariableFont(TTFont(src), {'wght': wght})
    opt = subset.Options()
    opt.name_IDs = ['*']; opt.name_languages = ['*']; opt.notdef_outline = True; opt.hinting = False
    opt.layout_features = ['palt', 'kern']; opt.drop_tables += ['DSIG']
    s = subset.Subsetter(opt)
    s.populate(unicodes=[ord(c) for c in chars])
    s.subset(f)
    for rec in f['name'].names:
        if rec.nameID in (1, 4, 16): rec.string = fam
        elif rec.nameID == 6: rec.string = fam.replace(' ', '')
    f.save(os.path.join(B, out))
    print(out, os.path.getsize(os.path.join(B, out)), 'bytes', len(chars), 'chars')
