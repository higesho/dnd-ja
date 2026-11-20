/*
 * Ravenloft: Stone Prophet Japanese patch - runtime part.
 * Loaded into DOSBox.exe (Steam "Ravenloft Series" SVN build, also 0.74-3 / GOG 0.74-2.1) as a winmm.dll proxy.
 *  - redirects fopen() of T (all game text), CODE.1, CODE.2, OBJECTS.DAT, NPCS.DAT to copies built in %LOCALAPPDATA%\PROPHET_JP
 *  - CODE.1/CODE.2 (DOS/16M flat): the font width lookups and the char-draw functions call DOSBox callbacks;
 *    a Japanese glyph is put into a one-frame sprite in DOS memory and drawn by the game's own Mode X blitter.
 *  Japanese text uses a private 2-byte code: lead 0x80-0xBF, trail 0xC0-0xFF; 0x1F marks a line-break opportunity.
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "resource.h"

void proxy_init(void);

/* ---------------------------------------------------------------- logging */
static char g_dir[MAX_PATH];
static FILE *g_log;
static int g_logon;
static void dbg(const char *fmt, ...)
{
    va_list ap;
    if (!g_logon) return;
    if (!g_log) {
        char p[MAX_PATH];
        wsprintfA(p, "%s\\log.txt", g_dir);
        g_log = fopen(p, "a");
        if (!g_log) return;
    }
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);
}

/* ------------------------------------------------- DOSBox 0.74-3 internals */
static uint32_t g_dbx, g_dbx_code;   /* DOSBox build: Steam 0.74-3 = 0, GOG 0.74-2.1 = +0xe000 / -0x10, Ravenloft SVN = +0x3b000 / +0xbc0 */
#define A_CB_HANDLERS (0x74d460u + g_dbx)
#define A_CB_ILLEGAL (0x415e40u + g_dbx_code)
#define A_REGS (0x74d1e0u + g_dbx)
#define A_SEGVAL (0x74d100u + g_dbx)
#define A_SEGPHYS (0x74d110u + g_dbx)
#define A_TLB_READ (0x74d6d0u + g_dbx)
#define A_TLB_WRITE (0xb4d6d0u + g_dbx)
#define A_TLB_RH (0xf4d6d0u + g_dbx)
#define A_TLB_WH (0x134d6d0u + g_dbx)

typedef uint32_t (__cdecl *ph_rb)(void *self, uint32_t a);
typedef void (__cdecl *ph_wb)(void *self, uint32_t a, uint32_t v);

static int verify_dosbox(void)
{
    /* Steam (DOSBox 0.74-3) and GOG (DOSBox 0.74-2.1) builds: same code, globals shifted by g_dbx, IllegalHandler by g_dbx_code */
    static const struct { uint32_t s1, s2, s3, dbx, dcode; } builds[] = {
        {0x415e66u, 0x41897fu, 0x4351ccu, 0u, 0u},
        {0x415e56u, 0x41896fu, 0x4351bcu, 0xe000u, 0xfffffff0u},
        {0x416a26u, 0x41965fu, 0x431da2u, 0x3b000u, 0xbc0u},
    };
    HMODULE h = GetModuleHandleA(NULL);
    int i;
    if ((uintptr_t)h != 0x400000) return 0;
    for (i = 0; i < 3; i++) {
        uint32_t cb = 0x74d460u + builds[i].dbx, ill = 0x415e40u + builds[i].dcode;
        uint32_t tlb = 0x74d6d0u + builds[i].dbx, regs = 0x74d1e0u + builds[i].dbx;
        uint8_t s1[11] = {0x81, 0x3c, 0x85}, s2[7] = {0x8b, 0x04, 0x95}, s3[6] = {0x66, 0xa3};
        memcpy(s1 + 3, &cb, 4); memcpy(s1 + 7, &ill, 4);
        memcpy(s2 + 3, &tlb, 4);
        memcpy(s3 + 2, &regs, 4);
        if (memcmp((void*)(uintptr_t)builds[i].s1, s1, 11) || memcmp((void*)(uintptr_t)builds[i].s2, s2, 7) ||
            memcmp((void*)(uintptr_t)builds[i].s3, s3, 6)) continue;
        g_dbx = builds[i].dbx; g_dbx_code = builds[i].dcode;
        return 1;
    }
    return 0;
}

