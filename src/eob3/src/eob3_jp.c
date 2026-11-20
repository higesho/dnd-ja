/*
 * Eye of the Beholder III Japanese patch - runtime part.
 * Loaded into DOSBox.exe (0.74-3, "Forgotten Realms: The Archives" build) as a winmm.dll proxy.
 *  - redirects fopen() of INTERP.EXE / EYE.RES to patched copies built in %LOCALAPPDATA%\EOB3_JP
 *    (EYE.RES: translated "S:" string resources; INTERP.EXE: three text routines call DOSBox callbacks)
 *  - callbacks (32-bit protected mode, DOS/32A flat model):
 *      VFX_character_width(font, ch)            -> Japanese width
 *      VFX_character_draw(pane,x,y,font,ch,xlat) -> draws Japanese glyphs (Misaki 8x8)
 *      GIL2VFXA_print_buffer(pane, maxlines)    -> inserts line breaks for Japanese (kinsoku) first
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
static __inline uint32_t rd(uint32_t a) { return rb(a) | (rb(a + 1) << 8) | (rb(a + 2) << 16) | ((uint32_t)rb(a + 3) << 24); }
static __inline int32_t rds(uint32_t a) { return (int32_t)rd(a); }
static __inline void wd(uint32_t a, uint32_t v) { wb(a, (uint8_t)v); wb(a + 1, (uint8_t)(v >> 8)); wb(a + 2, (uint8_t)(v >> 16)); wb(a + 3, (uint8_t)(v >> 24)); }
#define REG32(i) (((uint32_t*)A_REGS)[i])
#define R_EAX REG32(0)
#define R_ESP REG32(4)
#define R_EBP REG32(5)
#define R_EIP REG32(8)
#define SEGV(i) (((uint16_t*)A_SEGVAL)[i])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
/* guest stack / data: DOS/32A flat model, bases are 0 but keep them for safety */
#define STK(off) (PHYS_SS + R_ESP + (off))
static __inline void push32(uint32_t v) { R_ESP -= 4; wd(PHYS_SS + R_ESP, v); }

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
static int g_cb_base = 0x70;
static const uint8_t *g_tab;     /* 'T': u32 n, n * {u8 flags, u16 enlen, en, u16 jalen, ja} */
static uint32_t g_tab_n;
static int g_enabled;
/* INTERP.EXE layout (link addresses as in our LE dump: code object at 0x10000) */
static uint32_t A_PB_AFTER = 0x14ef4, A_CW_AFTER = 0x22ce7, A_CD_AFTER = 0x22d07, A_TWPTR_REF = 0x14d01;

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
    if (memcmp(h->magic, "EJP3", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len);
    p = g_blob + sizeof(blob_hdr);
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == 'F') {     /* font: u32 n; n*u16 code; n*8 bitmap; n flags */
            uint32_t n = *(const uint32_t*)d, k;
            const uint16_t *codes = (const uint16_t*)(d + 4);
            g_glyphs = d + 4 + n * 2;
            g_gflags = g_glyphs + n * 8;
            g_glyph_index = (uint16_t*)calloc(65536, 2);
            for (k = 0; k < n; k++) g_glyph_index[codes[k]] = (uint16_t)(k + 1);
        } else if (s->type == 'T') {
            g_tab_n = *(const uint32_t*)d;
            g_tab = d + 4;
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

static const uint8_t *glyph(uint8_t lead, uint8_t trail, int *flags)
{
    uint16_t gi = g_glyph_index[(lead << 8) | trail];
    if (!gi) { if (flags) *flags = 0; return NULL; }
    if (flags) *flags = g_gflags[gi - 1];
    return g_glyphs + (gi - 1) * 8;
}

/* width of a Japanese character for a given VFX font */
static int jp_w(uint32_t font)
{
    int h = rds(font + 8);
    return h >= 8 ? 8 : 7;
}
static int font_w(uint32_t font, uint8_t c)
{
    uint32_t n = rd(font + 4);
    if (c >= n) return 0;
    return rds(font + rd(font + 0x10 + c * 4));
}

/* CB: VFX_character_width(font, ch) -> eax ; patched as "callback; ret" */
static uint32_t __cdecl cb_char_width(void)
{
    uint32_t font = rd(STK(4));
    uint8_t c = (uint8_t)rd(STK(8));
    if (IS_LEAD(c)) R_EAX = jp_w(font);
    else if (IS_TRAIL(c)) R_EAX = 0;
    else R_EAX = font_w(font, c);
    return 0;
}

static uint8_t g_lead;

static void draw_glyph(uint32_t pane, int x, int y, uint32_t font, const uint8_t *g, uint32_t xlat)
{
    uint32_t win = rd(pane);
    int px0 = rds(pane + 4), py0 = rds(pane + 8), px1 = rds(pane + 12), py1 = rds(pane + 16);
    int wx0 = rds(win + 4), wy0 = rds(win + 8), wx1 = rds(win + 12), wy1 = rds(win + 16);
    int cx0 = px0 > wx0 ? px0 : wx0, cy0 = py0 > wy0 ? py0 : wy0;
    int cx1 = px1 < wx1 ? px1 : wx1, cy1 = py1 < wy1 ? py1 : wy1;
    uint32_t buf = rd(win), pitch = (uint32_t)(wx1 + 1 - wx0);
    int ax = x + px0, ay = y + py0, w = jp_w(font), fh = rds(font + 8), gx, gy, rows;
    uint8_t ink = 15, bg = 0;
    int draw_bg = 1;
    if (xlat) {
        ink = rb(xlat + 15);
        bg = rb(xlat + 0);
        draw_bg = bg != 0xff;
    }
    rows = fh > 8 ? fh : 8;
    for (gy = 0; gy < rows; gy++) {
        int yy = ay + gy;
        uint8_t bits = gy < 8 ? g[gy] : 0;
        if (yy < cy0 || yy > cy1) continue;
        if (gy >= fh && !(bits)) continue;   /* below the font cell: only ink */
        for (gx = 0; gx < w; gx++) {
            int xx = ax + gx;
            int on = gx < 8 && (bits & (0x80 >> gx));
            if (xx < cx0 || xx > cx1) continue;
            if (on) { if (ink != 0xff) wb(buf + yy * pitch + xx, ink); }
            else if (draw_bg && gy < fh) wb(buf + yy * pitch + xx, bg);
        }
    }
}

/* CB: VFX_character_draw(pane, x, y, font, ch, xlat) -> eax width.
 * Patched entry: "callback; nop; nop" replacing "push ebp; mov ebp,esp; add esp,-24h". */
static uint32_t __cdecl cb_char_draw(void)
{
    uint8_t c = (uint8_t)rd(STK(0x14));
    if (c < 0x80) {
        uint32_t ebp = R_EBP;
        g_lead = 0;
        push32(ebp);          /* push ebp */
        R_EBP = R_ESP;        /* mov ebp, esp */
        R_ESP -= 0x24;        /* add esp, -24h */
        return 0;             /* continue with the original body */
    }
    {
        uint32_t pane = rd(STK(4)), font = rd(STK(0x10)), xlat = rd(STK(0x18));
        int x = rds(STK(8)), y = rds(STK(0xc));
        uint32_t ret = rd(STK(0));
        if (IS_LEAD(c)) {
            g_lead = c;
            R_EAX = 0;
        } else {
            const uint8_t *g = g_lead ? glyph(g_lead, c, NULL) : NULL;
            if (g) draw_glyph(pane, x, y, font, g, xlat);
            g_lead = 0;
            R_EAX = jp_w(font);
        }
        R_EIP = ret;          /* ret */
        R_ESP += 4;
    }
    return 0;
}

/* --------------------------------------------------- line breaking (prepass) */
#define MAXU 2400
typedef struct { uint16_t pos, len; int16_t w; uint8_t kind, fl; } unit;   /* kind: 0 ascii, 1 jp, 2 space, 3 newline */

static int ascii_no_start(uint8_t c) { return c == '.' || c == ',' || c == '!' || c == '?' || c == ')' || c == ':' || c == ';' || c == '%'; }

static uint8_t g_txt[MAXU + 8], g_out[MAXU * 2 + 8];
static unit g_u[MAXU];
static uint8_t g_brk[MAXU];

static void prepass(uint32_t tw, uint32_t pane)
{
    uint32_t buf = rd(tw + 0x18), font = rd(tw + 0xc);
    int n = 0, nu = 0, i, k, jp = 0, xl, xr, x, line_first, last_brk, o;
    uint8_t c;
    for (n = 0; n < MAXU && (c = rb(buf + n)); n++) { g_txt[n] = c; if (c >= 0x80) jp = 1; }
    g_txt[n] = 0;
    /* inline "." / "\"" printed alone after a character name (script literals) */
    if (n == 1 && (g_txt[0] == '.' || g_txt[0] == '"')) {
        const uint8_t *g;
        uint16_t code = 0, k2;
        /* find the codes of 。 and 「 : stored as glyph flags 4 / 8 */
        for (k2 = 0; k2 < 0xffff && !code; k2++) {
            uint16_t gi = g_glyph_index[k2];
            if (gi && (g_gflags[gi - 1] & (g_txt[0] == '.' ? 4 : 8))) code = k2;
        }
        (void)g;
        if (code) {
            wb(buf, (uint8_t)(code >> 8)); wb(buf + 1, (uint8_t)code); wb(buf + 2, 0);
            wd(tw + 0x1c, buf + 2);
        }
        return;
    }
    if (!jp || n >= MAXU) return;
    for (i = 0; i < n; ) {
        unit *u = &g_u[nu];
        c = g_txt[i];
        u->pos = (uint16_t)i; u->fl = 0;
        if (IS_LEAD(c) && IS_TRAIL(g_txt[i + 1])) {
            int fl;
            u->len = 2; u->kind = 1; u->w = (int16_t)jp_w(font);
            glyph(c, g_txt[i + 1], &fl); u->fl = (uint8_t)fl;
        } else {
            u->len = 1; u->kind = c == '\n' ? 3 : c == ' ' ? 2 : 0;
            u->w = (int16_t)(c == '\n' || c == '\r' ? 0 : font_w(font, c));
            if (c >= 0x80) u->w = 0;
        }
        i += u->len; nu++;
    }
    xl = rds(pane + 4); xr = rds(pane + 12) + 1;
    x = rds(tw + 4);
    memset(g_brk, 0, nu);
    line_first = 0; last_brk = -1;
    for (k = 0; k < nu; k++) {
        unit *u = &g_u[k];
        if (u->kind == 3) { x = xl; line_first = k + 1; last_brk = -1; continue; }
        if (k > line_first) {
            unit *p = &g_u[k - 1];
            int ok;
            if (u->kind == 2) ok = 0;
            else if (p->kind == 2) ok = 1;
            else if (u->kind == 1 || p->kind == 1)
                ok = !(u->kind == 1 && (u->fl & 1)) && !(u->kind == 0 && ascii_no_start(g_txt[u->pos]))
                     && !(p->kind == 1 && (p->fl & 2));
            else ok = 0;
            if (ok) last_brk = k;
        } else if (k == line_first && x > xl && u->kind == 1 && !(u->fl & 1)) {
            last_brk = k;   /* continuing a line after a name: may break before the first char */
        }
        if (u->kind != 2 && x + u->w > xr && last_brk >= 0) {
            int j;
            g_brk[last_brk] = 1;
            line_first = last_brk;
            last_brk = -1;
            x = xl;
            for (j = line_first; j < k; j++) if (!(g_u[j].kind == 2 && j == line_first)) x += g_u[j].w;
            /* skip leading spaces of the new line */
            while (line_first < k && g_u[line_first].kind == 2) { x -= 0; line_first++; }
        }
        x += u->w;
    }
    /* rebuild */
    o = 0;
    for (k = 0; k < nu; k++) {
        unit *u = &g_u[k];
        if (g_brk[k]) {
            /* drop trailing spaces before the break, then newline; leading spaces after it are dropped too */
            while (o > 0 && g_out[o - 1] == ' ') o--;
            g_out[o++] = '\n';
        }
        if (u->kind == 2 && o > 0 && g_out[o - 1] == '\n' && k > 0 && (g_brk[k] || (k > 0 && g_brk[k - 1]))) continue;
        memcpy(g_out + o, g_txt + u->pos, u->len); o += u->len;
    }
    g_out[o] = 0;
    if (o >= 2390) return;   /* buffer is 2400 bytes (+ "wi" guard) */
    for (i = 0; i <= o; i++) wb(buf + i, g_out[i]);
    wd(tw + 0x1c, buf + o);
}

/* CB: GIL2VFX_print_buffer(maxlines) wrapper at link 0x14ef0, entry "mov edx,[esp+4]" replaced by the callback.
 * tw = *[0x36d28] (operand at 0x14ef7), pane = 0x355b8 (operand at 0x14f0e) + tw->pane * 20. */
static uint32_t g_codebase;
static uint32_t __cdecl cb_print_buffer(void)
{
    uint32_t twv, tw;
    g_codebase = R_EIP - A_PB_AFTER;      /* linear address of link address 0 */
    twv = rd(g_codebase + 0x14ef7);
    tw = rd(twv);
    if (tw) {
        uint32_t pane = rd(g_codebase + 0x14f0e) + rd(tw) * 20;
        if (g_logon) { char t[200]; int i; uint32_t b = rd(tw + 0x18); for (i = 0; i < 199 && (t[i] = (char)rb(b + i)); i++); t[i] = 0;
            dbg("print_buffer tw=%x pane=%x htab=%d vtab=%d font=%x just=%d [%s]\n", tw, pane, rds(tw + 4), rds(tw + 8), rd(tw + 0xc), rd(tw + 0x20), t); }
        prepass(tw, pane);
    }
    REG32(2) = rd(STK(4));                /* mov edx, [esp+4] */
    return 0;
}

/* ------------------------------------------- character generation (CHGEN.EXE, real mode) */
/* MGA.OVL Print(char far *s, int x, int y, int fg, int bg) is replaced by "callback; retf".
 * The English string is looked up in the 'T' table and drawn in Japanese. */
#define PHYS_CS (((uint32_t*)A_SEGPHYS)[1])
static __inline uint16_t rw(uint32_t a) { return (uint16_t)(rb(a) | (rb(a + 1) << 8)); }
static __inline int16_t rws(uint32_t a) { return (int16_t)rw(a); }
static __inline uint32_t farptr(uint32_t a) { return ((uint32_t)rw(a + 2) << 4) + rw(a); }


static const uint8_t *tr_lookup(const uint8_t *s, int n, int *jalen, int *flags)
{
    /* entries: u8 flags, u16 english length, u32 crc32 of the English text, u16 jalen, ja */
    const uint8_t *p = g_tab;
    uint32_t i, h;
    if (!p) return NULL;
    h = crc32(s, (uint32_t)n);
    for (i = 0; i < g_tab_n; i++) {
        uint8_t fl = p[0];
        uint16_t el = *(const uint16_t*)(p + 1), jl = *(const uint16_t*)(p + 7);
        if (el == n && *(const uint32_t*)(p + 3) == h) { *jalen = jl; *flags = fl; return p + 9; }
        p += 9 + jl;
    }
    return NULL;
}

typedef struct { uint32_t dest, font; int fw, fh; uint8_t fg, bg; } drawctx;

static __inline void px(const drawctx *c, int x, int y, int on)
{
    if (x < 0 || x >= 320 || y < 0 || y >= 200) return;
    if (on) { if (c->fg) wb(c->dest + y * 320 + x, c->fg); }
    else if (c->bg) wb(c->dest + y * 320 + x, c->bg);
}

static void draw_half(const drawctx *c, uint8_t ch, int x, int y)
{
    uint32_t g = c->font + rw(c->font + 2 + (ch & 0x7f) * 2);
    int bw = (c->fw + 7) / 8, r, k;
    for (r = 0; r < c->fh; r++)
        for (k = 0; k < c->fw; k++)
            px(c, x + k, y + r, (rb(g + r * bw + k / 8) >> (7 - (k & 7))) & 1);
}

static uint32_t __cdecl cb_chg_print(void)
{
    uint32_t sp = PHYS_SS + (uint16_t)R_ESP, cs = PHYS_CS, sa, fp;
    int x, y, x0, n = 0, i, jl = 0, fl = 0, jw;
    static uint8_t buf[512];
    const uint8_t *s;
    drawctx c;
    sa = farptr(sp + 4);
    x = rws(sp + 8); y = rws(sp + 10);
    c.fg = rb(sp + 12); c.bg = rb(sp + 14);
    while (n < (int)sizeof buf - 1 && (buf[n] = rb(sa + n)) != 0) n++;
    buf[n] = 0;
    if (g_logon) dbg("chg_print %d,%d fg=%d bg=%d [%s]\n", x, y, c.fg, c.bg, buf);
    s = tr_lookup(buf, n, &jl, &fl);
    if (!s) { s = buf; jl = n; }
    fp = farptr(cs + 3);
    c.font = farptr(fp);
    c.fh = rb(c.font + 0x102);
    c.fw = rb(c.font + 0x103);
    {
        uint32_t pg = farptr(cs + 0x0b), tab = farptr(cs + 7);
        c.dest = (uint32_t)rw(tab + (rb(pg) & 0x1e) * 2) << 4;
    }
    if (c.fw <= 0 || c.fh <= 0) return 0;
    jw = c.fw + 2 > 8 ? c.fw + 2 : 8;     /* width of a Japanese character */
    if (fl & 1) {                          /* centred: keep the centre of the English text */
        int ew = n * c.fw, w = 0;
        for (i = 0; i < jl; i++) w += (IS_LEAD(s[i]) && i + 1 < jl && IS_TRAIL(s[i + 1])) ? (i++, jw) : c.fw;
        x += (ew - w) / 2;
    }
    if (x < 0) x = 0;
    x0 = x;
    for (i = 0; i < jl;) {
        uint8_t ch = s[i];
        if (ch == 0x0d) { x = x0; y += c.fh > 8 ? c.fh : 8; i++; continue; }
        if (IS_LEAD(ch) && i + 1 < jl && IS_TRAIL(s[i + 1])) {
            const uint8_t *g = glyph(ch, s[i + 1], NULL);
            int r, k;
            if (g) for (r = 0; r < 8; r++) for (k = 0; k < jw; k++) {
                int on = k < 8 && ((g[r] >> (7 - k)) & 1), yy = r + (c.fh < 8 ? -1 : 0);
                if (on || (yy >= 0 && yy < c.fh)) px(&c, x + k, y + yy, on);
            }
            x += jw; i += 2;
            continue;
        }
        draw_half(&c, ch, x, y);
        x += c.fw;
        i++;
    }
    return 0;
}

static int g_ncalls;
#define WRAP(name) static uint32_t __cdecl w_##name(void) { uint32_t r, a1 = rd(STK(4)), a2 = rd(STK(8)), a5 = rd(STK(0x14)); \
    r = name(); \
    if (g_ncalls < 300) dbg("cb %s a=%x,%x,%x -> eax=%x esp=%x eip=%x\n", #name, a1, a2, a5, R_EAX, R_ESP, R_EIP); \
    g_ncalls++; return r; }
WRAP(cb_char_width) WRAP(cb_char_draw) WRAP(cb_print_buffer)

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    if (g_cb_installed) return 1;
    for (i = 0; i < 4; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy (%x)\n", b + i, tab[b + i]); return 0; }
    tab[b + 0] = (uint32_t)(g_logon ? w_cb_char_width : cb_char_width);
    tab[b + 1] = (uint32_t)(g_logon ? w_cb_char_draw : cb_char_draw);
    tab[b + 2] = (uint32_t)(g_logon ? w_cb_print_buffer : cb_print_buffer);
    tab[b + 3] = (uint32_t)cb_chg_print;
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
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

/* ---- VFX shape ("1.10") editing: erase English lettering baked into a picture and draw Japanese.
 * The picture itself comes from the user's EYE.RES; the patch only carries the edit operations. */
static uint8_t *edit_shape(const uint8_t *res, uint32_t rlen, uint32_t idx, const uint8_t *ops, uint32_t oplen, uint32_t *outlen)
{
    uint32_t ent = res_dirent(res, rlen, idx), off, so, len, w, h, x, y, pos, o;
    const uint8_t *d, *q, *qe;
    uint8_t *pix, *msk, *out;
    if (!ent) return NULL;
    off = *(const uint32_t*)(res + ent);
    if (off + 12 > rlen) return NULL;
    len = *(const uint32_t*)(res + off + 8);
    d = res + off + 12;
    if (off + 12 + len > rlen || len < 40 || memcmp(d, "1.10", 4) || *(const uint32_t*)(d + 4) != 1) return NULL;
    so = *(const uint32_t*)(d + 8);
    if (so + 24 > len) return NULL;
    h = *(const uint16_t*)(d + so) + 1u; w = *(const uint16_t*)(d + so + 2) + 1u;
    pix = (uint8_t*)calloc(w * h, 1); msk = (uint8_t*)calloc(w * h, 1);
    pos = so + 24;
    for (y = 0; y < h; y++) {
        x = 0;
        while (pos < len) {
            uint8_t m = d[pos++], a = m >> 1, i;
            if (!m) break;
            if (m == 1) { x += d[pos++]; continue; }
            for (i = 0; i < a; i++, x++) {
                uint8_t v = (m & 1) ? d[pos++] : d[pos];
                if (x < w) { pix[y * w + x] = v; msk[y * w + x] = 1; }
            }
            if (!(m & 1)) pos++;
        }
    }
    /* operations */
    for (q = ops, qe = ops + oplen; q < qe; ) {
        uint8_t t = *q++;
        if (t == 1) {            /* erase: x0 y0 x1 y1 (i16), nc, colors */
            int x0 = ((const int16_t*)q)[0], y0 = ((const int16_t*)q)[1], x1 = ((const int16_t*)q)[2], y1 = ((const int16_t*)q)[3];
            uint8_t nc = q[8], set[256];
            const uint8_t *cl = q + 9;
            int xx, yy, k;
            memset(set, 0, sizeof set);
            for (k = 0; k < nc; k++) set[cl[k]] = 1;
            q += 9 + nc;
            for (yy = y0; yy <= y1 && yy < (int)h; yy++) {
                uint8_t row[1024];
                if (yy < 0 || w > 1024) continue;
                memcpy(row, pix + yy * w, w);
                for (xx = x0; xx <= x1 && xx < (int)w; xx++) {
                    int dd;
                    if (xx < 0 || !set[row[xx]]) continue;
                    for (dd = 1; dd < 64; dd++) {
                        if (xx - dd >= 0 && xx - dd >= x0 - 8 && !set[row[xx - dd]]) { pix[yy * w + xx] = row[xx - dd]; break; }
                        if (xx + dd < (int)w && xx + dd <= x1 + 8 && !set[row[xx + dd]]) { pix[yy * w + xx] = row[xx + dd]; break; }
                    }
                }
            }
        } else if (t == 2) {     /* text: x y (i16), ink, shadow(0xff none), n, codes (u16) */
            int tx = ((const int16_t*)q)[0], ty = ((const int16_t*)q)[1], k, gx, gy, pass;
            uint8_t ink = q[4], sh = q[5], n = q[6];
            const uint16_t *codes = (const uint16_t*)(q + 7);
            q += 7 + n * 2;
            for (pass = 0; pass < 2; pass++) {
                uint8_t c = pass ? ink : sh;
                int ox = pass ? 0 : 1, oy = pass ? 0 : 1;
                if (c == 0xff) continue;
                for (k = 0; k < n; k++) {
                    const uint8_t *g = glyph((uint8_t)(codes[k] >> 8), (uint8_t)codes[k], NULL);
                    if (!g) continue;
                    for (gy = 0; gy < 8; gy++) for (gx = 0; gx < 8; gx++) {
                        int px = tx + k * 8 + gx + ox, py = ty + gy + oy;
                        if (!(g[gy] & (0x80 >> gx)) || px < 0 || py < 0 || px >= (int)w || py >= (int)h) continue;
                        pix[py * w + px] = c; msk[py * w + px] = 1;
                    }
                }
            }
        } else break;
    }
    /* re-encode (runs of >= 3 equal pixels, literals otherwise) */
    out = (uint8_t*)malloc(16 + 24 + h * (w * 2 + 8));
    memcpy(out, d, 16);
    *(uint32_t*)(out + 8) = 16;
    memcpy(out + 16, d + so, 24);
    o = 40;
    for (y = 0; y < h; y++) {
        const uint8_t *pr = pix + y * w, *mr = msk + y * w;
        x = 0;
        while (x < w) {
            uint32_t r = 1;
            if (!mr[x]) {
                while (x + r < w && !mr[x + r] && r < 255) r++;
                if (x + r >= w) { x = w; break; }
                out[o++] = 1; out[o++] = (uint8_t)r; x += r; continue;
            }
            while (x + r < w && mr[x + r] && pr[x + r] == pr[x] && r < 127) r++;
            if (r >= 3) { out[o++] = (uint8_t)(r << 1); out[o++] = pr[x]; x += r; continue; }
            r = 1;
            while (x + r < w && mr[x + r] && r < 127 &&
                   !(x + r + 2 < w && mr[x + r + 1] && mr[x + r + 2] && pr[x + r] == pr[x + r + 1] && pr[x + r] == pr[x + r + 2])) r++;
            out[o++] = (uint8_t)((r << 1) | 1);
            memcpy(out + o, pr + x, r); o += r; x += r;
        }
        out[o++] = 0;
    }
    free(pix); free(msk);
    *outlen = o;
    return out;
}

/* 'R' section (EYE.RES): u32 orig_size, u32 orig_crc, u32 n, n * {u32 index, u32 len, bytes},
 *   then u32 nb, nb * {u32 index, u32 oplen, ops} picture edits.
 * The replaced resources are appended at the end of the file and their directory entries
 * repointed; the old data stays in place as unused space. */
typedef struct { uint32_t idx, len; const uint8_t *data; uint8_t *own; } rrep;
static uint8_t *build_res(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], n = ((const uint32_t*)sec)[2];
    const uint8_t *p = sec + 12, *e = sec + slen;
    uint32_t total = olen, i, pos, nr = 0, nbm = 0;
    rrep *rp;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("res mismatch\n"); return NULL; }
    rp = (rrep*)calloc(n + 64, sizeof(rrep));
    for (i = 0; i < n; i++) {
        rp[nr].idx = ((const uint32_t*)p)[0]; rp[nr].len = ((const uint32_t*)p)[1]; rp[nr].data = p + 8;
        p += 8 + rp[nr].len; nr++;
    }
    if (p + 4 <= e) { nbm = *(const uint32_t*)p; p += 4; }
    for (i = 0; i < nbm && i < 64 && p + 8 <= e; i++) {
        uint32_t idx = ((const uint32_t*)p)[0], ol = ((const uint32_t*)p)[1], l;
        uint8_t *b = edit_shape(orig, olen, idx, p + 8, ol, &l);
        p += 8 + ol;
        if (!b) { dbg("picture %u not edited\n", idx); continue; }
        rp[nr].idx = idx; rp[nr].len = l; rp[nr].data = b; rp[nr].own = b; nr++;
    }
    /* byte patches of script resources: u32 n, n * {u32 index, u32 count, count * {u32 off, u8 len, orig[len], new[len]}} */
    if (p + 4 <= e) {
        uint32_t ncp = *(const uint32_t*)p, k;
        p += 4;
        for (i = 0; i < ncp && p + 8 <= e; i++) {
            uint32_t idx = ((const uint32_t*)p)[0], cnt = ((const uint32_t*)p)[1], ent = res_dirent(orig, olen, idx), roff, rl;
            uint8_t *b = NULL;
            int ok = 1;
            p += 8;
            if (ent && (roff = *(const uint32_t*)(orig + ent)) && roff + 12 <= olen &&
                roff + 12 + (rl = *(const uint32_t*)(orig + roff + 8)) <= olen) {
                b = (uint8_t*)malloc(rl);
                memcpy(b, orig + roff + 12, rl);
            } else ok = 0;
            for (k = 0; k < cnt; k++) {
                uint32_t off = *(const uint32_t*)p; uint8_t l = p[4];
                if (ok && (off + l > rl || memcmp(b + off, p + 5, l))) ok = 0;
                if (ok) memcpy(b + off, p + 5 + l, l);
                p += 5 + 2 * l;
            }
            if (!ok || nr >= n + 64) { dbg("script %u not patched\n", idx); free(b); continue; }
            rp[nr].idx = idx; rp[nr].len = rl; rp[nr].data = b; rp[nr].own = b; nr++;
        }
    }
    for (i = 0; i < nr; i++) total += 12 + rp[i].len;
    out = (uint8_t*)malloc(total);
    memcpy(out, orig, olen);
    pos = olen;
    for (i = 0; i < nr; i++) {
        uint32_t ent = res_dirent(out, olen, rp[i].idx), oldoff;
        if (!ent || !(oldoff = *(uint32_t*)(out + ent)) || oldoff + 12 > olen) { free(out); out = NULL; break; }
        memcpy(out + pos, orig + oldoff, 8);            /* timestamp, attributes */
        *(uint32_t*)(out + pos + 8) = rp[i].len;
        memcpy(out + pos + 12, rp[i].data, rp[i].len);
        *(uint32_t*)(out + ent) = pos;
        pos += 12 + rp[i].len;
    }
    for (i = 0; i < nr; i++) free(rp[i].own);
    free(rp);
    if (!out) return NULL;
    *(uint32_t*)(out + 0x10) = total;                     /* file_size */
    *outlen = total;
    return out;
}

