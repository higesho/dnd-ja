/*
 * War of the Lance Japanese patch - runtime part (winmm.dll proxy for the bundled DOSBox 0.74-3).
 *  - fopen() of START.EXE is redirected to copies built in %LOCALAPPDATA%\WAR_JP
 *  - START.EXE's character drawing routines get one DOSBox callback (Japanese glyph in the font slot of 7Fh)
 *  Japanese text uses a private 2-byte code: lead 0x80-0xBF, trail 0xC0-0xFF (one character cell per Japanese character).
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
    if (memcmp(h->magic, "WLJP", 4)) return 0;
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
#define IS_LEAD(c)  ((c) >= 0x81 && (c) <= 0xbf)
#define IS_TRAIL(c) ((c) >= 0xc0)

static int glyph_no(uint8_t lead, uint8_t trail)
{
    return g_glyph_index[(lead << 8) | trail];   /* 0 = none */
}

#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
#define R_ECX REG32(1)
#define R_EDX REG32(2)
typedef uint32_t (__cdecl *cb_fn)(void);
static cb_fn g_int10_orig;
static int g_int10_cb = -1;
static int g_trace_n;

/* War of the Lance (QuickBASIC) prints with INT 10h: AH=09h (char at cursor) then AH=02h (cursor to the next column).
 * Japanese = 2 bytes but 1 cell: the lead byte is not drawn and every following cursor move on that line is shifted
 * one column left; the trail byte is drawn from an 8-byte glyph put in the BIOS inter-application area (0040:00F0)
 * by pointing INT 43h there for that one call. */
#define GLYPHBUF 0x4f0u
static uint8_t g_lead;
static int g_shift, g_row = -1, g_bc = -1;


static int is_lead_any(uint8_t c) { return IS_LEAD(c) || (c >= 'a' && c <= 'z'); }
static int pair_glyph(uint8_t lead, uint8_t al)
{
    if (lead >= 0x81 && IS_TRAIL(al)) return glyph_no(lead, al);
    if (lead >= 'a' && lead <= 'z' && al > 0x20 && al < 0x7f) {
        int ix = (lead - 'a') * 94 + (al - 0x21);
        return glyph_no((uint8_t)(0x81 + ix / 64), (uint8_t)(0xc0 + ix % 64));
    }
    return -1;
}
/* draw glyph gn (0 = blank) with the original handler: AL = 0, INT 43h -> 0040:00F0 for this call */
static uint32_t draw_glyph(int gn)
{
    uint32_t v43 = rw(0x43 * 4) | ((uint32_t)rw(0x43 * 4 + 2) << 16), r;
    const uint8_t *g = gn > 0 ? g_glyphs + (gn - 1) * 8 : NULL;
    int k;
    for (k = 0; k < 8; k++) wb(GLYPHBUF + k, g ? g[k] : 0);
    ww(0x43 * 4, 0xf0); ww(0x43 * 4 + 2, 0x40);
    R_EAX &= 0xffffff00u;
    r = g_int10_orig();
    ww(0x43 * 4, (uint16_t)v43); ww(0x43 * 4 + 2, (uint16_t)(v43 >> 16));
    return r;
}