static __inline uint8_t rb(uint32_t a)
{
    uint8_t *p = ((uint8_t**)A_TLB_READ)[a >> 12];
    void *h;
    if (p) return p[a];
    h = ((void**)A_TLB_RH)[a >> 12];
    return (uint8_t)((ph_rb)((*(void***)h)[2]))(h, a);
}
static __inline void wb(uint32_t a, uint8_t v)
{
    uint8_t *p = ((uint8_t**)A_TLB_WRITE)[a >> 12];
    void *h;
    if (p) { p[a] = v; return; }
    h = ((void**)A_TLB_WH)[a >> 12];
    ((ph_wb)((*(void***)h)[5]))(h, a, v);
}
static __inline uint16_t rw(uint32_t a) { return (uint16_t)(rb(a) | (rb(a + 1) << 8)); }
static __inline int16_t rws(uint32_t a) { return (int16_t)rw(a); }
static __inline void ww(uint32_t a, uint16_t v) { wb(a, (uint8_t)v); wb(a + 1, (uint8_t)(v >> 8)); }
static __inline uint32_t farptr(uint32_t a) { return ((uint32_t)rw(a + 2) << 4) + rw(a); }
#define REG32(i) (((uint32_t*)A_REGS)[i])
#define R_EAX REG32(0)
#define R_EBX REG32(3)
#define R_ESP REG32(4)
#define R_EBP REG32(5)
#define R_EIP REG32(8)
#define SEGV(i) (((uint16_t*)A_SEGVAL)[i])
#define PHYS_CS (((uint32_t*)A_SEGPHYS)[1])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define SP16 ((uint16_t)R_ESP)
#define BP16 ((uint16_t)R_EBP)
static __inline void push16(uint16_t v)
{
    uint16_t sp = (uint16_t)(SP16 - 2);
    R_ESP = (R_ESP & 0xffff0000u) | sp;
    ww(PHYS_SS + sp, v);
}
static __inline void set16(uint32_t *r, uint16_t v) { *r = (*r & 0xffff0000u) | v; }

/* ------------------------------------------------------- patch blob */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)

static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;
static uint16_t *g_glyph_index;  /* 65536 entries -> glyph number+1 */
static const uint8_t *g_glyphs;  /* 8 bytes each (rows, bit7 = left) */
static const uint8_t *g_gflags;  /* per glyph: 1 = no line start, 2 = no line end */
static const uint8_t *g_big;     /* 14x14 glyphs, 28 bytes each (2 bytes per row), same order as g_glyphs */
static const uint8_t *g_six;     /* the 8x8 glyphs squeezed to 6 rows (8 bytes each) */
static const uint8_t *g_t12;     /* k8x12 glyphs for the prose fonts (12 bytes each, 8 columns) */
static uint32_t g_text_mask;     /* bit h set: a font of height h is a prose font (k8x12) */
static int g_text_w = 8;
static int g_in_wrap;            /* 1 while the game's word-wrap (prose) function runs with a line pitch >= 12 */
static uint8_t g_wrap_stack[16]; static int g_wrap_depth;
static uint32_t g_ret_stack[16];
static int32_t g_tramp_delta;     /* trampoline (cb 9) = word-wrap entry callback end + delta */
static int g_cb_base = 0x70;
static int g_small_w = 9;        /* advance of a small (8-row) Japanese glyph */
static int g_enabled;

static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xffffffffu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}

static int load_blob(HMODULE self)
{
    HRSRC r = FindResourceA(self, MAKEINTRESOURCEA(IDR_PATCH), (LPCSTR)RT_RCDATA);
    HGLOBAL g;
    const blob_hdr *h;
    const uint8_t *p;
    uint32_t i;
    if (!r) return 0;
    g = LoadResource(self, r);
    g_blob = (const uint8_t*)LockResource(g);
    g_blob_len = SizeofResource(self, r);
    if (!g_blob || g_blob_len < sizeof(blob_hdr)) return 0;
    h = (const blob_hdr*)g_blob;
    if (memcmp(h->magic, "MZJP", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len);
    p = g_blob + sizeof(blob_hdr);
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == 'F') {     /* font: u32 n; n*u16 code; n*8 bitmap; n flags; n*28 big bitmap */
            uint32_t n = *(const uint32_t*)d, k;
            const uint16_t *codes = (const uint16_t*)(d + 4);
            g_glyphs = d + 4 + n * 2;
            g_gflags = g_glyphs + n * 8;
            g_big = g_gflags + n;
            g_six = g_big + n * 28;
            if (s->len >= 4 + n * (2 + 8 + 1 + 28 + 8 + 12)) g_t12 = g_six + n * 8;
            g_glyph_index = (uint16_t*)calloc(65536, 2);
            for (k = 0; k < n; k++) g_glyph_index[codes[k]] = (uint16_t)(k + 1);
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0];
            g_logon = (int)v[1];
            if (s->len >= 12) g_small_w = (int)v[2];
            if (s->len >= 20) { g_text_mask = v[3]; g_text_w = (int)v[4]; }
            if (s->len >= 24) g_tramp_delta = (int32_t)v[5];
        }
        p = d + s->len;
    }
    return g_glyph_index != NULL;
}

