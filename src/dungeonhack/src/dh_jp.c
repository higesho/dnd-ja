/*
 * Dungeon Hack Japanese patch - runtime part.
 * Loaded into DOSBox.exe (0.74-3, "Forgotten Realms: The Archives" build) as a winmm.dll proxy.
 *  - redirects fopen() of AESOP.EXE / HACK.RES / HACK.TBL / OPEN.RES / OPEN.TBL to patched copies
 *    built in %LOCALAPPDATA%\DHACK_JP (the patch carries only differences; the game files are not changed)
 *  - AESOP/16 (real mode) text module, segment 0x12fc of AESOP.EXE, gets three DOSBox callbacks:
 *      char width   (0x51ea)  -> width of Japanese characters
 *      char draw    (0x5236)  -> a Japanese glyph is put into a small "fake font" in DOS memory and the
 *                                game's own Mode X renderer draws it (colours/clipping stay the game's)
 *      print_buffer (0x5c40)  -> inserts line breaks for Japanese text (no spaces) before the game wraps
 *  Japanese text uses a private 2-byte code: lead 0x80-0xBF, trail 0xC0-0xFF.
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
static uint32_t g_dbx, g_dbx_code;   /* DOSBox build: Steam 0.74-3 = 0, GOG 0.74-2.1 = +0xe000 / -0x10 */
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
    };
    HMODULE h = GetModuleHandleA(NULL);
    int i;
    if ((uintptr_t)h != 0x400000) return 0;
    for (i = 0; i < 2; i++) {
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
static int g_cb_base = 0x70;
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
    if (memcmp(h->magic, "DHJP", 4)) return 0;
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
            g_glyph_index = (uint16_t*)calloc(65536, 2);
            for (k = 0; k < n; k++) g_glyph_index[codes[k]] = (uint16_t)(k + 1);
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0];
            g_logon = (int)v[1];
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

/* our scratch DOS memory (4 KB, reserved from the free MCB) */
static uint32_t g_mem;

/* AESOP/16 font: u16 count, u16 height, u16 ?, u16 xlat_used, u8 xlat[256], u16 offs[count] (at +0x108),
 * glyph: u16 width, height rows of width bytes (colour indices; 0 = background) */
static int big_font(uint32_t font) { return rw(font + 2) >= 12; }
static int jp_w(uint32_t font) { return big_font(font) ? 16 : 8; }
static int font_w(uint32_t font, uint8_t c)
{
    if (c >= rw(font)) return 0;
    return rw(font + rw(font + 0x108 + c * 2));
}
static int char_w(uint32_t font, uint8_t c)
{
    if (IS_LEAD(c)) return jp_w(font);
    if (IS_TRAIL(c)) return 0;
    return font_w(font, c);
}

/* text module data (CS-relative): window struct far ptr at cs:16d1, copy at cs:16d5 (13 words),
 * pane tables at cs:0a + pane*2 (+0x200 per field: x0 +0x40a, y0 +0x60a, x1 +0x80a, y1 +0xa0a) */
#define W_PTR    0x16d1
#define W_COPY   0x16d5
#define W_FONT   (W_COPY + 6)

static uint32_t cur_window(uint32_t cs) { return farptr(cs + W_PTR); }

/* CB: char width (far proc, arg ch at [sp+4]); patched entry "callback; retf" */
static uint32_t __cdecl cb_char_width(void)
{
    uint32_t cs = PHYS_CS, win = cur_window(cs), font = farptr(win + 6);
    uint8_t c = (uint8_t)rw(PHYS_SS + SP16 + 4);
    set16(&R_EAX, (uint16_t)char_w(font, c));
    return 0;
}

static uint8_t g_lead;
#define EXIT_IP 0x5472u

