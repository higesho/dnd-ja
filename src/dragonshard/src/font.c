/* Japanese glyphs for the game's LIQDFONT (.lfn) fonts.

   'LIQDFONT' f32 version, u32 nglyph, u32 nsub, then nsub sub-fonts (one per pixel size):
       u32 hdr[7] = { height, char spacing, space width, ?, texW, texH, ntex }
       ntex * texW*texH*4 bytes (A8R8G8B8, white + coverage alpha)
       nglyph * { u16 ch, u16 tex, i32 width, f32 u0, v0, u1, v1 }  (sorted by ch; a glyph cell is width x height)
   We write the textures as A8L8 (2 bytes per pixel) and patch the loader to match (font_init).
   Every sub-font gets bigger texture pages: the original page is copied into the top left of page 0 and
   the Japanese glyphs (rendered with GDI from the bundled Noto fonts) fill the rest. */
#include "dsjp.h"
#include <math.h>

#pragma pack(push, 1)
typedef struct { uint16_t ch, tex; int32_t w; float u0, v0, u1, v1; } Glyph;
#pragma pack(pop)

static HANDLE g_fontres[2];
static const WCHAR *g_face[2] = { L"DSJP Serif", L"DSJP Sans" };
static const int g_weight[2] = { 600, 500 };

static void load_fonts(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        DWORD n, cnt = 0;
        const void *p;
        if (g_fontres[i]) continue;
        p = res_get(3 + i, &n);
        if (p) g_fontres[i] = AddFontMemResourceEx((void *)p, n, NULL, &cnt);
        dlog("font res %d: %p (%u)", i, g_fontres[i], cnt);
    }
}

typedef struct { uint8_t *p; uint32_t n, cap; } Buf;
static void put(Buf *b, const void *d, uint32_t n)
{
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) + (b->n + n) / 2 + (1u << 20);
        b->p = (uint8_t *)realloc(b->p, b->cap);
    }
    if (d) memcpy(b->p + b->n, d, n); else memset(b->p + b->n, 0, n);
    b->n += n;
}

/* baseline of the Latin glyphs in a cell: bottom of 'H' */
static int find_baseline(const uint8_t *tex, int tw, int th, const Glyph *g, int n, int height)
{
    int i, x, y;
    for (i = 0; i < n; i++) if (g[i].ch == 'H') break;
    if (i == n) return height * 4 / 5;
    {
        int x0 = (int)(g[i].u0 * tw + 0.5f), y0 = (int)(g[i].v0 * th + 0.5f), w = g[i].w, last = -1;
        for (y = 0; y < height && y0 + y < th; y++)
            for (x = 0; x < w && x0 + x < tw; x++)
                if (tex[((y0 + y) * tw + x0 + x) * 4 + 3] > 0x80) last = y;
        return last >= 0 ? last + 1 : height * 4 / 5;
    }
}

