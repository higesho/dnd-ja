/*
 * DeathKeep 日本語化パッチ（実行時部分）
 * DK.EXE が読み込む winmm.dll の代理としてゲームのフォルダに置く。ゲームのファイルは書き換えない。
 *  - 文字を描く関数と幅を測る関数（DK.EXE 内、パターンで探す）を差し替え、描く時に訳へ置き換える
 *    （ASCII はゲームのフォント、日本語は k8x12 の 8x12 グリフで描く）
 *  - sprintf で組み立てた文は、書式（%s %d など）で照合して訳の書式で組み立て直す（%s の中身も訳す）
 *  - キー設定の一覧やエラー画面（GDI の TextOutA / TabbedTextOutA）、MessageBoxA も訳す
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "resource.h"

void proxy_init(void);
void testio_init(void);
void subs_init(HMODULE self);
void *iat_hook(HMODULE mod, const char *dll, const char *func, void *repl);

static char g_dir[MAX_PATH];
static FILE *g_log;
static int g_logon, g_test;
void dlog(const char *fmt, ...)
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

void *iat_hook(HMODULE mod, const char *dll, const char *func, void *repl)
{
    uint8_t *base = (uint8_t*)mod;
    IMAGE_NT_HEADERS *nh = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *id = (IMAGE_IMPORT_DESCRIPTOR*)(base + dd->VirtualAddress);
    for (; id->Name; id++) {
        IMAGE_THUNK_DATA *ot, *ft;
        if (_stricmp((const char*)(base + id->Name), dll)) continue;
        ot = (IMAGE_THUNK_DATA*)(base + (id->OriginalFirstThunk ? id->OriginalFirstThunk : id->FirstThunk));
        ft = (IMAGE_THUNK_DATA*)(base + id->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;
            void *orig;
            if (ot->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME*)(base + ot->u1.AddressOfData);
            if (strcmp((const char*)ibn->Name, func)) continue;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
            orig = (void*)ft->u1.Function;
            ft->u1.Function = (DWORD)(uintptr_t)repl;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            return orig;
        }
    }
    return NULL;
}

/* ---------------- 訳のデータ（リソース IDR_PATCH、tools/make_patch.py が作る） ----------------
 * 'DKJ2', nglyph, glyph_off, nstr, str_off, nfmt, fmt_off, nglyph8, glyph8_off
 * glyph: u32 cp, u8 rows[12]（左が MSB）。cp の昇順。glyph8: u32 cp, u8 rows[8]（美咲 8x8、同じ並び）
 * str/fmt: u32 en_off, u32 jp_off（UTF-8、0 終わり）
 */
typedef struct { uint32_t cp; uint8_t rows[12]; } glyph_t;
typedef struct { uint32_t cp; uint8_t rows[8]; } glyph8_t;
static const glyph8_t *g_glyph8;
typedef struct { uint32_t en, jp; } pair_t;
static const uint8_t *g_blob;
static const glyph_t *g_glyph;
static uint32_t g_nglyph, g_nstr, g_nfmt;
static const pair_t *g_str, *g_fmt;
static uint32_t *g_htab, g_hmask;