/* build a one-glyph font at g_mem with the header (colour table) of the current font */
static int make_fake(uint32_t cs, int gn)
{
    uint16_t foff = rw(cs + W_FONT), fseg = rw(cs + W_FONT + 2);
    uint32_t font = ((uint32_t)fseg << 4) + foff, B, k;
    int h = rw(font + 2), big = h >= 12, w = big ? 16 : 8, fh, r, x;
    uint16_t nseg;
    /* our copy must sit at the same offset as the real font (the wrap path reloads only the offset) */
    B = g_mem + 16 + (foff & 15);
    if (B < foff) return 0;
    nseg = (uint16_t)((B - foff) >> 4);
    if (((uint32_t)nseg << 4) + foff != B) return 0;
    for (k = 0; k < 0x108; k++) wb(B + k, rb(font + k));
    fh = big ? h : (h < 7 ? 7 : h);
    ww(B + 2, (uint16_t)fh);
    if (rw(B) < 2) ww(B, 2);
    ww(B + 0x108 + 2, 0x110);            /* glyph 1 at +0x110 (the offsets table only needs slot 1) */
    ww(B + 0x110, (uint16_t)w);
    {
        uint32_t g = B + 0x112;
        uint8_t ink = 15, out = 0;
        if (big) {                         /* the intro font: main colour 2 with a dark rim 1 */
            const uint8_t *bm = g_big + (gn - 1) * 28;
            uint8_t pix[20][16];
            int y0 = (h - 14) / 2;
            memset(pix, 0, sizeof pix);
            for (r = 0; r < 14; r++) for (x = 0; x < 14; x++)
                if ((bm[r * 2 + (x >> 3)] >> (7 - (x & 7))) & 1) pix[r + y0][x + 1] = 2;
            for (r = 0; r < fh && r < 20; r++) for (x = 0; x < 16; x++) {
                int dy, dx;
                if (pix[r][x]) continue;
                for (dy = -1; dy <= 1; dy++) for (dx = -1; dx <= 1; dx++) {
                    int yy = r + dy, xx = x + dx;
                    if (yy >= 0 && yy < 20 && xx >= 0 && xx < 16 && pix[yy][xx] == 2) pix[r][x] = 3;
                }
            }
            for (r = 0; r < fh && r < 20; r++) for (x = 0; x < 16; x++)
                wb(g + r * w + x, pix[r][x] == 2 ? 2 : pix[r][x] == 3 ? 1 : 0);
        } else {
            /* the small font (6x8, grown to 8 rows) printed on the whole-screen pane is used for lists with a
             * 6-pixel pitch: there the glyphs are squeezed to 6 rows */
            uint16_t pane = rw(cs + W_COPY);
            int pw = rws(cs + 0x80a + pane * 2) - rws(cs + 0x40a + pane * 2) + 1;
            const uint8_t *bm = (font_w(font, 'A') == 6 && pw >= 300) ? g_six + (gn - 1) * 8 : g_glyphs + (gn - 1) * 8;
            for (r = 0; r < fh; r++) {
                uint8_t bits = r < 8 ? bm[r] : 0;
                for (x = 0; x < 8; x++) wb(g + r * w + x, (bits & (0x80 >> x)) ? ink : out);
            }
        }
    }
    ww(cs + W_FONT + 2, nseg);
    return 1;
}

/* CB: inside char draw, replaces "mov bx,[16d5]" right after the window struct was copied */
static uint32_t __cdecl cb_char_draw(void)
{
    uint32_t cs = PHYS_CS, ss = PHYS_SS;
    uint8_t c = (uint8_t)rw(ss + BP16 + 6);
    if (IS_LEAD(c)) {
        g_lead = c;
        set16(&R_EIP, (uint16_t)EXIT_IP);
        return 0;
    }
    if (IS_TRAIL(c)) {
        int gn = g_lead ? glyph_no(g_lead, c) : 0;
        g_lead = 0;
        if (!gn || !g_mem || !make_fake(cs, gn)) { set16(&R_EIP, (uint16_t)EXIT_IP); return 0; }
        ww(ss + BP16 + 6, 1);
    } else g_lead = 0;
    set16(&R_EBX, rw(cs + W_COPY));      /* mov bx, [16d5] */
    return 0;
}

/* --------------------------------------------------- line breaking (prepass) */
#define MAXU 2400
typedef struct { uint16_t pos, len; int16_t w; uint8_t kind, fl; } unit;   /* kind: 0 ascii, 1 jp, 2 space, 3 newline */

static int ascii_no_start(uint8_t c) { return c == '.' || c == ',' || c == '!' || c == '?' || c == ')' || c == ':' || c == ';' || c == '%'; }

static uint8_t g_txt[MAXU + 8], g_out[MAXU * 2 + 8];
static unit g_u[MAXU];
static uint8_t g_brk[MAXU];