int font_build(const uint8_t *src, uint32_t n, uint8_t **out, uint32_t *outn)
{
    uint32_t nglyph, nsub, p = 20, s;
    const uint16_t *cs;
    uint16_t *add;
    int ncs, nadd = 0, kind, i;
    Buf b = { 0 };
    HDC dc;
    if (n < 20 || memcmp(src, "LIQDFONT", 8)) return 0;
    nglyph = *(const uint32_t *)(src + 12);
    nsub = *(const uint32_t *)(src + 16);
    cs = text_charset(&ncs);
    if (!cs || !ncs) return 0;
    load_fonts();
    kind = nsub >= 10 ? 0 : 1;            /* "Sample.lfn" (12 sizes, serif) / "Small Thin.lfn" (6 sizes, sans) */

    /* characters to add: those the font lacks */
    {
        const uint8_t *q = src + 20;
        uint32_t tw = ((const uint32_t *)q)[4], th = ((const uint32_t *)q)[5], nt = ((const uint32_t *)q)[6];
        const Glyph *g = (const Glyph *)(q + 28 + tw * th * 4 * nt);
        add = (uint16_t *)malloc(ncs * 2);
        for (i = 0; i < ncs; i++) {
            uint32_t k; int have = 0;
            for (k = 0; k < nglyph; k++) if (g[k].ch == cs[i]) { have = 1; break; }
            if (!have) add[nadd++] = cs[i];
        }
    }
    put(&b, src, 12);
    { uint32_t ng = nglyph + nadd; put(&b, &ng, 4); put(&b, &nsub, 4); }

    dc = CreateCompatibleDC(NULL);
    for (s = 0; s < nsub; s++) {
        uint32_t hdr[7], tw, th, nt, P, perrow, rows0, x, y, page, k, npages, cellw, top0;
        const uint8_t *tex0;
        const Glyph *og;
        Glyph *ng;
        uint8_t **pages;
        int H, em, base, ty, ebase, gi, oi;
        HFONT hf, old;
        TEXTMETRICW tm;
        MAT2 m2 = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
        uint8_t *gbuf = NULL;
        DWORD gcap = 0;

        if (p + 28 > n) goto bad;
        memcpy(hdr, src + p, 28);
        tw = hdr[4]; th = hdr[5]; nt = hdr[6]; H = (int)hdr[0];
        if (nt != 1 || p + 28 + tw * th * 4 + nglyph * sizeof(Glyph) > n) goto bad;
        tex0 = src + p + 28;
        og = (const Glyph *)(tex0 + tw * th * 4);
        p += 28 + tw * th * 4 + nglyph * sizeof(Glyph);

        base = find_baseline(tex0, tw, th, og, nglyph, H);
        em = (int)floor(H * 0.92 + 0.5);
        if (em > H) em = H;
        cellw = em + 1;
        P = H <= 15 ? 512 : H <= 30 ? 1024 : 2048;
        while (P < tw || P < th) P *= 2;
        perrow = P / cellw;
        rows0 = (P - th) / H;                    /* rows left under the original page */
        npages = 1;
        {
            uint32_t cap0 = perrow * rows0, per = perrow * (P / H);
            if ((uint32_t)nadd > cap0) npages += (nadd - cap0 + per - 1) / per;
        }
        pages = (uint8_t **)calloc(npages, sizeof *pages);
        /* pages are A8L8 (2 bytes per pixel, luminance 255): the loader is patched for that (font_init) */
        for (k = 0; k < npages; k++) {
            uint16_t *pg = (uint16_t *)malloc(P * P * 2);
            uint32_t q;
            for (q = 0; q < P * P; q++) pg[q] = 0x00ff;
            pages[k] = (uint8_t *)pg;
        }
        for (y = 0; y < th; y++)
            for (x = 0; x < tw; x++)
                ((uint16_t *)pages[0])[y * P + x] = 0x00ff | (uint16_t)(tex0[(y * tw + x) * 4 + 3] << 8);

        /* GDI font: em-square height em */
        hf = CreateFontW(-em, 0, 0, 0, g_weight[kind], 0, 0, 0, SHIFTJIS_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
                         ANTIALIASED_QUALITY, DEFAULT_PITCH, g_face[kind]);
        old = (HFONT)SelectObject(dc, hf);
        GetTextMetricsW(dc, &tm);
        /* place the em box in the cell so that its baseline sits a little below the Latin baseline */
        ty = (H - em) / 2;
        ebase = ty + tm.tmAscent - tm.tmInternalLeading / 2;
        if (ebase < base) ebase = base;
        if (ebase > H - 1) ebase = H - 1;

        ng = (Glyph *)malloc((nglyph + nadd) * sizeof(Glyph));
        /* merge original (rescaled) + new glyphs, sorted by code */
        x = 0; y = th; page = 0; top0 = th;
        gi = 0; oi = 0;
        for (k = 0; k < nglyph + (uint32_t)nadd; k++) {
            Glyph g;
            if (oi < (int)nglyph && (gi >= nadd || og[oi].ch < add[gi])) {
                g = og[oi++];
                g.u0 = g.u0 * tw / P; g.u1 = g.u1 * tw / P;
                g.v0 = g.v0 * th / P; g.v1 = g.v1 * th / P;
            } else {
                WCHAR c = add[gi++];
                GLYPHMETRICS gm;
                DWORD sz;
                int adv;
                if (x + cellw > P) { x = 0; y += H; }
                if (y + H > P) { page++; x = 0; y = 0; }
                if (page >= npages) page = npages - 1;     /* cannot happen */
                adv = em;
                sz = GetGlyphOutlineW(dc, c, GGO_GRAY8_BITMAP, &gm, 0, NULL, &m2);
                if (sz != GDI_ERROR) {
                    int gx, gy, ox, oy, pitch;
                    if (sz > gcap) { gcap = sz + 4096; gbuf = (uint8_t *)realloc(gbuf, gcap); }
                    if (sz) GetGlyphOutlineW(dc, c, GGO_GRAY8_BITMAP, &gm, sz, gbuf, &m2);
                    adv = gm.gmCellIncX;
                    if (adv > em + 1 || adv < 1) adv = em;
                    pitch = (gm.gmBlackBoxX + 3) & ~3;
                    ox = gm.gmptGlyphOrigin.x;
                    oy = ebase - gm.gmptGlyphOrigin.y;
                    if (sz)
                        for (gy = 0; gy < (int)gm.gmBlackBoxY; gy++)
                            for (gx = 0; gx < (int)gm.gmBlackBoxX; gx++) {
                                int px = ox + gx, py = oy + gy, a;
                                if (px < 0 || px >= (int)cellw || py < 0 || py >= H) continue;
                                a = gbuf[gy * pitch + gx] * 255 / 64;
                                if (a > 255) a = 255;
                                if (a) ((uint16_t *)pages[page])[(y + py) * P + x + px] = (uint16_t)(0x00ff | (a << 8));
                            }
                }
                if (adv > (int)cellw) adv = cellw;
                g.ch = c; g.tex = (uint16_t)page; g.w = adv;
                g.u0 = (float)x / P; g.v0 = (float)y / P;
                g.u1 = (float)(x + adv) / P; g.v1 = (float)(y + H) / P;
                x += cellw;
            }
            ng[k] = g;
        }
        (void)top0;
        hdr[4] = P; hdr[5] = P; hdr[6] = npages;
        put(&b, hdr, 28);
        for (k = 0; k < npages; k++) { put(&b, pages[k], P * P * 2); free(pages[k]); }
        put(&b, ng, (nglyph + nadd) * sizeof(Glyph));
        dlog("font %d sub %u: H %d em %d base %d/%d page %u x%u", kind, s, H, em, base, ebase, P, npages);
        free(ng); free(pages); free(gbuf);
        SelectObject(dc, old);
        DeleteObject(hf);
    }
    DeleteDC(dc);
    free(add);
    *out = b.p;
    *outn = b.n;
    return 1;
bad:
    dlog("font_build: bad LIQDFONT at %x", p);
    DeleteDC(dc);
    free(add);
    free(b.p);
    return 0;
}