/* ---- cinematics (INTRO/DARK/LICH/FINALE.GFF, played by CINE.EXE through XVA.DRV).
 * Subtitles are baked into the lower part (rows >= y0) of some BMP/BMA frames.
 * 'G' section: u32 orig_size, u32 orig_crc, u32 n, n * {char tag[4], u32 chunk_id, u32 frame, u32 y0, u32 len, rows[len]}
 * rows = our rendered Japanese subtitle rows (row records of the frame format, without the 0xff end).
 * Frame format: u16 w, u16 h, then rows {u8 y; segments {u16 x (bit15 = last), u8 width, u8 datalen, data}} ... u8 0xff.
 * A chunk is a series of blocks {u32 len, u16 count, u32 frame_off[count], frames}. */
typedef struct { const uint8_t *tag; uint32_t id, frame, y0, len; const uint8_t *rows; } grep_t;

/* end offset (exclusive, after the 0xff) of the frame at f; copies rows with y < y0 to out when out != NULL */
static uint32_t gff_frame(const uint8_t *c, uint32_t clen, uint32_t f, uint32_t y0, uint8_t *out, uint32_t *olen)
{
    uint32_t p = f + 4, o = 0;
    if (out) { memcpy(out, c + f, 4); o = 4; }
    while (p < clen) {
        uint32_t r = p, y = c[p++];
        if (y == 0xff) break;
        for (;;) {
            uint16_t xw;
            if (p + 4 > clen) return 0;
            xw = (uint16_t)(c[p] | (c[p + 1] << 8));
            p += 4 + c[p + 3];
            if (xw & 0x8000) break;
        }
        if (out && y < y0) { memcpy(out + o, c + r, p - r); o += p - r; }
    }
    if (p > clen) return 0;
    if (olen) *olen = o;
    return p;
}