static uint32_t __cdecl cb_int10(void)
{
    uint8_t ah = (uint8_t)(R_EAX >> 8), al = (uint8_t)R_EAX;
    if (g_trace_n < 3000 && g_logon > 1) {
        g_trace_n++;
        dbg("int10 ax=%04x bx=%04x cx=%04x dx=%04x\n", (uint16_t)R_EAX, (uint16_t)R_EBX, (uint16_t)R_ECX, (uint16_t)R_EDX);
    }
    if (ah == 0x09 || ah == 0x0a) {
        uint8_t page = (uint8_t)(R_EBX >> 8) & 7;
        int col = rb(0x450 + page * 2), row = rb(0x451 + page * 2);
        if (row != g_row) g_shift = 0;
        if (IS_LEAD(al) || (al >= 'a' && al <= 'z' && !g_lead)) {
            g_lead = al; g_row = row; g_bc = col + g_shift; g_shift++;
            return 0;
        }
        g_row = row; g_bc = col + g_shift;
        if (al == ' ' && g_shift > 0 && !g_lead && (uint16_t)R_ECX == 1) {
            /* a space after Japanese text takes two cells and gives one column back (keeps padded tables aligned) */
            R_ECX = (R_ECX & 0xffff0000u) | 2;
            g_shift--;
            return g_int10_orig();
        }
        if ((IS_TRAIL(al) && g_lead >= 0x81) || (g_lead >= 'a' && g_lead <= 'z' && al > 0x20 && al < 0x7f)) {
            int gn, k;
            if (g_lead < 0x80) {      /* 7-bit pair from the data files (they keep only 7 bits): index = (lead-'a')*94 + trail-21h */
                int ix = (g_lead - 'a') * 94 + (al - 0x21);
                gn = glyph_no((uint8_t)(0x81 + ix / 64), (uint8_t)(0xc0 + ix % 64));
            } else gn = glyph_no(g_lead, al);
            uint32_t v43 = rw(0x43 * 4) | ((uint32_t)rw(0x43 * 4 + 2) << 16), r;
            const uint8_t *g = gn ? g_glyphs + (gn - 1) * 8 : NULL;
            g_lead = 0;
            for (k = 0; k < 8; k++) wb(GLYPHBUF + k, g ? g[k] : 0);
            ww(0x43 * 4, 0xf0); ww(0x43 * 4 + 2, 0x40);
            R_EAX &= 0xffffff00u;
            r = g_int10_orig();
            ww(0x43 * 4, (uint16_t)v43); ww(0x43 * 4 + 2, (uint16_t)(v43 >> 16));
            return r;
        }
        g_lead = 0;
        return g_int10_orig();
    }
    if (ah == 0x0e) {               /* teletype (the event messages): BIOS moves the cursor itself, no shift needed */
        if (g_lead) {
            int gn = pair_glyph(g_lead, al);
            g_lead = 0;
            if (gn >= 0) return draw_glyph(gn);
        } else if (is_lead_any(al)) { g_lead = al; return 0; }
        return g_int10_orig();
    }
    if (ah == 0x02) {
        int row = (uint8_t)(R_EDX >> 8), col = (uint8_t)R_EDX;
        if (g_shift && row == g_row && col == g_bc + 1) R_EDX = (R_EDX & 0xffffff00u) | (uint8_t)(col - g_shift);
        else { g_shift = 0; g_lead = 0; }
        return g_int10_orig();
    }
    if (ah == 0x03) {
        uint32_t r = g_int10_orig();
        if (g_shift && (uint8_t)(R_EDX >> 8) == g_row) R_EDX = (R_EDX & 0xffffff00u) | (uint8_t)((uint8_t)R_EDX + g_shift);
        return r;
    }
    return g_int10_orig();
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    uint32_t v;
    if (g_cb_installed) return 1;
    v = farptr(0x10 * 4);
    if (rb(v) == 0xfe && rb(v + 1) == 0x38) {
        g_int10_cb = rw(v + 2);
        g_int10_orig = (cb_fn)tab[g_int10_cb];
        tab[g_int10_cb] = (uint32_t)cb_int10;
        dbg("int10 at %x callback %d\n", v, g_int10_cb);
    } else dbg("int10 vector %x: %02x %02x\n", v, rb(v), rb(v + 1));
    g_cb_installed = 1;
    return 1;
}

/* ------------------------------------------- DOS memory for the fake font */
/* walk the MCB chain and carve 4 KB (+MCB) off the end of a free block, owned by DOS (8), named "WLJP" */
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
        if (own == 8 && size == 0x100 && rb(s * 16 + 8) == 'H' && rb(s * 16 + 9) == 'L' && rb(s * 16 + 10) == 'J' && rb(s * 16 + 11) == 'P')
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
        memcpy(mcb + 8, "WLJP", 4);
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

/* 'U' section: like 'X', but the original is an EXEPACKed MZ file. It is unpacked first and rebuilt as a plain MZ
 * (header 0x1c + relocations, rounded to 16 bytes); the records then patch that plain file.
 * 'R' records in the same layout patch data files whose size may change: u32 orig_size, u32 crc, u32 newlen, bytes. */