static const uint8_t *find_sect(uint32_t type, const char *name, uint32_t *len)
{
    const blob_hdr *h = (const blob_hdr*)g_blob;
    const uint8_t *p = g_blob + sizeof(blob_hdr);
    uint32_t i;
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == type && !_stricmp((const char*)d, name)) {
            *len = s->len - 16;
            return d + 16;
        }
        p = d + s->len;
    }
    return NULL;
}

/* ------------------------------------------------------------ Japanese text */
#define IS_LEAD(c)  ((c) >= 0x80 && (c) <= 0xbf)
#define IS_TRAIL(c) ((c) >= 0xc0)

static int glyph_no(uint8_t lead, uint8_t trail)
{
    return g_glyph_index[(lead << 8) | trail];   /* 0 = none */
}

/* our scratch DOS memory (4 KB, reserved from the free MCB); DOS/4G maps the first megabyte 1:1 */
static uint32_t g_mem;

#define R_ECX REG32(1)
#define R_EDX REG32(2)
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
static __inline uint32_t rd(uint32_t a) { return rb(a) | (rb(a + 1) << 8) | (rb(a + 2) << 16) | ((uint32_t)rb(a + 3) << 24); }
static __inline void wd(uint32_t a, uint32_t v) { wb(a, (uint8_t)v); wb(a + 1, (uint8_t)(v >> 8)); wb(a + 2, (uint8_t)(v >> 16)); wb(a + 3, (uint8_t)(v >> 24)); }
#define STK(off) (PHYS_SS + R_ESP + (off))
static __inline void push32(uint32_t v) { R_ESP -= 4; wd(PHYS_SS + R_ESP, v); }

/* DreamForge font = {u32 sprite handle, u8 width[128] at +4, ...}; sprite = *handle:
 * u16 frame_size, u16 ?, u16 height, u8 words per plane row, u8 flags (0x80 = first byte of a frame is the transparent key);
 * frames at +8: [key] then 4 plane blocks of height rows x (2*words) bytes (plane k holds the pixels x = 4*c + k) */
static uint32_t font_sprite(uint32_t font)
{
    uint32_t h = rd(PHYS_DS + font);      /* Stone Prophet (like Menzoberranzan): font -> handle -> sprite */
    return h ? rd(PHYS_DS + h) : 0;
}
static int jp_w_sprite(uint32_t spr)
{
    int h = spr ? rw(PHYS_DS + spr + 4) : 8;
    if (h >= 13) return 15;
    if (g_t12 && g_in_wrap && h < 32 && ((g_text_mask >> h) & 1)) return g_text_w;
    return g_small_w;
}
static int jp_w(uint32_t font) { return jp_w_sprite(font_sprite(font)); }

/* CB width: replaces "movzx eax, byte ptr [r1 + r2 + 4]" (5 bytes) by "callback; sib".
 * One of the two registers holds the character (sign-extended), the other the font. */
static uint32_t width_of(void)
{
    uint8_t sib = rb(PHYS_CS + R_EIP);
    uint32_t r1 = REG32(sib & 7), r2 = REG32((sib >> 3) & 7), font, ch;
    if (r1 + 128u < 384u) { ch = r1; font = r2; } else { ch = r2; font = r1; }
    ch &= 0xff;
    R_EIP += 1;
    if (IS_LEAD(ch)) return (uint32_t)jp_w(font);
    if (IS_TRAIL(ch) || ch == 0x1f) return 0;
    return rb(PHYS_DS + font + ch + 4);
}
static uint32_t __cdecl cb_width(void) { R_EAX = width_of(); return 0; }       /* movzx eax, [r1+r2+4] */
static uint32_t __cdecl cb_width_edx(void) { R_EDX = width_of(); return 0; }   /* movzx edx, [r1+r2+4] */

