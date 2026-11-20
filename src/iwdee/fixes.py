# Mechanical fixes applied to every Japanese string of the TLK.
import re,unicodedata
CJK=re.compile(r'[　-ヿ一-鿿！-ﾟ]')
HWK=re.compile(r'[｡-ﾟ]+')
NBSP=' '
def fix_halfwidth(s):
    # half-width katakana (and its middle dots) -> full-width
    return HWK.sub(lambda m: unicodedata.normalize('NFKC',m.group(0)).replace('･','・'),s)
def fix_spaces(s):
    """The engine's word-wrap switches to space-delimited mode as soon as a string has an ASCII space,
    and then loses text when a space-delimited chunk containing a newline is wider than the box.
    Japanese strings therefore must not contain ASCII spaces: replace them with no-break spaces."""
    if ' ' not in s or not CJK.search(s): return s
    return s.replace(' ',NBSP)
LATINSP=re.compile(r'(?<=[A-Za-z0-9.,:&\'")\]])　(?=[A-Za-z0-9("\'\[&])')
def fix_all(s):
    if not CJK.search(s): return s
    s=fix_halfwidth(s)
    s=fix_spaces(s)
    s=LATINSP.sub(NBSP,s)   # full-width space typed between Latin words (credits etc.)
    return s