static uint8_t *gff_chunk(const uint8_t *c, uint32_t clen, const grep_t *rp, uint32_t nr, uint32_t *outlen)
{
    uint32_t cap = clen + 1024, o = 0, b = 0, gidx = 0;
    uint32_t k;
    uint8_t *out;
    for (k = 0; k < nr; k++) cap += rp[k].len + 8;
    out = (uint8_t*)malloc(cap);
    while (b + 6 <= clen) {
        uint32_t ln = *(const uint32_t*)(c + b), n = *(const uint16_t*)(c + b + 4), hb = 6 + 4 * n, ob = o, i;
        if (!ln || !n || b + ln > clen || hb > ln) break;
        memcpy(out + o, c + b, hb);
        o += hb;
        for (i = 0; i < n; i++, gidx++) {
            uint32_t fo = *(const uint32_t*)(c + b + 6 + 4 * i), fe = ln, j;
            const grep_t *r = NULL;
            for (j = 0; j < n; j++) {            /* frame extent: up to the next frame */
                uint32_t q = *(const uint32_t*)(c + b + 6 + 4 * j);
                if (q > fo && q < fe) fe = q;
            }
            for (j = 0; j < nr; j++) if (rp[j].frame == gidx) r = &rp[j];
            *(uint32_t*)(out + ob + 6 + 4 * i) = o - ob;
            if (fo >= ln) { free(out); return NULL; }
            if (r) {
                /* CINE's ACF scripts carry the byte size of each BMA block, so a frame must keep its size:
                 * the new frame is padded with zeros up to the old extent (and skipped if it would not fit) */
                uint32_t keep, end = gff_frame(c + b, ln, fo, r->y0, out + o, &keep);
                if (!end || end > fe) { free(out); return NULL; }
                if (keep + r->len + 1 > fe - fo) {
                    dbg("gff frame %u does not fit\n", gidx);
                    memcpy(out + o, c + b + fo, fe - fo); o += fe - fo;
                } else {
                    uint32_t start = o;
                    o += keep;
                    memcpy(out + o, r->rows, r->len); o += r->len;
                    out[o++] = 0xff;
                    memset(out + o, 0, fe - fo - (o - start)); o = start + (fe - fo);
                }
            } else {
                memcpy(out + o, c + b + fo, fe - fo); o += fe - fo;
            }
        }
        *(uint32_t*)(out + ob) = o - ob;
        b += ln;
    }
    if (b < clen) { memcpy(out + o, c + b, clen - b); o += clen - b; }
    *outlen = o;
    return out;
}