static uint8_t g_lead;
typedef struct { uint32_t spr; uint8_t ink, shade, key; } colcache;
static colcache g_cc[8];

/* colours of a font sprite: the most common non-key value of the frame of 'A' = ink, the next one = shade */
static void sprite_colours(uint32_t spr, uint8_t *ink, uint8_t *shade, uint8_t *key)
{
    uint32_t fs = rw(PHYS_DS + spr), fr = PHYS_DS + spr + 8 + fs * 'A', i;
    int cnt[256], b1 = -1, b2 = -1, k;
    uint8_t fl = rb(PHYS_DS + spr + 7);
    for (k = 0; k < 8; k++) if (g_cc[k].spr == spr) { *ink = g_cc[k].ink; *shade = g_cc[k].shade; *key = g_cc[k].key; return; }
    memset(cnt, 0, sizeof cnt);
    *key = (fl & 0x80) ? rb(fr) : 0;
    for (i = (fl & 0x80) ? 1 : 0; i < fs && i < 2048; i++) cnt[rb(fr + i)]++;
    cnt[*key] = 0;
    for (k = 0; k < 256; k++) if (cnt[k] && (b1 < 0 || cnt[k] > cnt[b1])) b1 = k;
    for (k = 0; k < 256; k++) if (k != b1 && cnt[k] && (b2 < 0 || cnt[k] > cnt[b2])) b2 = k;
    *ink = (uint8_t)(b1 < 0 ? 15 : b1);
    *shade = (uint8_t)(b2 < 0 ? *key : b2);
    memmove(g_cc + 1, g_cc, sizeof g_cc[0] * 7);
    g_cc[0].spr = spr; g_cc[0].ink = *ink; g_cc[0].shade = *shade; g_cc[0].key = *key;
    dbg("font sprite %x: h=%d words=%d flags=%x ink=%d shade=%d key=%d\n", spr, rw(PHYS_DS + spr + 4),
        rb(PHYS_DS + spr + 6), fl, *ink, *shade, *key);
}

/* fonts are sprites of at most 128 frames (u16 at +2); other sprites drawn through the same functions are left alone */
static int is_font(uint32_t spr) { return spr && rw(PHYS_DS + spr + 2) <= 0x80; }

/* builds a one-frame sprite for the glyph at g_mem (linear), returns its address or 0 */
static uint32_t make_glyph(uint32_t spr, int gn)
{
    uint32_t B = g_mem + 16, k;
    int h = rw(PHYS_DS + spr + 4), big = h >= 13, gw = big ? 16 : 8, words = gw / 8, fs, x, y, y0;
    int text = !big && g_t12 && g_in_wrap && h < 32 && ((g_text_mask >> h) & 1);
    uint8_t ink, shade, key, pix[20][16];
    if (!g_mem || h <= 0 || h > 20) return 0;
    sprite_colours(spr, &ink, &shade, &key);
    memset(pix, key, sizeof pix);
    if (big) {
        const uint8_t *bm = g_big + (gn - 1) * 28;
        y0 = (h - 14) / 2;
        for (y = 0; y < 14; y++) for (x = 0; x < 14; x++)
            if ((bm[y * 2 + (x >> 3)] >> (7 - (x & 7))) & 1) pix[y + y0][x] = ink;
    } else if (text) {
        const uint8_t *bm = g_t12 + (gn - 1) * 12;
        h = 12;                                    /* our frame is 12 rows high (the line pitch of the prose fonts) */
        for (y = 0; y < 12; y++) for (x = 0; x < 8; x++)
            if ((bm[y] >> (7 - x)) & 1) pix[y][x] = ink;
    } else {
        const uint8_t *bm = g_glyphs + (gn - 1) * 8;
        y0 = h > 8 ? (h - 8 + 1) / 2 : 0;
        for (y = 0; y < 8 && y + y0 < h; y++) for (x = 0; x < 8; x++)
            if ((bm[y] >> (7 - x)) & 1) pix[y + y0][x] = ink;
    }
    fs = 1 + 4 * h * words * 2;
    for (k = 0; k < 8; k++) wb(B + k, rb(PHYS_DS + spr + k));
    ww(B, (uint16_t)fs);
    ww(B + 4, (uint16_t)h);
    wb(B + 6, (uint8_t)words);
    wb(B + 7, (uint8_t)(rb(PHYS_DS + spr + 7) | 0x80));
    wb(B + 8, key);
    for (k = 0; k < 4; k++)
        for (y = 0; y < h; y++)
            for (x = 0; x < words * 2; x++)
                wb(B + 9 + (k * h + y) * words * 2 + x, pix[y][4 * x + k]);
    return B - PHYS_DS;
}