static uint8_t *unpack_exe(const uint8_t *d, uint32_t n, uint32_t *outlen)
{
    uint32_t hdr, eh, plen, dlen, si, di, t, k, nrel = 0, hsize, total, g;
    const uint16_t *h = (const uint16_t*)d;
    uint16_t rIP, rCS, rSP, rSS;
    uint8_t *buf, *out;
    const char *msg = "Packed file is corrupt";
    if (n < 0x40 || h[0] != 0x5a4d) return NULL;
    hdr = h[4] * 16u;
    eh = hdr + h[11] * 16u;
    if (eh + 18 > n) return NULL;
    rIP = *(const uint16_t*)(d + eh); rCS = *(const uint16_t*)(d + eh + 2);
    rSP = *(const uint16_t*)(d + eh + 8); rSS = *(const uint16_t*)(d + eh + 10);
    dlen = *(const uint16_t*)(d + eh + 12) * 16u;
    if (*(const uint16_t*)(d + eh + 14) != 0x4252) return NULL;
    plen = eh - hdr;
    buf = (uint8_t*)calloc(dlen > plen ? dlen : plen, 1);
    memcpy(buf, d + hdr, plen);
    si = plen - 1;
    for (k = 0; k < 16 && buf[si] == 0xff; k++) si--;
    di = dlen - 1;
    for (;;) {
        uint8_t cmd = buf[si];
        uint32_t cx;
        si -= 2;
        cx = buf[si] | (buf[si + 1] << 8);
        si -= 1;
        if ((cmd & 0xfe) == 0xb0) { uint8_t v = buf[si--]; while (cx--) buf[di--] = v; }
        else if ((cmd & 0xfe) == 0xb2) { while (cx--) buf[di--] = buf[si--]; }
        else { free(buf); return NULL; }
        if (cmd & 1) break;
    }
    /* relocation table: after the error message, 16 groups {u16 count, u16 offsets[count]} */
    for (t = eh; t + 22 < n; t++) if (!memcmp(d + t, msg, 22)) break;
    if (t + 22 >= n) { free(buf); return NULL; }
    t += 22;
    { uint32_t q = t; for (g = 0; g < 16; g++) { uint16_t c = *(const uint16_t*)(d + q); nrel += c; q += 2 + c * 2u; } }
    hsize = (0x1c + nrel * 4 + 15) / 16 * 16;
    total = hsize + dlen;
    out = (uint8_t*)calloc(total, 1);
    {
        uint16_t *o = (uint16_t*)out;
        uint32_t loadparas = (n - hdr + 15) / 16, needparas = loadparas + h[5], minalloc;
        minalloc = needparas > dlen / 16 ? needparas - dlen / 16 : 0;
        if (minalloc < 0x100) minalloc = 0x100;
        o[0] = 0x5a4d; o[1] = (uint16_t)(total % 512); o[2] = (uint16_t)((total + 511) / 512); o[3] = (uint16_t)nrel;
        o[4] = (uint16_t)(hsize / 16); o[5] = (uint16_t)minalloc; o[6] = 0xffff; o[7] = rSS; o[8] = rSP; o[9] = 0;
        o[10] = rIP; o[11] = rCS; o[12] = 0x1c; o[13] = 0;
        k = 0x1c;
        for (g = 0; g < 16; g++) {
            uint16_t c = *(const uint16_t*)(d + t), i;
            t += 2;
            for (i = 0; i < c; i++) {
                *(uint16_t*)(out + k) = *(const uint16_t*)(d + t); *(uint16_t*)(out + k + 2) = (uint16_t)(g * 0x1000);
                k += 4; t += 2;
            }
        }
    }
    memcpy(out + hsize, buf, dlen);
    free(buf);
    *outlen = total;
    return out;
}

static uint8_t *build_unpacked(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], ulen;
    const uint8_t *p = sec + 8, *e = sec + slen;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("exe mismatch\n"); return NULL; }
    out = unpack_exe(orig, olen, &ulen);
    if (!out) { dbg("unpack failed\n"); return NULL; }
    while (p < e) {
        uint32_t off = ((const uint32_t*)p)[0], n = ((const uint32_t*)p)[1];
        p += 8;
        if (off + n > ulen) { free(out); return NULL; }
        memcpy(out + off, p, n);
        p += n;
    }
    *outlen = ulen;
    return out;
}

/* 'D' section: BSAVE data file (FD seg off len, body, ...) whose body is a list of items separated by FFh;
 * u32 orig_size, u32 crc, u32 n, n * {u32 index, u32 len, bytes} replace items. */