static uint32_t fnv(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

static int load_patch(HMODULE self)
{
    HRSRC r = FindResourceA(self, MAKEINTRESOURCEA(IDR_PATCH), (LPCSTR)RT_RCDATA);
    const uint32_t *h;
    uint32_t i, n;
    if (!r) return 0;
    g_blob = (const uint8_t*)LockResource(LoadResource(self, r));
    if (!g_blob || SizeofResource(self, r) < 36 || memcmp(g_blob, "DKJ2", 4)) return 0;
    h = (const uint32_t*)g_blob;
    g_nglyph = h[1]; g_glyph = (const glyph_t*)(g_blob + h[2]);
    g_nstr = h[3]; g_str = (const pair_t*)(g_blob + h[4]);
    g_nfmt = h[5]; g_fmt = (const pair_t*)(g_blob + h[6]);
    g_glyph8 = h[7] == g_nglyph ? (const glyph8_t*)(g_blob + h[8]) : NULL;
    for (n = 64; n < g_nstr * 2; n <<= 1);
    g_hmask = n - 1;
    g_htab = (uint32_t*)calloc(n, 4);
    for (i = 0; i < g_nstr; i++) {
        uint32_t k = fnv((const char*)g_blob + g_str[i].en) & g_hmask;
        while (g_htab[k]) k = (k + 1) & g_hmask;
        g_htab[k] = i + 1;
    }
    dlog("patch: %u glyphs, %u strings, %u formats\n", g_nglyph, g_nstr, g_nfmt);
    return 1;
}

static const char *lookup(const char *s)
{
    uint32_t k;
    if (!g_htab || !*s) return NULL;
    k = fnv(s) & g_hmask;
    while (g_htab[k]) {
        const pair_t *p = &g_str[g_htab[k] - 1];
        if (!strcmp((const char*)g_blob + p->en, s)) return (const char*)g_blob + p->jp;
        k = (k + 1) & g_hmask;
    }
    return NULL;
}

/* ---- 書式の照合: en の書式に s が合えば、取り出した値で jp の書式を埋める ---- */
typedef struct { const char *p; int n; char conv; } cap_t;
#define MAXCAP 8

static const char *spec(const char *f, int *left, int *width, char *conv)
{
    /* f は '%' の次。[n$][-][0-9]*[l]conv */
    *left = 0; *width = 0;
    if (*f == '-') { *left = 1; f++; }
    while (*f >= '0' && *f <= '9') *width = *width * 10 + (*f++ - '0');
    if (*f == 'l') f++;
    *conv = *f;
    return *f ? f + 1 : f;
}

static int fmatch(const char *f, const char *s, cap_t *cap, int nc)
{
    while (*f) {
        if (*f == '%' && f[1] == '%') { if (*s != '%') return -1; f += 2; s++; continue; }
        if (*f == '%') {
            int left, width; char conv;
            const char *nf = spec(f + 1, &left, &width, &conv);
            if (nc >= MAXCAP) return -1;
            if (conv == 's') {
                int k, len = (int)strlen(s), r;
                for (k = 1; k <= len; k++) {
                    cap[nc].p = s; cap[nc].n = k; cap[nc].conv = 's';
                    if ((r = fmatch(nf, s + k, cap, nc + 1)) >= 0) return r;
                }
                return -1;
            } else if (conv == 'd' || conv == 'u' || conv == 'i') {
                const char *q = s;
                while (*q == ' ') q++;
                if (*q == '-' || *q == '+') q++;
                if (!(*q >= '0' && *q <= '9')) return -1;
                while (*q >= '0' && *q <= '9') q++;
                if (left) while (*q == ' ' && q - s < width) q++;
                cap[nc].p = s; cap[nc].n = (int)(q - s); cap[nc].conv = 'd';
                return fmatch(nf, q, cap, nc + 1);
            } else if (conv == 'c') {
                if (!*s) return -1;
                cap[nc].p = s; cap[nc].n = 1; cap[nc].conv = 'c';
                return fmatch(nf, s + 1, cap, nc + 1);
            }
            return -1;
        }
        if (*f != *s) return -1;
        f++; s++;
    }
    return *s ? -1 : nc;
}

static const char *translate(const char *s);

static int fbuild(const char *jf, cap_t *cap, int nc, char *out, int cap_out)
{
    int o = 0, seq = 0;
    while (*jf && o < cap_out - 1) {
        if (*jf == '%' && jf[1] == '%') { out[o++] = '%'; jf += 2; continue; }
        if (*jf == '%') {
            int left, width, idx = seq; char conv;
            const char *q = jf + 1;
            if (*q >= '1' && *q <= '9' && q[1] == '$') { idx = *q - '1'; q += 2; }
            else seq++;
            jf = spec(q, &left, &width, &conv);
            if (idx < nc) {
                char tmp[512];
                const char *v;
                int n = cap[idx].n < 511 ? cap[idx].n : 511;
                memcpy(tmp, cap[idx].p, n); tmp[n] = 0;
                v = tmp;
                if (cap[idx].conv == 's') { const char *t = translate(tmp); if (t) v = t; }
                else if (cap[idx].conv == 'd') { while (*v == ' ') v++; n = (int)strlen(v); while (n && v[n - 1] == ' ') n--; tmp[(v - tmp) + n] = 0; }
                while (*v && o < cap_out - 1) out[o++] = *v++;
            }
            continue;
        }
        out[o++] = *jf++;
    }
    out[o] = 0;
    return o;
}

/* 組み立てた訳の置き場（描く間だけ使えればよい） */
static char g_ring[8][1024];
static int g_ringi;
static int g_depth;

static const char *translate(const char *s)
{
    const char *t;
    uint32_t i;
    if (!g_blob || !s || !*s) return NULL;
    if ((t = lookup(s))) return t;
    if (g_depth > 2) return NULL;
    for (i = 0; i < g_nfmt; i++) {
        cap_t cap[MAXCAP];
        const char *ef = (const char*)g_blob + g_fmt[i].en;
        int nc;
        /* 先頭の固定部分で早めに外す */
        if (*ef != '%' && *ef != *s) continue;
        nc = fmatch(ef, s, cap, 0);
        if (nc >= 0) {
            char *out = g_ring[g_ringi++ & 7];
            g_depth++;
            fbuild((const char*)g_blob + g_fmt[i].jp, cap, nc, out, sizeof g_ring[0]);
            g_depth--;
            return out;
        }
    }
    return NULL;
}

/* 訳の無い文字列を一度ずつ記録（テスト用） */
static uint32_t g_seen[4096];
static void note_missing(const char *s)
{
    uint32_t h, k;
    const char *q;
    if (!g_logon || !*s) return;
    for (q = s; *q; q++) if ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z')) break;
    if (!*q) return;
    h = fnv(s) | 1; k = h & 4095;
    while (g_seen[k]) { if (g_seen[k] == h) return; k = (k + 1) & 4095; }
    g_seen[k] = h;
    dlog("MISSING [%s]\n", s);
}

static const char *tr_or_orig(const char *s)
{
    const char *t = translate(s);
    if (!t) { note_missing(s); return s; }
    return t;
}

/* ---------------- ゲーム内の文字描画 ---------------- */
static uint8_t *g_font;               /* 24 バイト/文字: i16 幅, u16 行[11] */
static uint8_t **g_pbuf;
static int *g_ppitch, *g_pclipx, *g_pclipy;
static int g_nojp;

static uint32_t utf8_next(const uint8_t **pp)
{
    const uint8_t *p = *pp;
    uint32_t c = *p++;
    if (c >= 0xf0 && p[0] && p[1] && p[2]) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
    else if (c >= 0xe0 && p[0] && p[1]) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
    else if (c >= 0xc0 && p[0]) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
    *pp = p;
    return c;
}

static int find_gi(uint32_t cp)
{
    int lo = 0, hi = (int)g_nglyph - 1;
    while (lo <= hi) {
        int m = (lo + hi) >> 1;
        if (g_glyph[m].cp == cp) return m;
        if (g_glyph[m].cp < cp) lo = m + 1; else hi = m - 1;
    }
    return -1;
}
static const glyph_t *find_glyph(uint32_t cp)
{
    int i = find_gi(cp);
    return i >= 0 ? &g_glyph[i] : NULL;
}

static int char_adv(uint32_t c)
{
    if (c < 0x80) return *(int16_t*)(g_font + (int)(signed char)c * 24) + 1;
    if (c >= 0xff61 && c <= 0xff9f) return 5;
    return 8;
}

static __inline void pset(int x, int y, uint8_t col)
{
    if (x < 0 || y < 0 || x > *g_pclipx || y > *g_pclipy) return;
    (*g_pbuf)[y * *g_ppitch + x] = col;
}

static void draw_utf8(int x, int y, int color, const uint8_t *p)
{
    while (*p) {
        uint32_t c = utf8_next(&p);
        if (c < 0x80) {
            const uint8_t *g = g_font + (int)(signed char)c * 24;
            int w = *(int16_t*)g, r, i;
            for (r = 0; r < 11; r++) {
                uint16_t bits = *(uint16_t*)(g + 2 + r * 2);
                for (i = 0; i < w && bits; i++, bits <<= 1) if (bits & 0x8000) pset(x + i, y + r, (uint8_t)color);
            }
            x += w + 1;
        } else {
            const glyph_t *g = find_glyph(c);
            if (g) {
                int r, i;
                for (r = 0; r < 12; r++) {
                    uint8_t bits = g->rows[r];
                    for (i = 0; bits; i++, bits <<= 1) if (bits & 0x80) pset(x + i, y + r, (uint8_t)color);
                }
            }
            x += char_adv(c);
        }
    }
}

static void __cdecl my_draw(int x, int y, int color, const char *str)
{
    draw_utf8(x, y, color, (const uint8_t*)tr_or_orig(str));
}

/* 字幕（subs.c）から使う: 訳さずにそのまま描く・幅を測る、ゲームの画面バッファ、コードのパターン探し */
void jp_draw_raw(int x, int y, int color, const char *utf8) { draw_utf8(x, y, color, (const uint8_t*)utf8); }
int jp_width_raw(const char *utf8)
{
    const uint8_t *p = (const uint8_t*)utf8;
    int w = 0;
    while (*p) w += char_adv(utf8_next(&p));
    return w;
}
uint8_t *jp_frame_buffer(int *pitch)
{
    if (!g_pbuf || !*g_pbuf) return NULL;
    *pitch = *g_ppitch;
    return *g_pbuf;
}

/* 8x8 の文字（インベントリの数値・名前、地図など）: ASCII はゲームの 8x8、日本語は美咲 */
static uint8_t *g_font8;
static void __cdecl my_draw8(int x, int y, int color, const char *str)
{
    const uint8_t *p = (const uint8_t*)tr_or_orig(str);
    while (*p) {
        uint32_t c = utf8_next(&p);
        const uint8_t *rows = NULL;
        int r, i;
        if (c < 0x80) rows = g_font8 + (int)(signed char)c * 8;
        else if (g_glyph8) { int gi = find_gi(c); if (gi >= 0) rows = g_glyph8[gi].rows; }
        if (rows)
            for (r = 0; r < 8; r++) {
                uint8_t bits = rows[r];
                for (i = 0; bits; i++, bits <<= 1) if (bits & 0x80) pset(x + i, y + r, (uint8_t)color);
            }
        x += 8;
    }
}

static int __cdecl my_width(const char *str)
{
    const uint8_t *p = (const uint8_t*)tr_or_orig(str);
    int w = 0;
    while (*p) w += char_adv(utf8_next(&p));
    return w;
}

/* パターン探し（?? は 0x100） */
static uint8_t *find_pat(uint8_t *from, uint8_t *to, const uint16_t *pat, int n)
{
    uint8_t *p;
    int i;
    for (p = from; p + n <= to; p++) {
        for (i = 0; i < n; i++) if (pat[i] < 0x100 && p[i] != pat[i]) break;
        if (i == n) return p;
    }
    return NULL;
}

static void put_jmp(uint8_t *at, void *to)
{
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    at[0] = 0xe9;
    *(int32_t*)(at + 1) = (int32_t)((uint8_t*)to - (at + 5));
    VirtualProtect(at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
}

#define W 0x100
static const uint16_t PAT_DRAW[] = {
    0x8b,0x44,0x24,0x10, 0x83,0xec,0x08, 0x53, 0x80,0x38,0x00, 0x56, 0x57, 0x55, 0x0f,0x84,W,W,W,W,
    0x8b,0x4c,0x24,0x1c, 0x8b,0x44,0x24,0x28, 0x33,0xd2, 0x8a,0x00, 0x0f,0xbe,0xf0, 0x8d,0x04,0x76, 0x8d,0x1c,0x76,
    0xc1,0xe0,0x02, 0x8d,0x2c,0xdd,W,W,W,W };
static const uint16_t PAT_DRAW8[] = {
    0x8b,0x44,0x24,0x10, 0x83,0xec,0x04, 0x53, 0x80,0x38,0x00, 0x56, 0x57, 0x55, 0x74,W,
    0x8b,0x54,0x24,0x18, 0x8b,0x74,0x24,0x1c, 0x8a,0x4c,0x24,0x20, 0x8b,0x44,0x24,0x24, 0x0f,0xbe,0x18,
    0x8d,0x2c,0xdd,W,W,W,W };
static const uint16_t PAT_WIDTH[] = {
    0x33,0xc0, 0x8b,0x54,0x24,0x04, 0x38,0x02, 0x74,W, 0x0f,0xbe,0x0a, 0x42, 0x8d,0x0c,0x49,
    0x0f,0xbf,0x0c,0xcd,W,W,W,W, 0x41, 0x03,0xc1, 0x80,0x3a,0x00, 0x75,W, 0xc3 };

static uint8_t *g_t0, *g_t1;
uint8_t *find_code(const uint16_t *pat, int n)
{
    if (!g_t0) {
        uint8_t *base = (uint8_t*)GetModuleHandleA(NULL);
        IMAGE_NT_HEADERS *nh = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
        IMAGE_SECTION_HEADER *sh = IMAGE_FIRST_SECTION(nh);
        int i;
        for (i = 0; i < nh->FileHeader.NumberOfSections; i++)
            if (sh[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) { g_t0 = base + sh[i].VirtualAddress; g_t1 = g_t0 + sh[i].Misc.VirtualSize; break; }
        if (!g_t0) return NULL;
    }
    return find_pat(g_t0, g_t1, pat, n);
}

static int install_draw(void)
{
    uint8_t *t0, *t1, *d, *w, *q;
    if (!find_code(PAT_WIDTH, 1)) return 0;
    t0 = g_t0; t1 = g_t1;
    d = find_pat(t0, t1, PAT_DRAW, sizeof PAT_DRAW / 2);
    w = find_pat(t0, t1, PAT_WIDTH, sizeof PAT_WIDTH / 2);
    if (!d || !w) { dlog("draw/width not found %p %p\n", d, w); return 0; }
    g_font = *(uint8_t**)(d + 0x2f);
    if (*(uint8_t**)(w + 0x15) != g_font) { dlog("font mismatch\n"); return 0; }
    /* 関数内の cmp [clipx],eax / cmp [clipy],ebx / imul ebx,[pitch] / mov ebp,[buf] */
    g_pclipx = g_pclipy = g_ppitch = NULL; g_pbuf = NULL;
    for (q = d + 0x40; q < d + 0xb0; q++) {
        if (!g_pclipx && q[0] == 0x39 && q[1] == 0x05) g_pclipx = *(int**)(q + 2);
        else if (!g_pclipy && q[0] == 0x39 && q[1] == 0x1d) g_pclipy = *(int**)(q + 2);
        else if (!g_ppitch && q[0] == 0x0f && q[1] == 0xaf && q[2] == 0x1d) g_ppitch = *(int**)(q + 3);
        else if (!g_pbuf && q[0] == 0x8b && q[1] == 0x2d) g_pbuf = *(uint8_t***)(q + 2);
    }
    if (!g_pclipx || !g_pclipy || !g_ppitch || !g_pbuf) { dlog("draw operands not found\n"); return 0; }
    put_jmp(d, my_draw);
    put_jmp(w, my_width);
    q = find_pat(t0, t1, PAT_DRAW8, sizeof PAT_DRAW8 / 2);
    if (q) { g_font8 = *(uint8_t**)(q + 0x26); put_jmp(q, my_draw8); dlog("draw8 %p font8 %p\n", q, g_font8); }
    else dlog("draw8 not found\n");
    dlog("draw %p width %p font %p buf %p pitch %p clip %p %p\n", d, w, g_font, g_pbuf, g_ppitch, g_pclipx, g_pclipy);
    return 1;
}

/* ---------------- GDI（キー設定・エラー画面）と MessageBox ---------------- */
static int utf8_to_w(const char *s, int n, WCHAR *out, int cap)
{
    return MultiByteToWideChar(CP_UTF8, 0, s, n, out, cap);
}

static int g_jpfont_used;
static HFONT WINAPI my_CreateFontA(int h, int w, int esc, int ori, int wt, DWORD it, DWORD ul, DWORD so, DWORD cs,
                                   DWORD op, DWORD cp, DWORD q, DWORD pf, LPCSTR face)
{
    g_jpfont_used = 1;
    (void)cs; (void)q; (void)face; (void)h; (void)w; (void)wt;
    return CreateFontW(-10, 0, esc, ori, FW_NORMAL, it, ul, so, SHIFTJIS_CHARSET, op, cp, NONANTIALIASED_QUALITY, pf, L"MS Gothic");
}

static BOOL WINAPI my_TextOutA(HDC dc, int x, int y, LPCSTR s, int n)
{
    char buf[1024];
    WCHAR wb[1024];
    const char *t;
    int m;
    if (n < 0 || n > 1000) return TextOutA(dc, x, y, s, n);
    memcpy(buf, s, n); buf[n] = 0;
    t = tr_or_orig(buf);
    m = utf8_to_w(t, -1, wb, 1024);
    return TextOutW(dc, x, y, wb, m > 0 ? m - 1 : 0);
}

static LONG WINAPI my_TabbedTextOutA(HDC dc, int x, int y, LPCSTR s, int n, int nt, const INT *tabs, int org)
{
    char buf[1024], out[2048];
    WCHAR wb[2048];
    char *p, *q;
    int o = 0, m;
    if (n < 0 || n > 1000) return TabbedTextOutA(dc, x, y, s, n, nt, tabs, org);
    memcpy(buf, s, n); buf[n] = 0;
    for (p = buf; ; p = q + 1) {
        const char *t;
        int k;
        q = strchr(p, '\t');
        if (q) *q = 0;
        t = tr_or_orig(p);
        k = (int)strlen(t);
        if (o + k + 2 < (int)sizeof out) { memcpy(out + o, t, k); o += k; }
        if (!q) break;
        out[o++] = '\t';
    }
    out[o] = 0;
    m = utf8_to_w(out, -1, wb, 2048);
    return TabbedTextOutW(dc, x, y, wb, m > 0 ? m - 1 : 0, nt, tabs, org);
}

static int WINAPI my_MessageBoxA(HWND h, LPCSTR text, LPCSTR cap, UINT type)
{
    WCHAR wt[1024], wc[256];
    const char *t = text ? tr_or_orig(text) : "", *c = cap ? tr_or_orig(cap) : "";
    utf8_to_w(t, -1, wt, 1024);
    utf8_to_w(c, -1, wc, 256);
    return MessageBoxW(h, wt, wc, type);
}

/* ---------------- 画像の差し替え ----------------
 * GLOBALS の NTERFACE フォルダの PCX を開く時、DLL に入れた日本語版（リソース IDR_IMAGES）を
 * %LOCALAPPDATA% の DK_JP/img に書き出して、そちらを開かせる */
static const uint8_t *g_img;
static uint32_t g_nimg;
static char g_imgdir[MAX_PATH];
static uint8_t g_written[256];
static HANDLE (WINAPI *o_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

static int img_path(const char *path, char *out)
{
    const char *b = strrchr(path, '\\'), *q;
    uint32_t i;
    if (!b || b == path || !g_img) return 0;
    for (q = b - 1; q > path && *q != '\\'; q--);
    if (_strnicmp(q, "\\nterface\\", 10)) return 0;
    b++;
    for (i = 0; i < g_nimg && i < 256; i++) {
        const uint8_t *e = g_img + 8 + i * 24;
        if (_stricmp((const char*)e, b)) continue;
        wsprintfA(out, "%s\\%s", g_imgdir, (const char*)e);
        if (!g_written[i]) {
            HANDLE h = o_CreateFileA(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            DWORD n;
            if (h == INVALID_HANDLE_VALUE) return 0;
            WriteFile(h, g_img + *(uint32_t*)(e + 16), *(uint32_t*)(e + 20), &n, NULL);
            CloseHandle(h);
            g_written[i] = 1;
        }
        return 1;
    }
    return 0;
}

static HANDLE WINAPI my_CreateFileA(LPCSTR path, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD fl, HANDLE tmpl)
{
    char np[MAX_PATH];
    if (path && !(acc & GENERIC_WRITE) && img_path(path, np)) {
        dlog("image %s -> %s\n", path, np);
        return o_CreateFileA(np, acc, share, sa, disp, fl, tmpl);
    }
    return o_CreateFileA(path, acc, share, sa, disp, fl, tmpl);
}

static void load_images(HMODULE self)
{
    HRSRC r = FindResourceA(self, MAKEINTRESOURCEA(IDR_IMAGES), (LPCSTR)RT_RCDATA);
    char t[MAX_PATH];
    if (!r) return;
    g_img = (const uint8_t*)LockResource(LoadResource(self, r));
    if (!g_img || memcmp(g_img, "DKIM", 4)) { g_img = NULL; return; }
    g_nimg = *(uint32_t*)(g_img + 4);
    if (!GetEnvironmentVariableA("LOCALAPPDATA", t, MAX_PATH)) GetTempPathA(MAX_PATH, t);
    wsprintfA(g_imgdir, "%s\\DK_JP", t);
    CreateDirectoryA(g_imgdir, NULL);
    lstrcatA(g_imgdir, "\\img");
    CreateDirectoryA(g_imgdir, NULL);
    dlog("images: %u in %s\n", g_nimg, g_imgdir);
}

static void init(HMODULE self)
{
    char t[MAX_PATH], v[8];
    HMODULE exe = GetModuleHandleA(NULL);
    GetTempPathA(MAX_PATH, t);
    wsprintfA(g_dir, "%sDK_JP", t);
    CreateDirectoryA(g_dir, NULL);
    if (GetEnvironmentVariableA("DK_TEST", v, sizeof v) && v[0] == '1') g_test = 1;
    if ((GetEnvironmentVariableA("DK_LOG", v, sizeof v) && v[0] == '1') || g_test) g_logon = 1;
    if (GetEnvironmentVariableA("DK_NOJP", v, sizeof v) && v[0] == '1') g_nojp = 1;
    dlog("DeathKeep JP patch loaded\n");
    if (g_nojp) return;
    if (!load_patch(self)) { dlog("no patch data\n"); return; }
    if (!install_draw()) return;
    subs_init(self);
    load_images(self);
    if (g_img) {
        o_CreateFileA = CreateFileA;
        iat_hook(exe, "KERNEL32.dll", "CreateFileA", (void*)my_CreateFileA);
    }
    iat_hook(exe, "GDI32.dll", "CreateFontA", (void*)my_CreateFontA);
    iat_hook(exe, "GDI32.dll", "TextOutA", (void*)my_TextOutA);
    iat_hook(exe, "USER32.dll", "TabbedTextOutA", (void*)my_TabbedTextOutA);
    iat_hook(exe, "USER32.dll", "MessageBoxA", (void*)my_MessageBoxA);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        init(h);
        testio_init();
    }
    return TRUE;
}
