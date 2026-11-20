# 一度きりの書き換え: オープニング（INTRO.BMA / INTRO.ACF）の字幕を差し替える 'G' 節の処理を DLL に足す
import sys
root = sys.argv[1]
p = root + '/src/pr_jp.c'
s = open(p, encoding='utf-8').read()
code = r'''
/* ------------------------------------------------ オープニングの字幕（VACE の INTRO.BMA / INTRO.ACF）
 * 'G' 節（名前 "INTRO.BMA"）: u32 BMA の大きさ, BMA の CRC, ACF の大きさ, ACF の CRC, y0, y1, n,
 *   n * {u32 フレーム番号, u8 文字の色, u8 背景の色, u16 0, 1bpp の字形 (y1-y0+1)*40 バイト}
 * 指定のフレームの y0〜y1 行を、英語の字の色を背景色で消して日本語の字を重ねた行に作り直す。
 * フレームの大きさが変わるので、ブロックの中の位置表・ブロックの大きさ・ACF の「08 05 番号 枚数 大きさ」を直す */
static uint32_t rle_row(const uint8_t *seg, int n, uint8_t *out)
{
    int i = 0;
    uint32_t o = 0;
    while (i < n) {
        int r = 1;
        while (i + r < n && seg[i + r] == seg[i] && r < 128) r++;
        if (r >= 3) { out[o++] = (uint8_t)(((r - 1) << 1) | 1); out[o++] = seg[i]; i += r; continue; }
        r = 1;
        while (i + r < n && r < 128 && !(i + r + 2 < n && seg[i + r] == seg[i + r + 1] && seg[i + r] == seg[i + r + 2])) r++;
        out[o++] = (uint8_t)((r - 1) << 1);
        memcpy(out + o, seg + i, r); o += r; i += r;
    }
    return o;
}

/* 1 行を 320 字の並びに展開（無い所はそのまま）。p は行の先頭（y の次）。戻り値は次の行の先頭 */
static uint32_t row_decode(const uint8_t *f, uint32_t flen, uint32_t p, uint8_t *line)
{
    for (;;) {
        uint16_t xw; int width, dl, x, k = 0;
        uint32_t q, end;
        if (p + 4 > flen) return flen;
        xw = (uint16_t)(f[p] | (f[p + 1] << 8)); width = f[p + 2] ? f[p + 2] : 256; dl = f[p + 3]; p += 4;
        x = xw & 0x7fff; q = p; end = p + dl;
        while (q < end && q < flen) {
            uint8_t b = f[q++]; int c = (b >> 1) + 1, j;
            for (j = 0; j < c; j++) {
                uint8_t v = (b & 1) ? f[q] : f[q + j];
                if (k < width && x + k < 320 && line) line[x + k] = v;
                k++;
            }
            q += (b & 1) ? 1 : c;
        }
        p = end;
        if (xw & 0x8000) return p;
    }
}

static uint32_t splice_frame(const uint8_t *f, uint32_t flen, uint8_t *out, int y0, int y1, uint8_t fg, uint8_t bg, const uint8_t *mask)
{
    uint32_t p = 4, o = 4, rs[256], re[256];
    int have[256] = {0}, y, done = 0;
    memcpy(out, f, 4);
    while (p < flen && f[p] != 0xff) {       /* 行の位置を集める */
        int yy = f[p];
        rs[yy] = p; re[yy] = row_decode(f, flen, p + 1, NULL); have[yy] = 1;
        p = re[yy];
    }
    for (y = 0; y < 256; y++) {
        if (y >= y0 && !done) {
            int yy;
            for (yy = y0; yy <= y1; yy++) {
                uint8_t line[320];
                int x, h;
                memset(line, bg, 320);
                if (have[yy]) row_decode(f, flen, rs[yy] + 1, line);
                for (x = 0; x < 320; x++) {
                    if (line[x] == fg) line[x] = bg;
                    if (mask[(yy - y0) * 40 + (x >> 3)] & (0x80 >> (x & 7))) line[x] = fg;
                }
                out[o++] = (uint8_t)yy;
                for (h = 0; h < 2; h++) {
                    uint32_t at = o, n;
                    o += 4;
                    n = rle_row(line + h * 160, 160, out + o);
                    out[at] = (uint8_t)(h ? 160 : 0); out[at + 1] = (uint8_t)(h ? 0x80 : 0); out[at + 2] = 160; out[at + 3] = (uint8_t)n;
                    o += n;
                }
            }
            done = 1;
        }
        if (have[y] && (y < y0 || y > y1)) { memcpy(out + o, f + rs[y], re[y] - rs[y]); o += re[y] - rs[y]; }
    }
    out[o++] = 0xff;
    return o;
}

static int build_cine(const char *orig_path, char *bma_out, char *acf_out)
{
    uint32_t slen, blen, alen, b = 0, fi = 0, nblk = 0, sizes[64];
    const uint8_t *sec = find_sect('G', "INTRO.BMA", &slen);
    const uint32_t *h;
    char dir[MAX_PATH], pb[MAX_PATH], pa[MAX_PATH];
    const char *bn = base_name(orig_path);
    uint8_t *bma, *acf, *out, *tmp;
    uint32_t o = 0, i;
    int ok = 0;
    if (!sec) return 0;
    h = (const uint32_t*)sec;
    lstrcpynA(dir, orig_path, (int)(bn - orig_path) + 1);
    wsprintfA(pb, "%sINTRO.BMA", dir); wsprintfA(pa, "%sINTRO.ACF", dir);
    bma = read_all(pb, &blen); acf = read_all(pa, &alen);
    if (!bma || !acf || blen != h[0] || crc32(bma, blen) != h[1] || alen != h[2] || crc32(acf, alen) != h[3]) {
        dbg("cine: original mismatch\n"); free(bma); free(acf); return 0;
    }
    out = (uint8_t*)malloc(blen + 65536 * 4); tmp = (uint8_t*)malloc(70000);
    while (b + 6 <= blen && nblk < 64) {
        uint32_t ln = *(const uint32_t*)(bma + b), n = *(const uint16_t*)(bma + b + 4), k, pos, bo = o;
        if (!ln) break;
        pos = 6 + 4 * n;
        o += pos;
        for (k = 0; k < n; k++, fi++) {
            uint32_t fo = *(const uint32_t*)(bma + b + 6 + 4 * k);
            uint32_t fe = k + 1 < n ? *(const uint32_t*)(bma + b + 6 + 4 * (k + 1)) : ln;
            const uint8_t *r = (const uint8_t*)(h + 7);
            uint32_t m = 0, rlen = 8 + (h[5] - h[4] + 1) * 40, nl = fe - fo;
            for (m = 0; m < h[6]; m++, r += rlen)
                if (*(const uint32_t*)r == fi) break;
            *(uint32_t*)(out + bo + 6 + 4 * k) = o - bo;
            if (m < h[6]) {
                nl = splice_frame(bma + b + fo, fe - fo, tmp, (int)h[4], (int)h[5], r[4], r[5], r + 8);
                memcpy(out + o, tmp, nl);
            } else memcpy(out + o, bma + b + fo, nl);
            o += nl;
        }
        *(uint32_t*)(out + bo) = o - bo;
        *(uint16_t*)(out + bo + 4) = (uint16_t)n;
        sizes[nblk++] = o - bo;
        b += ln;
    }
    memcpy(out + o, bma + b, blen - b); o += blen - b;
    {   /* ACF: 08 05 番号 枚数 大きさ */
        uint32_t nb = 0;
        for (i = 0; i + 8 <= alen && nb < nblk; i++) {
            if (acf[i] == 8 && acf[i + 1] == 5 && *(const uint16_t*)(acf + i + 2) == nb) {
                if (sizes[nb] > 0xffff) break;
                *(uint16_t*)(acf + i + 6) = (uint16_t)sizes[nb];
                nb++; i += 7;
            }
        }
        ok = nb == nblk;
    }
    if (ok) ok = write_all(bma_out, out, o) && write_all(acf_out, acf, alen);
    dbg("cine: %s (%u blocks, %u -> %u bytes)\n", ok ? "built" : "FAILED", nblk, blen, o);
    free(out); free(tmp); free(bma); free(acf);
    return ok;
}

static const char *cine_path(const char *orig_path, const char *bn, char *buf)
{
    char pb[MAX_PATH], pa[MAX_PATH];
    uint32_t slen;
    if (!find_sect('G', "INTRO.BMA", &slen)) return NULL;
    wsprintfA(pb, "%s\\%08X_INTRO.BMA", g_dir, g_blob_crc);
    wsprintfA(pa, "%s\\%08X_INTRO.ACF", g_dir, g_blob_crc);
    if (GetFileAttributesA(pb) == INVALID_FILE_ATTRIBUTES || GetFileAttributesA(pa) == INVALID_FILE_ATTRIBUTES)
        if (!build_cine(orig_path, pb, pa)) return NULL;
    lstrcpyA(buf, _stricmp(bn, "INTRO.BMA") ? pa : pb);
    return buf;
}

'''
anchor = 'static const char *patched_path(const char *orig_path, char *buf)'
assert anchor in s and 'build_cine' not in s
s = s.replace(anchor, code + anchor, 1)
old = """    if (!g_enabled) return NULL;
    se = find_sect('E', bn, &slen);"""
new = """    if (!g_enabled) return NULL;
    if (!_stricmp(bn, "INTRO.BMA") || !_stricmp(bn, "INTRO.ACF")) return cine_path(orig_path, bn, buf);
    se = find_sect('E', bn, &slen);"""
assert old in s
s = s.replace(old, new, 1)
open(p, 'w', encoding='utf-8').write(s)
print('ok')
