/* Line breaking for Japanese text.
   The game wraps a text object (0x69cf80) by finding how many glyph entries fit and then calling 0x69dc80,
   which walks back from the first entry that does not fit to the last space (entry flags & 3) and returns
   its index (the line ends there, inclusive), or 0 when there is none. Japanese has no spaces, so we
   also allow a break between two characters when one of them is Japanese (with the usual kinsoku rules).

   text object: +0x14 list of glyph entries (node: +0 next, +4 prev, +8 data), +0x40 entry count
   entry data:  +0 glyph pointer (NULL for spaces / markup; glyph+0 = u16 character), +0x128 flags (1 space, 2 para, 4 newline) */
#include "dsjp.h"

static int is_jp(unsigned c)
{
    return (c >= 0x2e80 && c <= 0x9fff) || (c >= 0xf900 && c <= 0xfaff) || (c >= 0xff00 && c <= 0xffef) || c == 0x2015 || c == 0x2026;
}
static int no_start(unsigned c)   /* may not begin a line */
{
    static const wchar_t s[] = L"、。，．・：；？！ー―…‥」』）】〕〉》｝］〟’”ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々〻ゝゞヽヾ!?),.:;";
    const wchar_t *p;
    for (p = s; *p; p++) if (*p == c) return 1;
    return 0;
}
static int no_end(unsigned c)     /* may not end a line */
{
    static const wchar_t s[] = L"「『（【〔〈《｛［〝‘“(";
    const wchar_t *p;
    for (p = s; *p; p++) if (*p == c) return 1;
    return 0;
}

#define MAXE 4096

static uint32_t __cdecl wrap_break(uint8_t *obj, uint32_t pos)
{
    static uint8_t *ent[MAXE];
    uint32_t cnt = *(uint32_t *)(obj + 0x40), n = 0, i;
    uint8_t *node;
    if (pos >= cnt) return pos;
    for (node = *(uint8_t **)(obj + 0x14); node && n < MAXE && n <= pos; node = *(uint8_t **)node) ent[n++] = node + 8;
    if (n <= pos) return 0;
    for (i = pos; ; i--) {
        uint8_t *e = ent[i];
        if (*(uint32_t *)(e + 0x128) & 3) return i;
        if (i == 0) break;
        {
            uint8_t *ga = *(uint8_t **)ent[i - 1], *gb = *(uint8_t **)e;
            unsigned a = ga ? *(uint16_t *)ga : 0, b = gb ? *(uint16_t *)gb : 0;
            if (a && b && (is_jp(a) || is_jp(b)) && !no_start(b) && !no_end(a)) return i - 1;
        }
    }
    return 0;
}

static __declspec(naked) void wrap_stub(void)
{
    __asm {
        push edx
        push ecx
        push eax
        call wrap_break
        add esp, 8
        pop edx
        ret
    }
}

void wrap_init(void)
{
    /* よく似た関数が 3 つあり、違いは呼び出し先と +3Dh の mov bl,3 だけ */
    static const uint8_t sig[64] = {
        0x57, 0x8b, 0xf9, 0x3b, 0x78, 0x40, 0x72, 0x04, 0x8b, 0xc7, 0x5f, 0xc3, 0x56, 0x8d, 0x70, 0x14,
        0x8b, 0x06, 0x85, 0xc0, 0x89, 0x46, 0x14, 0x74, 0x05, 0x83, 0xc0, 0x08, 0xeb, 0x02, 0x33, 0xc0,
        0x85, 0xff, 0x53, 0x76, 0x0c, 0x8b, 0xdf, 0x8b, 0xce, 0xe8, 0x72, 0xf8, 0x04, 0x00, 0x4b, 0x75,
        0xf6, 0x85, 0xc0, 0x75, 0x06, 0x5b, 0x5e, 0x8b, 0xc7, 0x5f, 0xc3, 0xb3, 0x03, 0x8d, 0x49, 0x00 };
    uint32_t at = find_code(0x69dc80, sig, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxxxxxx");
    if (!at) { dlog("wrap: unexpected code"); return; }
    patch_jmp(at, (void *)wrap_stub);
}

/* Glyph lookup (0x682ef0: eax = character, [esp+4] = sub-font; returns glyph data = node + 8 or NULL).
   The game keeps a sub-font's glyphs in a sorted linked list and binary-searches it by walking the list,
   which gets slow with a few thousand Japanese glyphs; we index each list once. */
typedef struct { uint8_t *sub, *head; uint32_t count; uint8_t **map; } GIdx;
static GIdx g_idx[32];
static int g_nidx;
static uint32_t g_used_h;   /* bit per pixel height 0..31 that was asked for (test log) */

static uint8_t *__cdecl getglyph(uint8_t *sub, uint32_t ch)
{
    uint8_t *head = *(uint8_t **)(sub + 0x20);
    uint32_t count = *(uint32_t *)(sub + 0x20 + 0x18);
    int i;
    GIdx *x = NULL;
    ch &= 0xffff;
    if (g_test) {
        uint32_t h = *(uint32_t *)(sub + 4);
        if (h < 64 && !(g_used_h & (1u << (h & 31)))) { g_used_h |= 1u << (h & 31); dlog("font size %u used", h); }
    }
    for (i = 0; i < g_nidx; i++) if (g_idx[i].sub == sub) { x = &g_idx[i]; break; }
    if (!x) {
        if (g_nidx == 32) { memset(g_idx, 0, sizeof g_idx); g_nidx = 0; }
        x = &g_idx[g_nidx++];
        x->sub = sub;
    }
    if (x->head != head || x->count != count || !x->map) {
        uint8_t *n;
        if (!x->map) x->map = (uint8_t **)VirtualAlloc(NULL, 65536 * sizeof(uint8_t *), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!x->map) return NULL;
        memset(x->map, 0, 65536 * sizeof(uint8_t *));
        for (n = head; n; n = *(uint8_t **)n) x->map[*(uint16_t *)(n + 8)] = n;
        x->head = head;
        x->count = count;
    }
    return x->map[ch] ? x->map[ch] + 8 : NULL;
}

static __declspec(naked) void getglyph_stub(void)
{
    __asm {
        push ecx
        push edx
        push eax
        push dword ptr [esp + 0x10]
        call getglyph
        add esp, 8
        pop edx
        pop ecx
        ret 4
    }
}

void glyph_init(void)
{
    static const uint8_t sig[] = { 0x83, 0xec, 0x1c, 0x8d, 0x4c, 0x24, 0x20, 0x66, 0x89, 0x04, 0x24, 0x8b, 0x44, 0x24, 0x20,
                                   0x51, 0x8d, 0x54, 0x24, 0x04, 0x52, 0x83, 0xc0, 0x20 };
    uint32_t at = find_code(0x682ef0, sig, "xxxxxxxxxxxxxxxxxxxxxxxx");
    if (!at) { dlog("glyph: unexpected code"); return; }
    patch_jmp(at, (void *)getglyph_stub);
}