static uint8_t *build_items(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], n = ((const uint32_t*)sec)[2];
    const uint8_t *p = sec + 12, *e = sec + slen;
    const uint8_t *it[4096];
    uint32_t il[4096], cnt = 0, blen, i, total, pos, start;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc || olen < 7 || orig[0] != 0xfd) { dbg("data mismatch\n"); return NULL; }
    blen = orig[5] | (orig[6] << 8);
    if (7 + blen > olen) return NULL;
    start = 7;
    for (i = 7; i <= 7 + blen && cnt < 4096; i++) {
        if (i == 7 + blen || orig[i] == 0xff) { it[cnt] = orig + start; il[cnt] = i - start; cnt++; start = i + 1; }
    }
    for (i = 0; i < n && p + 8 <= e; i++) {
        uint32_t k = ((const uint32_t*)p)[0], l = ((const uint32_t*)p)[1];
        if (k < cnt) { it[k] = p + 8; il[k] = l; }
        p += 8 + l;
    }
    total = 0;
    for (i = 0; i < cnt; i++) total += il[i] + (i + 1 < cnt);
    out = (uint8_t*)malloc(7 + total + (olen - 7 - blen));
    memcpy(out, orig, 5);
    out[5] = (uint8_t)total; out[6] = (uint8_t)(total >> 8);
    pos = 7;
    for (i = 0; i < cnt; i++) {
        memcpy(out + pos, it[i], il[i]); pos += il[i];
        if (i + 1 < cnt) out[pos++] = 0xff;
    }
    memcpy(out + pos, orig + 7 + blen, olen - 7 - blen); pos += olen - 7 - blen;
    *outlen = pos;
    return out;
}

static uint8_t *build_replace(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], n = ((const uint32_t*)sec)[2];
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc || n + 12 > slen) { dbg("data mismatch\n"); return NULL; }
    out = (uint8_t*)malloc(n ? n : 1);
    memcpy(out, sec + 12, n);
    *outlen = n;
    return out;
}

/* 'T' section (TEXT.DAT): u32 orig_size, u32 orig_crc, u32 n, n * {u32 id, u32 len, bytes}.
 * TEXT.DAT = u16 offset[count] (count = offset[0] / 2), NUL-terminated strings. */
static uint8_t *build_text(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], n = ((const uint32_t*)sec)[2];
    const uint8_t *p = sec + 12, *e = sec + slen;
    uint32_t cnt, i, total, pos;
    const uint8_t **txt;
    uint32_t *len;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc || olen < 2) { dbg("TEXT.DAT mismatch\n"); return NULL; }
    cnt = *(const uint16_t*)orig / 2;
    txt = (const uint8_t**)calloc(cnt, sizeof *txt); len = (uint32_t*)calloc(cnt, 4);
    for (i = 0; i < cnt; i++) {
        uint32_t o = ((const uint16_t*)orig)[i], k = o;
        while (k < olen && orig[k]) k++;
        txt[i] = orig + o; len[i] = k - o;
    }
    for (i = 0; i < n && p + 8 <= e; i++) {
        uint32_t id = ((const uint32_t*)p)[0], l = ((const uint32_t*)p)[1];
        if (id < cnt) { txt[id] = p + 8; len[id] = l; }
        p += 8 + l;
    }
    total = cnt * 2;
    for (i = 0; i < cnt; i++) total += len[i] + 1;
    if (total > 0xffff) { free(txt); free(len); dbg("TEXT.DAT too big\n"); return NULL; }
    out = (uint8_t*)malloc(total);
    pos = cnt * 2;
    for (i = 0; i < cnt; i++) {
        ((uint16_t*)out)[i] = (uint16_t)pos;
        memcpy(out + pos, txt[i], len[i]); pos += len[i];
        out[pos++] = 0;
    }
    free(txt); free(len);
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
    int kind = 0;
    sx = find_sect('U', bn, &slen);
    if (sx) kind = 1; else if ((sx = find_sect('R', bn, &slen)) != NULL) kind = 2;
    else if ((sx = find_sect('X', bn, &slen)) != NULL) kind = 3;
    else if ((sx = find_sect('D', bn, &slen)) != NULL) kind = 4;
    st = sx ? NULL : find_sect('T', bn, &slen);
    if (!sx && !st) return NULL;
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) return buf;
    clean_old(bn);
    orig = read_all(orig_path, &olen);
    if (!orig) return NULL;
    out = kind == 1 ? build_unpacked(orig, olen, sx, slen, &nlen) : kind == 2 ? build_replace(orig, olen, sx, slen, &nlen) :
          kind == 3 ? build_exe(orig, olen, sx, slen, &nlen) : kind == 4 ? build_items(orig, olen, sx, slen, &nlen) :
          build_text(orig, olen, st, slen, &nlen);
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
    wsprintfA(g_dir, "%s\\WAR_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("War of the Lance JP patch active, blob %08X\n", g_blob_crc);
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