static uint8_t *build_gff(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], n = ((const uint32_t*)sec)[2];
    const uint8_t *p = sec + 12, *e = sec + slen;
    grep_t *rp;
    uint32_t i, tl, tlen, nt, t, o, total, ndone = 0;
    uint8_t *out;
    typedef struct { uint32_t te, off, len; uint8_t *data; uint32_t nlen; } ent_t;
    ent_t *ents = NULL;
    uint32_t ne = 0;
    if (osz != olen || crc32(orig, olen) != ocrc || olen < 0x1c) { dbg("gff mismatch\n"); return NULL; }
    rp = (grep_t*)calloc(n + 1, sizeof(grep_t));
    for (i = 0; i < n && p + 20 <= e; i++) {
        rp[i].tag = p; rp[i].id = ((const uint32_t*)p)[1]; rp[i].frame = ((const uint32_t*)p)[2];
        rp[i].y0 = ((const uint32_t*)p)[3]; rp[i].len = ((const uint32_t*)p)[4]; rp[i].rows = p + 20;
        p += 20 + rp[i].len;
    }
    tl = *(const uint32_t*)(orig + 0xc); tlen = *(const uint32_t*)(orig + 0x10);
    if (tl + 10 > olen || tl + tlen > olen) { free(rp); return NULL; }
    nt = *(const uint16_t*)(orig + tl + 8);
    /* collect TOC entries */
    o = tl + 10;
    for (t = 0; t < nt; t++) {
        uint32_t cnt, k;
        if (o + 8 > olen) break;
        cnt = *(const uint32_t*)(orig + o + 4);
        ents = (ent_t*)realloc(ents, (ne + cnt) * sizeof(ent_t));
        for (k = 0; k < cnt && o + 8 + 12 * k + 12 <= olen; k++) {
            uint32_t te = o + 8 + 12 * k, j, nr = 0;
            const uint8_t *tag = orig + o;
            uint32_t id = *(const uint32_t*)(orig + te), off = *(const uint32_t*)(orig + te + 4), len = *(const uint32_t*)(orig + te + 8);
            grep_t sub[64];
            ents[ne].te = te; ents[ne].off = off; ents[ne].len = len; ents[ne].data = NULL; ents[ne].nlen = len;
            if (off + len > olen) { free(rp); free(ents); return NULL; }
            for (j = 0; j < n && nr < 64; j++)
                if (!memcmp(rp[j].tag, tag, 4) && rp[j].id == id) sub[nr++] = rp[j];
            if (nr) {
                ents[ne].data = gff_chunk(orig + off, len, sub, nr, &ents[ne].nlen);
                if (!ents[ne].data) dbg("gff chunk %.4s %u not rebuilt\n", tag, id);
                else ndone += nr;
            }
            ne++;
        }
        o += 8 + 12 * cnt;
    }
    dbg("gff: %u of %u subtitle frames\n", ndone, n);
    /* lay out: header, chunks in file order (with their new sizes), the TOC at the end */
    total = olen + 16;
    for (i = 0; i < ne; i++) total += ents[i].nlen;
    out = (uint8_t*)malloc(total);
    {
        uint32_t pos = 0, src = 0;
        uint32_t *order = (uint32_t*)malloc(ne * sizeof(uint32_t));
        for (i = 0; i < ne; i++) order[i] = i;
        for (i = 1; i < ne; i++) {               /* sort by original offset */
            uint32_t v = order[i], j = i;
            while (j > 0 && ents[order[j - 1]].off > ents[v].off) { order[j] = order[j - 1]; j--; }
            order[j] = v;
        }
        for (i = 0; i < ne; i++) {
            ent_t *en = &ents[order[i]];
            if (en->off > src) { memcpy(out + pos, orig + src, en->off - src); pos += en->off - src; src = en->off; }
            if (en->off < src) { free(order); free(out); out = NULL; break; }   /* overlapping chunks: give up */
            {
                uint32_t np = pos;
                if (en->data) memcpy(out + pos, en->data, en->nlen); else memcpy(out + pos, orig + en->off, en->len);
                pos += en->nlen; src = en->off + en->len;
                en->off = np;                     /* new offset */
            }
        }
        if (out) {
            uint32_t ntl;
            free(order);
            if (tl < src) { free(out); out = NULL; }
            else {
                memcpy(out + pos, orig + src, tl - src); pos += tl - src;
                ntl = pos;
                memcpy(out + pos, orig + tl, olen - tl); pos += olen - tl;
                for (i = 0; i < ne; i++) {
                    uint32_t te = ntl + (ents[i].te - tl);
                    *(uint32_t*)(out + te + 4) = ents[i].off;
                    *(uint32_t*)(out + te + 8) = ents[i].nlen;
                }
                *(uint32_t*)(out + 0xc) = ntl;
                *outlen = pos;
            }
        }
    }
    for (i = 0; i < ne; i++) free(ents[i].data);
    free(ents); free(rp);
    if (out && !ndone) { free(out); return NULL; }
    return out;
}

