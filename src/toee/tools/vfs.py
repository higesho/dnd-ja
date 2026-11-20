# The game's virtual file system as TPlite builds it (later entries win), for reading text files.
#   vfs.files()  -> {path: (source, entry)}   vfs.read(path) -> bytes
import os, sys, json
sys.path.insert(0, os.path.dirname(__file__))
import datx

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))
ORDER = ['tig.dat', 'ToEE1.dat', 'ToEE2.dat', 'ToEE3.dat', 'ToEE4.dat', 'tpldata/tplite.dat', 'tpldata/clearances.dat',
         'tpldata/kotbfixes.dat', 'tpldata/palcov.dat', 'tpldata/co8infra.dat', 'tpldata/co8fixes.dat',
         'modules/ToEESteam.dat', 'modules/ToEESteam_core.dat']
_cache = {}
_handles = {}


def files():
    if _cache: return _cache
    for p in ORDER:
        fp = os.path.join(GAME, p)
        if not os.path.exists(fp): continue
        f, ents = datx.read_dat(fp)
        _handles[p] = f
        for e in ents:
            if e[1] & 0x400: continue
            _cache[e[0]] = (p, e)
    # loose directories: tpldata (TPlite data) and data/
    for d in ('tpldata', 'data'):
        base = os.path.join(GAME, d)
        for root, ds, fs in os.walk(base):
            for fn in fs:
                rp = os.path.relpath(os.path.join(root, fn), base).replace(os.sep, '/').lower()
                if d == 'tpldata' and rp.startswith('python-lib/'): continue
                _cache[rp] = ('loose:' + d, os.path.join(root, fn))
    return _cache


def read(path):
    src, e = files()[path]
    if src.startswith('loose'):
        return open(e, 'rb').read()
    return datx.get(_handles[src], e)
