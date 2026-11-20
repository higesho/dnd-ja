/*
 * Shadow Sorcerer Japanese patch - runtime part (winmm.dll proxy for the bundled DOSBox 0.74-3).
 *  - fopen() of SHADOW.COM / GAME.EXE / TEXT.DAT is redirected to copies built in %LOCALAPPDATA%\SHADOW_JP
 *  - SHADOW.COM's string printer gets one DOSBox callback (Japanese glyph in the font slot of 7Fh)
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
    if (memcmp(h->magic, "SSJP", 4)) return 0;
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

/* SHADOW.COM (the EXEC loader, also the runtime GAME.EXE calls through int F8h) prints strings with
 * function 0Bh: ds:si = text, 8x8 font at cs:3FEh + (ch - 20h) * 8, one character cell per byte.
 * The callback replaces "push ds; push si; sub al, 20h" (cs:15EAh):
 *   lead byte  -> remembered, nothing drawn, the cell is not used (jump back to the loop at cs:15CFh)
 *   trail byte -> the Japanese glyph is copied into the font slot of 7Fh, which is then drawn in one cell */
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
#define LOOP_IP 0x15cfu
#define SLOT    0x6f6u        /* cs offset of the glyph of 7Fh */
static uint8_t g_lead;

static uint32_t __cdecl cb_char(void)
{
    uint8_t c = (uint8_t)R_EAX;
    uint32_t cs = PHYS_CS;
    if (IS_LEAD(c)) {
        g_lead = c;
        set16(&R_EIP, (uint16_t)LOOP_IP);
        return 0;
    }
    push16(SEGV(3));                          /* push ds */
    push16((uint16_t)REG32(6));               /* push si */
    if (IS_TRAIL(c) && g_lead) {
        int gn = glyph_no(g_lead, c), k;
        const uint8_t *g = gn ? g_glyphs + (gn - 1) * 8 : NULL;
        for (k = 0; k < 8; k++) wb(cs + SLOT + k, g ? g[k] : 0);
        c = 0x7f;
    }
    g_lead = 0;
    R_EAX = (R_EAX & 0xffffff00u) | (uint8_t)(c - 0x20);     /* sub al, 20h */
    return 0;
}

/* GAME.EXE measures message text (longest line, line count) one byte at a time (cs:0E37h).
 * The callback replaces "inc word ptr [bp+4]; cmp byte ptr [bx],0Dh" (cs:0E56h) and does the branch itself,
 * so that the second byte of a Japanese character is not counted as a column. */
static uint32_t __cdecl cb_measure(void)
{
    uint32_t ss = PHYS_SS, bp = BP16;
    uint8_t c = rb(PHYS_DS + (uint16_t)R_EBX);
    uint16_t base = (uint16_t)((uint16_t)R_EIP - (0x0e56 + 4));
    ww(ss + (uint16_t)(bp + 4), (uint16_t)(rw(ss + (uint16_t)(bp + 4)) + 1));      /* inc word ptr [bp+4] */
    if (c == 0x0d) { set16(&R_EIP, (uint16_t)(base + 0x0e5e)); return 0; }          /* new line */
    if (!IS_TRAIL(c)) ww(ss + (uint16_t)(bp - 2), (uint16_t)(rw(ss + (uint16_t)(bp - 2)) + 1));
    set16(&R_EIP, (uint16_t)(base + 0x0e79));                                       /* jmp loop */
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base;
    if (g_cb_installed) return 1;
    if (tab[b] != A_CB_ILLEGAL || tab[b + 1] != A_CB_ILLEGAL) { dbg("callback slot %x busy (%x)\n", b, tab[b]); return 0; }
    tab[b] = (uint32_t)cb_char;
    tab[b + 1] = (uint32_t)cb_measure;
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

/* ------------------------------------------- DOS memory for the fake font */
/* walk the MCB chain and carve 4 KB (+MCB) off the end of a free block, owned by DOS (8), named "SSJP" */
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
        if (own == 8 && size == 0x100 && rb(s * 16 + 8) == 'S' && rb(s * 16 + 9) == 'S' && rb(s * 16 + 10) == 'J' && rb(s * 16 + 11) == 'P')
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
        memcpy(mcb + 8, "SSJP", 4);
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
    sx = find_sect('X', bn, &slen);
    st = sx ? NULL : find_sect('T', bn, &slen);
    if (!sx && !st) return NULL;
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
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
    wsprintfA(g_dir, "%s\\SHADOW_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("Shadow Sorcerer JP patch active, blob %08X\n", g_blob_crc);
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