/* CB draw: replaces "push dword ptr [esp + 0x10]" (57fb3, argoff 0x10) / "push dword ptr [esp + 0x14]" (58003, 0x14)
 * of the two char-draw functions (ch, y, x, sprite[, remap]). */
/* the patched push follows "cmp dword ptr [esp+10h],0; je <ret>": the je gives the address of a ret */
static uint32_t do_draw(int argoff)
{
    uint8_t c = (uint8_t)rd(STK(4));
    uint32_t spr = rd(STK(0x10));
    uint32_t ret = R_EIP - 4 + (uint32_t)(int32_t)(int8_t)rb(PHYS_CS + R_EIP - 5);
    if (!is_font(spr)) c = 0;                  /* the same functions draw any sprite frame (panels: 400 frames) */
    if (c == 0x1f || IS_LEAD(c)) {
        if (IS_LEAD(c)) g_lead = c;
        R_EIP = ret;                           /* nothing pushed yet */
        return 0;
    }
    if (IS_TRAIL(c)) {
        int gn = g_lead ? glyph_no(g_lead, c) : 0;
        uint32_t f = gn && spr ? make_glyph(spr, gn) : 0;
        g_lead = 0;
        if (!f) { R_EIP = ret; return 0; }
        wd(STK(4), 0);
        wd(STK(0x10), f);
        wd(STK(0xc), rd(STK(0xc)) - (uint32_t)jp_w_sprite(spr));    /* x was already advanced by the lead's width */
    } else g_lead = 0;
    push32(rd(STK(argoff)));
    return 0;
}
static uint32_t __cdecl cb_draw1(void) { return do_draw(0x10); }
static uint32_t __cdecl cb_draw2(void) { return do_draw(0x14); }

/* CODE.1 (frame-pointer build): replaces "mov eax, [ebp+argoff]; push eax" after "cmp dword [ebp+14h],0; je <ret>";
 * args at [ebp+8]: ch, y, x, sprite[, remap] */
#define ARG(off) (PHYS_SS + R_EBP + (off))
static uint32_t do_draw_bp(int argoff)
{
    uint8_t c = (uint8_t)rd(ARG(8));
    uint32_t spr = rd(ARG(0x14));
    uint32_t ret = R_EIP - 4 + (uint32_t)(int32_t)(int8_t)rb(PHYS_CS + R_EIP - 5);
    if (!is_font(spr)) c = 0;                  /* the same functions draw any sprite frame (panels: 400 frames) */
    if (c == 0x1f || IS_LEAD(c)) {
        if (IS_LEAD(c)) g_lead = c;
        R_EIP = ret;
        return 0;
    }
    if (IS_TRAIL(c)) {
        int gn = g_lead ? glyph_no(g_lead, c) : 0;
        uint32_t f = gn && spr ? make_glyph(spr, gn) : 0;
        g_lead = 0;
        if (!f) { R_EIP = ret; return 0; }
        wd(ARG(8), 0);
        wd(ARG(0x14), f);
        wd(ARG(0x10), rd(ARG(0x10)) - (uint32_t)jp_w_sprite(spr));
    } else g_lead = 0;
    R_EAX = rd(ARG(argoff));
    push32(R_EAX);
    return 0;
}
static uint32_t __cdecl cb_draw_bp1(void) { return do_draw_bp(0x14); }

/* word-wrap function entry: push ebx, esi, edi, ebp ; exit: mov eax, [esp+8] */
static uint32_t __cdecl cb_wrap_in(void)
{
    uint32_t pitch = rd(PHYS_SS + R_ESP + 0x10);  /* args: mode, y, x, pitch, width, text, font */
    push32(R_EBX); push32(REG32(6)); push32(REG32(7)); push32(R_EBP);
    if (g_wrap_depth < 16) g_wrap_stack[g_wrap_depth] = (uint8_t)g_in_wrap;
    g_wrap_depth++;
    g_in_wrap = pitch >= 12 && pitch < 40;
    return 0;
}
/* Stone Prophet: the wrap function's exit is shared code, so the entry callback (cb 8) redirects the return address
 * to a trampoline callback (cb 9) placed in padding; the trampoline restores the flag and jumps back */