/* the .lfn loader (0x6bd750): texture format A8R8G8B8 -> A8L8, bytes per row w*4 -> w*2 */
int font_init(void)
{
    /* push 15h; push ecx; push edx; call ..; add esp,18h; mov [esp+10h],eax; nop; cmp eax,edi; je .. */
    static const uint8_t a[] = { 0x6a, 0x15, 0x51, 0x52, 0xe8, 0, 0, 0, 0, 0x83, 0xc4, 0x18, 0x89, 0x44, 0x24, 0x10, 0x90, 0x3b, 0xc7, 0x0f, 0x84, 0x7d, 0x01, 0x00, 0x00 };
    /* shl edi,2; cmp edi,eax; mov [esp+14h],0; jne ..; imul ebp,edi; lea ecx,[esp+14h]; push ecx; mov ecx,[esp+60h] */
    static const uint8_t b[] = { 0xc1, 0xe7, 0x02, 0x3b, 0xf8, 0xc7, 0x44, 0x24, 0x14, 0, 0, 0, 0, 0x75, 0x36, 0x0f, 0xaf, 0xef, 0x8d, 0x4c, 0x24, 0x14, 0x51, 0x8b, 0x4c, 0x24, 0x60 };
    static const uint8_t pa = 0x33, pb = 0x01;
    uint32_t xa = find_code(0x6bd84c, a, "xxxxx....xxxxxxxxxxxxxxxx"), xb = find_code(0x6bd8a0, b, "xxxxxxxxxxxxxxxxxxxxxxxxxxx");
    if (!xa || !xb) { dlog("font: unexpected loader code"); return 0; }
    patch_bytes(xa + 1, &pa, 1);
    patch_bytes(xb + 2, &pb, 1);
    return 1;
}