static void prepass(uint32_t cs, uint32_t win)
{
    uint32_t buf = farptr(win + 0x10), font = farptr(win + 6);
    uint16_t boff = rw(win + 0x10), pane = rw(win);
    int n = 0, nu = 0, i, k, jp = 0, xl, xr, x, line_first, last_brk, last_good, o;
    uint8_t c;
    for (n = 0; n < MAXU && (c = rb(buf + n)); n++) { g_txt[n] = c; if (c >= 0x80) jp = 1; }
    g_txt[n] = 0;
    if (!jp || n >= MAXU) return;
    for (i = 0; i < n; ) {
        unit *u = &g_u[nu];
        c = g_txt[i];
        u->pos = (uint16_t)i; u->fl = 0;
        if (IS_LEAD(c) && IS_TRAIL(g_txt[i + 1])) {
            int gn = glyph_no(c, g_txt[i + 1]);
            u->len = 2; u->kind = 1; u->w = (int16_t)jp_w(font);
            u->fl = gn ? g_gflags[gn - 1] : 0;
        } else {
            u->len = 1; u->kind = c == '\n' ? 3 : c == ' ' ? 2 : 0;
            u->w = (int16_t)(c == '\n' || c == '\r' ? 0 : char_w(font, c));
        }
        i += u->len; nu++;
    }
    xl = rws(cs + 0x40a + pane * 2); xr = rws(cs + 0x80a + pane * 2) + 1;
    x = rws(win + 2);
    memset(g_brk, 0, nu);
    line_first = 0; last_brk = -1; last_good = -1;
    for (k = 0; k < nu; k++) {
        unit *u = &g_u[k];
        if (u->kind == 3) { x = xl; line_first = k + 1; last_brk = -1; last_good = -1; continue; }
        if (k > line_first) {
            unit *p = &g_u[k - 1];
            int ok;
            if (u->kind == 2) ok = 0;
            else if (p->kind == 2) ok = 1;
            else if (u->kind == 1 || p->kind == 1)
                ok = !(u->kind == 1 && (u->fl & 1)) && !(u->kind == 0 && ascii_no_start(g_txt[u->pos]))
                     && !(p->kind == 1 && (p->fl & 2));
            else ok = 0;
            if (ok) {
                last_brk = k;
                /* a "good" break: after punctuation, at the end of a kana run (not between a kanji and its okurigana) */
                if (p->kind != 1 || u->kind != 1 || (p->fl & 32) || ((p->fl & 4) && !(u->fl & 4)) ||
                    ((p->fl & 8) && !(u->fl & 8)))
                    last_good = k;
            }
        } else if (k == line_first && x > xl && u->kind == 1 && !(u->fl & 1)) {
            last_brk = k;   /* continuing a line after a name: may break before the first char */
        }
        if (u->kind != 2 && x + u->w > xr && last_brk >= 0) {
            int j;
            if (last_good > line_first && k - last_good <= 6) last_brk = last_good;
            g_brk[last_brk] = 1;
            line_first = last_brk;
            last_brk = -1; last_good = -1;
            x = xl;
            for (j = line_first; j < k; j++) if (!(g_u[j].kind == 2 && j == line_first)) x += g_u[j].w;
            while (line_first < k && g_u[line_first].kind == 2) line_first++;
        }
        x += u->w;
    }
    o = 0;
    for (k = 0; k < nu; k++) {
        unit *u = &g_u[k];
        if (g_brk[k]) {
            while (o > 0 && g_out[o - 1] == ' ') o--;
            g_out[o++] = '\n';
        }
        if (u->kind == 2 && o > 0 && g_out[o - 1] == '\n' && (g_brk[k] || (k > 0 && g_brk[k - 1]))) continue;
        memcpy(g_out + o, g_txt + u->pos, u->len); o += u->len;
    }
    g_out[o] = 0;
    if (o >= 2390) return;   /* buffer is 2400 bytes (+ "wi" guard) */
    for (i = 0; i <= o; i++) wb(buf + i, g_out[i]);
    ww(win + 0x14, (uint16_t)(boff + o));
}