static uint32_t __cdecl cb_wrap_in_t(void)
{
    uint32_t pitch = rd(PHYS_SS + R_ESP + 0x10);
    uint32_t ret = rd(PHYS_SS + R_ESP);
    push32(R_EBX); push32(REG32(6)); push32(REG32(7)); push32(R_EBP);
    if (g_wrap_depth < 16) { g_wrap_stack[g_wrap_depth] = (uint8_t)g_in_wrap; g_ret_stack[g_wrap_depth] = ret; }
    g_wrap_depth++;
    g_in_wrap = pitch >= 12 && pitch < 40;
    if (g_wrap_depth <= 16) wd(PHYS_SS + R_ESP + 16, R_EIP + (uint32_t)g_tramp_delta);
    dbg("wrap in: eip=%x ret=%x tramp=%x pitch=%d depth=%d\n", R_EIP, ret, R_EIP + (uint32_t)g_tramp_delta, pitch, g_wrap_depth);
    return 0;
}
static uint32_t __cdecl cb_wrap_tramp(void)
{
    if (g_wrap_depth > 0) g_wrap_depth--;
    g_in_wrap = g_wrap_depth < 16 ? g_wrap_stack[g_wrap_depth] : 0;
    if (g_wrap_depth < 16) R_EIP = g_ret_stack[g_wrap_depth];
    dbg("wrap out: eip=%x depth=%d\n", R_EIP, g_wrap_depth);
    return 0;
}
static uint32_t __cdecl cb_wrap_out(void)       /* replaces 'mov eax, [esp+8]' before the exit */
{
    R_EAX = rd(PHYS_SS + R_ESP + 8);
    if (g_wrap_depth > 0) g_wrap_depth--;
    g_in_wrap = g_wrap_depth < 16 ? g_wrap_stack[g_wrap_depth] : 0;
    return 0;
}
static uint32_t __cdecl cb_draw_bp2(void) { return do_draw_bp(0x18); }

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    if (g_cb_installed) return 1;
    for (i = 0; i < 10; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy (%x)\n", b + i, tab[b + i]); return 0; }
    tab[b + 0] = (uint32_t)cb_width;
    tab[b + 1] = (uint32_t)cb_draw1;
    tab[b + 2] = (uint32_t)cb_draw2;
    tab[b + 3] = (uint32_t)cb_width_edx;
    tab[b + 4] = (uint32_t)cb_draw_bp1;
    tab[b + 5] = (uint32_t)cb_draw_bp2;
    tab[b + 6] = (uint32_t)cb_wrap_in;
    tab[b + 7] = (uint32_t)cb_wrap_out;
    tab[b + 8] = (uint32_t)cb_wrap_in_t;
    tab[b + 9] = (uint32_t)cb_wrap_tramp;
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

/* ------------------------------------------- DOS memory for the fake font */
/* walk the MCB chain and carve 4 KB (+MCB) off the end of a free block, owned by DOS (8), named "MZJP" */
static uint32_t reserve_dos(void)
{
    uint32_t s0, s, best = 0;
    for (s0 = 0x40; s0 < 0x400; s0++) {
        int cnt = 0, ok = 0;
        s = s0;
        while (cnt < 200 && s < 0xa000) {
            uint8_t t = rb(s * 16);
            uint16_t size = rw(s * 16 + 3);
            if (t != 'M' && t != 'Z') break;
            cnt++;
            if (t == 'Z') { ok = cnt >= 3; break; }
            s += size + 1u;
        }
        if (ok) break;
    }
    if (s0 >= 0x400) { dbg("MCB chain not found\n"); return 0; }
    s = s0;
    for (;;) {
        uint8_t t = rb(s * 16);
        uint16_t own = rw(s * 16 + 1), size = rw(s * 16 + 3);
        if (own == 8 && size == 0x100 && rb(s * 16 + 8) == 'M' && rb(s * 16 + 9) == 'Z' && rb(s * 16 + 10) == 'J' && rb(s * 16 + 11) == 'P')
            return (s + 1) * 16u;
        if (own == 0 && size >= 0x180) best = s;
        if (t == 'Z') break;
        s += size + 1u;
    }
    if (!best) { dbg("no free DOS block\n"); return 0; }
    {
        uint32_t a = best * 16;
        uint8_t t = rb(a);
        uint16_t size = rw(a + 3);
        uint16_t nsize = (uint16_t)(size - 0x101);
        uint32_t ns = best + 1 + nsize, k;
        uint8_t mcb[16] = {0};
        mcb[0] = t; mcb[1] = 8; mcb[2] = 0; mcb[3] = 0x00; mcb[4] = 0x01;
        memcpy(mcb + 8, "MZJP", 4);
        for (k = 0; k < 16; k++) wb(ns * 16 + k, mcb[k]);
        wb(a, 'M');
        ww(a + 3, nsize);
        dbg("reserved DOS memory at %x\n", (ns + 1) * 16);
        return (ns + 1) * 16u;
    }
}

/* ----------------------------------------------------- file building */
typedef FILE *(__cdecl *fopen_t)(const char *, const char *);
static fopen_t real_fopen;

static uint8_t *read_all(const char *path, uint32_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t*)malloc(n ? n : 1);
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f);
    *len = (uint32_t)n;
    return b;
}
static int write_all(const char *path, const uint8_t *b, uint32_t n)
{
    char tmp[MAX_PATH];
    FILE *f;
    wsprintfA(tmp, "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (!f) return 0;
    if (fwrite(b, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    DeleteFileA(path);
    return MoveFileA(tmp, path);
}

/* 'X' section: u32 orig_size, u32 orig_crc, then records {u32 off, u32 len, bytes} */
static uint8_t *build_exe(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1];
    const uint8_t *p = sec + 8, *e = sec + slen;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("exe mismatch\n"); return NULL; }
    out = (uint8_t*)malloc(olen);
    memcpy(out, orig, olen);
    while (p < e) {
        uint32_t off = ((const uint32_t*)p)[0], n = ((const uint32_t*)p)[1];
        p += 8;
        if (off + n > olen) { free(out); return NULL; }
        memcpy(out + off, p, n);
        p += n;
    }
    *outlen = olen;
    return out;
}

/* 'T' section (the text file T): u32 orig_size, u32 orig_crc, u32 n, n * {u32 id, u32 len, bytes}.
 * T = u32 count, u32 offset[count], entries {u16 len, bytes ^ 0xDC}; replaced entries get the new text. */
static uint8_t *build_text(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], n = ((const uint32_t*)sec)[2];
    const uint8_t *p = sec + 12, *e = sec + slen;
    uint32_t cnt, i, total, pos;
    const uint8_t **txt;
    uint32_t *len;
    int *enc;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc || olen < 4) { dbg("T mismatch\n"); return NULL; }
    cnt = *(const uint32_t*)orig;
    if (4 + cnt * 4 > olen) return NULL;
    txt = (const uint8_t**)calloc(cnt, sizeof *txt); len = (uint32_t*)calloc(cnt, 4); enc = (int*)calloc(cnt, sizeof(int));
    for (i = 0; i < cnt; i++) {
        uint32_t o = ((const uint32_t*)(orig + 4))[i];
        if (o + 2 > olen) { free(txt); free(len); free(enc); return NULL; }
        len[i] = *(const uint16_t*)(orig + o);
        txt[i] = orig + o + 2;
        enc[i] = 1;                                   /* still XOR-ed */
    }
    for (i = 0; i < n && p + 8 <= e; i++) {
        uint32_t id = ((const uint32_t*)p)[0], l = ((const uint32_t*)p)[1];
        if (id < cnt && l < 0x10000) { txt[id] = p + 8; len[id] = l; enc[id] = 0; }
        p += 8 + l;
    }
    total = 4 + cnt * 4;
    for (i = 0; i < cnt; i++) total += 2 + len[i];
    out = (uint8_t*)malloc(total);
    *(uint32_t*)out = cnt;
    pos = 4 + cnt * 4;
    for (i = 0; i < cnt; i++) {
        uint32_t k;
        ((uint32_t*)(out + 4))[i] = pos;
        *(uint16_t*)(out + pos) = (uint16_t)len[i];
        for (k = 0; k < len[i]; k++) out[pos + 2 + k] = enc[i] ? txt[i][k] : (uint8_t)(txt[i][k] ^ 0xdc);
        pos += 2 + len[i];
    }
    free(txt); free(len); free(enc);
    *outlen = total;
    return out;
}

