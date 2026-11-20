# SSI GFF（GFFI）ファイルの読み書き（Dark Sun）
#   python -X utf8 tools/gff.py FILE            種類ごとの件数
#   python -X utf8 tools/gff.py FILE TYPE [ID]  中身（16 進）
import struct, sys

class GFF:
    def __init__(self, path):
        self.path = path
        self.d = open(path, 'rb').read()
        d = self.d
        ident, ver, dloc, tloc, tlen, flags = struct.unpack_from('<4sIIIII', d, 0)
        assert ident == b'GFFI', path
        self.tloc, self.tlen = tloc, tlen
        typ_off, free_off = struct.unpack_from('<II', d, tloc)
        p = tloc + typ_off
        n = struct.unpack_from('<H', d, p)[0]; p += 2
        self.types = {}          # tag -> {id: (loc, len)}
        self.order = []
        for _ in range(n):
            tag = d[p:p + 4].decode('latin1'); p += 4
            cnt = struct.unpack_from('<I', d, p)[0]; p += 4
            ents = {}
            if cnt & 0x80000000:     # 区切り付き（segmented）: 位置の表は GFFI の別チャンクにある
                cnt &= 0x7fffffff
                total, gffi_id, nseg = struct.unpack_from('<III', d, p); p += 12
                segs = []
                for _ in range(nseg):
                    first, num = struct.unpack_from('<II', d, p); p += 8
                    segs.append((first, num))
                ids = [f + k for f, num in segs for k in range(num)]
                self.seg_ref = getattr(self, 'seg_ref', {})
                self.seg_ref[tag] = (gffi_id, ids)
                ents = None
            else:
                for _ in range(cnt):
                    i, loc, ln = struct.unpack_from('<III', d, p); p += 12
                    ents[i] = (loc, ln)
            self.types[tag] = ents
            self.order.append(tag)
        for tag, (gid, ids) in getattr(self, 'seg_ref', {}).items():
            loc, ln = self.types['GFFI'][gid]
            tab = d[loc:loc + ln]
            off = len(tab) - 8 * len(ids)          # 先頭に数などが付くことがある
            ents = {}
            for k, i in enumerate(ids):
                ents[i] = struct.unpack_from('<II', tab, off + 8 * k)
            self.types[tag] = ents
            self.seg_head = getattr(self, 'seg_head', {}); self.seg_head[tag] = tab[:off]

    def get(self, tag, i):
        loc, ln = self.types[tag][i]
        return self.d[loc:loc + ln]

if __name__ == '__main__':
    g = GFF(sys.argv[1])
    if len(sys.argv) == 2:
        for t in g.order:
            e = g.types[t]
            print(repr(t), len(e), 'bytes', sum(l for _, l in e.values()))
    else:
        t = sys.argv[2]
        ids = [int(sys.argv[3])] if len(sys.argv) > 3 else list(g.types[t])[:3]
        for i in ids:
            b = g.get(t, i)
            print(t, i, len(b)); print(b[:400])
