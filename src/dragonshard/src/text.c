/* String table (DND_Text.ltx) translation.
   Resource DSJP/1: u32 count, count * {u64 fnv64(English UTF-16LE), u32 offset}, sorted by hash,
   then the Japanese strings (UTF-16LE, NUL terminated) at the offsets (from the start of the resource).
   Resource DSJP/2: u32 count, count * u16: every non-ASCII character the translation uses (sorted). */
#include "dsjp.h"

typedef struct { uint64_t h; uint32_t off; } TrEnt;
#pragma pack(push, 1)
typedef struct { uint32_t lo, hi, off; } TrRaw;
#pragma pack(pop)

static const uint8_t *g_tr;
static uint32_t g_trn;
static const uint16_t *g_chars;
static int g_ncharset;

static uint64_t fnv64(const uint16_t *s, int n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    int i;
    for (i = 0; i < n; i++) {
        h ^= (uint8_t)(s[i] & 255); h *= 0x100000001b3ull;
        h ^= (uint8_t)(s[i] >> 8);  h *= 0x100000001b3ull;
    }
    return h;
}

void text_init(void)
{
    DWORD n;
    const uint8_t *r = (const uint8_t *)res_get(1, &n);
    if (r && n >= 4) { g_tr = r; g_trn = *(const uint32_t *)r; }
    r = (const uint8_t *)res_get(2, &n);
    if (r && n >= 4) { g_ncharset = *(const uint32_t *)r; g_chars = (const uint16_t *)(r + 4); }
    dlog("text: %u translations, %d chars", g_trn, g_ncharset);
}

const uint16_t *text_charset(int *n) { *n = g_ncharset; return g_chars; }

static const uint16_t *lookup(const uint16_t *s, int n)
{
    uint64_t h = fnv64(s, n);
    const TrRaw *t = (const TrRaw *)(g_tr + 4);
    uint32_t lo = 0, hi = g_trn;
    while (lo < hi) {
        uint32_t m = (lo + hi) / 2;
        uint64_t k = ((uint64_t)t[m].hi << 32) | t[m].lo;
        if (k == h) return (const uint16_t *)(g_tr + t[m].off);
        if (k < h) lo = m + 1; else hi = m;
    }
    return NULL;
}

/* buffer helpers */
typedef struct { uint8_t *p; uint32_t n, cap; } Buf;
static void put(Buf *b, const void *d, uint32_t n)
{
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 65536;
        b->p = (uint8_t *)realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, d, n);
    b->n += n;
}

/* LIQDTEXT: 'LIQDTEXT' + 8 bytes, then groups until EOF:
   u8 flag, wstr name, u32 count, count * (u32 id, u8 flag, wstr text); wstr = u32 nchars incl. NUL (0 = none) + UTF-16 */
int text_build(const uint8_t *src, uint32_t n, uint8_t **out, uint32_t *outn)
{
    Buf b = { 0 };
    uint32_t p = 16, replaced = 0, total = 0;
    if (n < 16 || memcmp(src, "LIQDTEXT", 8) || !g_tr) return 0;
    put(&b, src, 16);
    while (p < n) {
        uint32_t cnt, i, len;
        int keys;
        if (p + 5 > n) goto bad;
        len = *(const uint32_t *)(src + p + 1);
        if (p + 5 + len * 2 + 4 > n) goto bad;
        /* key names (group "Keys") stay English, as on the keyboard */
        keys = len == 5 && !memcmp(src + p + 5, L"Keys", 8);
        put(&b, src + p, 5 + len * 2 + 4);              /* group flag, name, count */
        p += 5 + len * 2;
        cnt = *(const uint32_t *)(src + p);
        p += 4;
        for (i = 0; i < cnt; i++) {
            const uint16_t *s, *ja;
            if (p + 9 > n) goto bad;
            len = *(const uint32_t *)(src + p + 5);
            if (p + 9 + len * 2 > n) goto bad;
            s = (const uint16_t *)(src + p + 9);
            ja = len > 1 && !keys ? lookup(s, len - 1) : NULL;
            total++;
            if (ja) {
                uint32_t jl = (uint32_t)wcslen((const wchar_t *)ja) + 1;
                put(&b, src + p, 5);
                put(&b, &jl, 4);
                put(&b, ja, jl * 2);
                replaced++;
            } else {
                put(&b, src + p, 9 + len * 2);
            }
            p += 9 + len * 2;
        }
    }
    dlog("text_build: %u / %u strings replaced", replaced, total);
    *out = b.p;
    *outn = b.n;
    return 1;
bad:
    dlog("text_build: bad LIQDTEXT at %x", p);
    free(b.p);
    return 0;
}
