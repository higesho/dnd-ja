/* 訳の表（資源 1, tools/make_data.py が作る）
   "DSTJ", u32 n, n x {u64 FNV-1a(英語 UTF-16LE), u32 英語の字数, u32 訳の字数, 訳 UTF-16LE}, u32 ncs, ncs x u16
   英語の文そのものは持たない（原作の文章を配布物に入れないため、ハッシュで照合する） */
#include "dst.h"
#include <stdlib.h>

typedef struct { uint64_t h; uint32_t enlen, jalen; const uint16_t *ja; } Entry;
static Entry *g_e;
static uint32_t g_n, g_mask;
static int32_t *g_hash;
static const uint16_t *g_cs;
static int g_ncs;

static uint64_t fnv64(const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint64_t h = 14695981039346656037ull;
    while (n--) h = (h ^ *b++) * 1099511628211ull;
    return h;
}

int data_init(void)
{
    DWORD size;
    const uint8_t *p = (const uint8_t *)res_get(1, &size), *end;
    uint32_t i, sz = 1;
    if (!p || size < 8 || memcmp(p, "DSTJ", 4)) return 0;
    end = p + size;
    g_n = *(const uint32_t *)(p + 4);
    p += 8;
    g_e = (Entry *)calloc(g_n ? g_n : 1, sizeof(Entry));
    while (sz < g_n * 2 + 2) sz <<= 1;
    g_hash = (int32_t *)calloc(sz, 4);
    g_mask = sz - 1;
    for (i = 0; i < g_n; i++) {
        uint32_t k;
        if (p + 16 > end) return 0;
        g_e[i].h = *(const uint64_t *)p;
        g_e[i].enlen = *(const uint32_t *)(p + 8);
        g_e[i].jalen = *(const uint32_t *)(p + 12);
        g_e[i].ja = (const uint16_t *)(p + 16);
        p += 16 + g_e[i].jalen * 2;
        k = (uint32_t)g_e[i].h & g_mask;
        while (g_hash[k]) k = (k + 1) & g_mask;
        g_hash[k] = (int32_t)i + 1;
    }
    if (p + 4 > end) return 0;
    g_ncs = (int)*(const uint32_t *)p;
    g_cs = (const uint16_t *)(p + 4);
    dlog("data: %u entries, %d chars", g_n, g_ncs);
    return 1;
}

const uint16_t *data_lookup(const uint16_t *en, int n, int *outn)
{
    uint64_t h;
    uint32_t k;
    if (!g_hash || n <= 0) return NULL;
    h = fnv64(en, (uint32_t)n * 2);
    k = (uint32_t)h & g_mask;
    while (g_hash[k]) {
        const Entry *e = &g_e[g_hash[k] - 1];
        if (e->h == h && e->enlen == (uint32_t)n) { *outn = (int)e->jalen; return e->ja; }
        k = (k + 1) & g_mask;
    }
    return NULL;
}

const uint16_t *data_charset(int *n) { *n = g_ncs; return g_cs; }