static const char *base_name(const char *p)
{
    const char *b = p, *q;
    for (q = p; *q; q++) if (*q == '\\' || *q == '/' || *q == ':') b = q + 1;
    return b;
}

static CRITICAL_SECTION g_cs;

static const char *patched_path(const char *orig_path, char *buf)
{
    const char *bn = base_name(orig_path);
    uint32_t slen, olen, nlen;
    const uint8_t *sx, *sr, *sg = NULL;
    uint8_t *orig, *out = NULL;
    if (!g_enabled) return NULL;
    sx = find_sect('X', bn, &slen);
    sr = sx ? NULL : find_sect('R', bn, &slen);
    if (!sx && !sr) sg = find_sect('G', bn, &slen);
    if (!sx && !sr && !sg) return NULL;
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) return buf;
    {   /* remove copies left by older patch versions */
        char pat[MAX_PATH], old[MAX_PATH], cur[16];
        WIN32_FIND_DATAA fd;
        HANDLE fh;
        wsprintfA(pat, "%s\\????????_%s", g_dir, bn);
        wsprintfA(cur, "%08X_", g_blob_crc);
        fh = FindFirstFileA(pat, &fd);
        if (fh != INVALID_HANDLE_VALUE) {
            do {
                if (_strnicmp(fd.cFileName, cur, 9)) {
                    wsprintfA(old, "%s\\%s", g_dir, fd.cFileName);
                    DeleteFileA(old);
                }
            } while (FindNextFileA(fh, &fd));
            FindClose(fh);
        }
    }
    orig = read_all(orig_path, &olen);
    if (!orig) return NULL;
    dbg("building %s (%u bytes)\n", bn, olen);
    out = sx ? build_exe(orig, olen, sx, slen, &nlen) : sr ? build_res(orig, olen, sr, slen, &nlen)
                                                       : build_gff(orig, olen, sg, slen, &nlen);
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
    wsprintfA(g_dir, "%s\\EOB3_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("EOB3 JP patch active, blob %08X\n", g_blob_crc);
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