static const char *base_name(const char *p)
{
    const char *b = p, *q;
    for (q = p; *q; q++) if (*q == '\\' || *q == '/' || *q == ':') b = q + 1;
    return b;
}

static CRITICAL_SECTION g_cs;

static void clean_old(const char *bn)
{
    char pat[MAX_PATH], old[MAX_PATH], cur[16];
    WIN32_FIND_DATAA fd;
    HANDLE fh;
    wsprintfA(pat, "%s\\????????_%s", g_dir, bn);
    wsprintfA(cur, "%08X_", g_blob_crc);
    fh = FindFirstFileA(pat, &fd);
    if (fh == INVALID_HANDLE_VALUE) return;
    do {
        if (_strnicmp(fd.cFileName, cur, 9)) {
            wsprintfA(old, "%s\\%s", g_dir, fd.cFileName);
            DeleteFileA(old);
        }
    } while (FindNextFileA(fh, &fd));
    FindClose(fh);
}

static const char *patched_path(const char *orig_path, char *buf)
{
    const char *bn = base_name(orig_path);
    uint32_t slen, olen, nlen;
    const uint8_t *sx, *st;
    uint8_t *orig, *out;
    if (!g_enabled) return NULL;
    {   /* test aid: RLJP_SKIP=T;OBJECTS.DAT leaves those files unpatched */
        char sk[256];
        if (GetEnvironmentVariableA("RLJP_SKIP", sk, sizeof sk) && strstr(sk, bn)) return NULL;
    }
    sx = find_sect('X', bn, &slen);
    st = sx ? NULL : find_sect('T', bn, &slen);
    if (!sx && !st) return NULL;
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (sx && !g_mem && !GetEnvironmentVariableA("RLJP_NOMEM", NULL, 0)) g_mem = reserve_dos();     /* the program is being loaded: DOS memory is still free */
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) return buf;
    clean_old(bn);
    orig = read_all(orig_path, &olen);
    if (!orig) return NULL;
    out = sx ? build_exe(orig, olen, sx, slen, &nlen) : build_text(orig, olen, st, slen, &nlen);
    free(orig);
    if (!out) return NULL;
    if (!write_all(buf, out, nlen)) { free(out); return NULL; }
    free(out);
    dbg("built %s\n", buf);
    return buf;
}