/* CB: print_buffer entry, replaces "push bp; mov bp,sp; push ds" */
static uint32_t __cdecl cb_print_buffer(void)
{
    uint32_t cs = PHYS_CS, win = cur_window(cs);
    if (g_logon) {
        char t[200]; int i; uint32_t b = farptr(win + 0x10);
        for (i = 0; i < 199 && (t[i] = (char)rb(b + i)); i++); t[i] = 0;
        dbg("print_buffer pane=%d x=%d y=%d font=%x [%s]\n", rw(win), rws(win + 2), rws(win + 4), farptr(win + 6), t);
    }
    prepass(cs, win);
    push16(BP16);                 /* push bp */
    set16(&R_EBP, SP16);          /* mov bp, sp */
    push16(SEGV(3));              /* push ds */
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    if (g_cb_installed) return 1;
    for (i = 0; i < 3; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy (%x)\n", b + i, tab[b + i]); return 0; }
    tab[b + 0] = (uint32_t)cb_char_width;
    tab[b + 1] = (uint32_t)cb_char_draw;
    tab[b + 2] = (uint32_t)cb_print_buffer;
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

/* ------------------------------------------- DOS memory for the fake font */
/* walk the MCB chain and carve 4 KB (+MCB) off the end of a free block, owned by DOS (8), named "DHJP" */
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
        if (own == 8 && size == 0x100 && rb(s * 16 + 8) == 'D' && rb(s * 16 + 9) == 'H' && rb(s * 16 + 10) == 'J' && rb(s * 16 + 11) == 'P')
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
        memcpy(mcb + 8, "DHJP", 4);
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

/* directory entry (file offset of the offset field) of resource idx, 0 if none */
static uint32_t res_dirent(const uint8_t *res, uint32_t rlen, uint32_t idx)
{
    uint32_t b = *(const uint32_t*)(res + 0x18), k;
    for (k = 0; k < idx / 128; k++) {
        if (!b || b + 4 > rlen) return 0;
        b = *(const uint32_t*)(res + b);
    }
    if (!b || b + 4 + 128 + 512 > rlen) return 0;
    return b + 4 + 128 + (idx % 128) * 4;
}

/* AESOP/16 font with more rows: the extra rows are blank (gives Japanese glyphs room and a wider line pitch) */
static uint8_t *grow_font(const uint8_t *f, uint32_t flen, uint32_t newh, uint32_t *outlen)
{
    uint32_t cnt, h, k, o, need;
    uint8_t *out;
    if (flen < 0x108) return NULL;
    cnt = *(const uint16_t*)f; h = *(const uint16_t*)(f + 2);
    if (newh <= h || 0x108 + cnt * 2 > flen) return NULL;
    need = 0x108 + cnt * 2;
    for (k = 0; k < cnt; k++) {
        uint32_t go = *(const uint16_t*)(f + 0x108 + k * 2), w;
        if (go + 2 > flen) return NULL;
        w = *(const uint16_t*)(f + go);
        if (go + 2 + w * h > flen) return NULL;
        need += 2 + w * newh;
    }
    if (need > 0xfff0) return NULL;
    out = (uint8_t*)calloc(need, 1);
    memcpy(out, f, 0x108);
    *(uint16_t*)(out + 2) = (uint16_t)newh;
    o = 0x108 + cnt * 2;
    for (k = 0; k < cnt; k++) {
        uint32_t go = *(const uint16_t*)(f + 0x108 + k * 2), w = *(const uint16_t*)(f + go);
        *(uint16_t*)(out + 0x108 + k * 2) = (uint16_t)o;
        *(uint16_t*)(out + o) = (uint16_t)w;
        memcpy(out + o + 2, f + go + 2, w * h);
        o += 2 + w * newh;
    }
    *outlen = need;
    return out;
}

/* 'R' section: u32 orig_size, u32 orig_crc, u32 n, n * {u32 index, u32 len, bytes},
 *   then u32 nf, nf * {u32 index, u32 new_height} fonts to grow.
 * The replaced resources are appended at the end of the file and their directory entries repointed.
 * The .TBL file (u32 offset per resource) is rebuilt with the same new offsets. */
typedef struct { uint32_t idx, len; const uint8_t *data; uint8_t *own; } rrep;
static uint8_t *build_res(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen,
                          uint8_t *tbl, uint32_t tlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], n = ((const uint32_t*)sec)[2];
    const uint8_t *p = sec + 12, *e = sec + slen;
    uint32_t total = olen, i, pos, nr = 0, nf = 0;
    uint8_t *out;
    rrep *rp;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("res mismatch\n"); return NULL; }
    rp = (rrep*)calloc(n + 32, sizeof(rrep));
    for (i = 0; i < n && p + 8 <= e; i++) {
        rp[nr].idx = ((const uint32_t*)p)[0]; rp[nr].len = ((const uint32_t*)p)[1]; rp[nr].data = p + 8; nr++;
        p += 8 + ((const uint32_t*)p)[1];
    }
    if (p + 4 <= e) { nf = *(const uint32_t*)p; p += 4; }
    for (i = 0; i < nf && i < 32 && p + 8 <= e; i++, p += 8) {
        uint32_t idx = ((const uint32_t*)p)[0], nh = ((const uint32_t*)p)[1], ent = res_dirent(orig, olen, idx), ro, rl, l;
        uint8_t *b;
        if (!ent || !(ro = *(const uint32_t*)(orig + ent)) || ro + 12 > olen || ro + 12 + (rl = *(const uint32_t*)(orig + ro + 8)) > olen) continue;
        b = grow_font(orig + ro + 12, rl, nh, &l);
        if (!b) { dbg("font %u not grown\n", idx); continue; }
        rp[nr].idx = idx; rp[nr].len = l; rp[nr].data = b; rp[nr].own = b; nr++;
    }
    for (i = 0; i < nr; i++) total += 12 + rp[i].len;
    out = (uint8_t*)malloc(total);
    memcpy(out, orig, olen);
    pos = olen;
    for (i = 0; i < nr; i++) {
        uint32_t idx = rp[i].idx, len = rp[i].len;
        uint32_t ent = res_dirent(out, olen, idx), oldoff;
        if (!ent || !(oldoff = *(uint32_t*)(out + ent)) || oldoff + 12 > olen) { free(out); out = NULL; break; }
        memcpy(out + pos, orig + oldoff, 8);            /* timestamp, attributes */
        *(uint32_t*)(out + pos + 8) = len;
        memcpy(out + pos + 12, rp[i].data, len);
        *(uint32_t*)(out + ent) = pos;
        if (tbl && (idx + 1) * 4 <= tlen && *(uint32_t*)(tbl + idx * 4) == oldoff) *(uint32_t*)(tbl + idx * 4) = pos;
        pos += 12 + len;
    }
    for (i = 0; i < nr; i++) free(rp[i].own);
    free(rp);
    if (!out) return NULL;
    *(uint32_t*)(out + 0x10) = total;                     /* file_size */
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

/* builds <crc>_X.RES and <crc>_X.TBL together */
static int build_res_pair(const char *res_path, const char *bn_res, const uint8_t *sr, uint32_t slen)
{
    char tpath[MAX_PATH], bn_tbl[64], out_res[MAX_PATH], out_tbl[MAX_PATH];
    uint8_t *orig, *tbl, *out;
    uint32_t olen, tlen = 0, nlen;
    int ok = 0;
    lstrcpynA(tpath, res_path, MAX_PATH);
    lstrcpyA(tpath + lstrlenA(tpath) - 3, "TBL");
    lstrcpynA(bn_tbl, bn_res, 60);
    lstrcpyA(bn_tbl + lstrlenA(bn_tbl) - 3, "TBL");
    wsprintfA(out_res, "%s\\%08X_%s", g_dir, g_blob_crc, bn_res);
    wsprintfA(out_tbl, "%s\\%08X_%s", g_dir, g_blob_crc, bn_tbl);
    clean_old(bn_res); clean_old(bn_tbl);
    orig = read_all(res_path, &olen);
    if (!orig) return 0;
    tbl = read_all(tpath, &tlen);
    dbg("building %s (%u bytes)\n", bn_res, olen);
    out = build_res(orig, olen, sr, slen, &nlen, tbl, tlen);
    free(orig);
    if (out && (!tbl || write_all(out_tbl, tbl, tlen)) && write_all(out_res, out, nlen)) ok = 1;
    free(out); free(tbl);
    dbg("built %s: %d\n", out_res, ok);
    return ok;
}

static const char *patched_path(const char *orig_path, char *buf)
{
    const char *bn = base_name(orig_path);
    uint32_t slen, olen, nlen;
    const uint8_t *sx, *sr;
    char rn[64];
    int is_tbl = 0, l = lstrlenA(bn);
    if (!g_enabled || l < 5 || l > 60) return NULL;
    lstrcpyA(rn, bn);
    if (!_stricmp(rn + l - 4, ".TBL")) { lstrcpyA(rn + l - 3, "RES"); is_tbl = 1; }
    sx = find_sect('X', rn, &slen);
    sr = sx ? NULL : find_sect('R', rn, &slen);
    if (!sx && !sr) return NULL;
    if (sx && is_tbl) return NULL;
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (sx && !g_mem) g_mem = reserve_dos();     /* AESOP.EXE is being loaded: memory is still free */
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) return buf;
    if (sr) {
        char rp[MAX_PATH];
        lstrcpynA(rp, orig_path, MAX_PATH);
        if (is_tbl) lstrcpyA(rp + lstrlenA(rp) - 3, "RES");
        if (!build_res_pair(rp, rn, sr, slen)) return NULL;
        return GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES ? buf : NULL;
    }
    {
        uint8_t *orig, *out;
        clean_old(bn);
        orig = read_all(orig_path, &olen);
        if (!orig) return NULL;
        out = build_exe(orig, olen, sx, slen, &nlen);
        free(orig);
        if (!out) return NULL;
        if (!write_all(buf, out, nlen)) { free(out); return NULL; }
        free(out);
        dbg("built %s\n", buf);
    }
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
    wsprintfA(g_dir, "%s\\DHACK_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("Dungeon Hack JP patch active, blob %08X\n", g_blob_crc);
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