static FILE *__cdecl hook_fopen(const char *name, const char *mode)
{
    char buf[MAX_PATH];
    const char *pp;
    FILE *f;
    if (!name || !g_enabled || (mode && strchr(mode, 'w'))) return real_fopen(name, mode);
    EnterCriticalSection(&g_cs);
    pp = patched_path(name, buf);
    if (pp && !install_callbacks()) pp = NULL;
    LeaveCriticalSection(&g_cs);
    if (pp) {
        f = real_fopen(pp, (mode && strchr(mode, '+')) ? "rb" : mode);
        if (f) { dbg("redirect %s -> %s (%s)\n", name, pp, mode); return f; }
    }
    return real_fopen(name, mode);
}

static int hook_iat(void)
{
    uint8_t *base = (uint8_t*)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *dh = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS *nh = (IMAGE_NT_HEADERS*)(base + dh->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *id = (IMAGE_IMPORT_DESCRIPTOR*)(base + dd->VirtualAddress);
    int done = 0;
    for (; id->Name; id++) {
        const char *dll = (const char*)(base + id->Name);
        IMAGE_THUNK_DATA *ot, *ft;
        if (_stricmp(dll, "msvcrt.dll")) continue;
        ot = (IMAGE_THUNK_DATA*)(base + (id->OriginalFirstThunk ? id->OriginalFirstThunk : id->FirstThunk));
        ft = (IMAGE_THUNK_DATA*)(base + id->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;
            if (ot->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME*)(base + ot->u1.AddressOfData);
            if (strcmp((const char*)ibn->Name, "fopen")) continue;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
            ft->u1.Function = (DWORD)(uintptr_t)hook_fopen;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            done = 1;
        }
    }
    return done;
}

static void init(HMODULE self)
{
    char la[MAX_PATH];
    HMODULE crt;
    InitializeCriticalSection(&g_cs);
    if (!GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) return;
    wsprintfA(g_dir, "%s\\PROPHET_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("Stone Prophet JP patch active, blob %08X\n", g_blob_crc);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        init(h);
    }
    return TRUE;
}
