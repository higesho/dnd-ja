/*
 * Fantasy Empires 日本語化パッチ（実行時部分）
 * DOSBox（Steam 版の SVN 版 / 0.74-3 / GOG の 0.74-2.1）に winmm.dll の代理として読み込まれる。ゲームのファイルは書き換えない。
 * DOSBox が FANTASY.EXE を開くとき（fopen）に、手を入れたコピー（%LOCALAPPDATA%\FE_JP）を渡す。コピーには次の所に
 * DOSBox のコールバック（FE 38 nn 00）を置いてある（番地はリンク時のもの）:
 *   0  0x15cc5 折り返しありの文字列の描画   (eax=文字列, edx=x, ebx=y, ecx=描き先, [esp+4]=字体, [esp+8]=右端) ret 8
 *   1  0x15ffc 1 行の文字列の描画           (eax=文字列, edx=x, ebx=y, ecx=描き先, [esp+4]=字体) ret 4
 *   2,3 0x16273 DM の語り（下の帯に 1 文ずつ出して待つ）を「準備 / 1 ページ描く → 待つ」の繰り返しに置き換え
 *   4,5 0x28aed DM の声（口の動き）の入口と、終わりの判定
 *   6  0x42c71 効果音・声の再生（al = VOC の番号）。DM の声なら下の帯に字幕を出す
 * 訳は描くときに英語の文字列から引く（完全一致 → %s/%d の雛形 → 知っている断片の連結）。
 * 日本語は美咲フォント（8x8）、英数字はゲームの字形（画面ごとの部品画像の下にある）で描く。字の色は部品画像の字から取る。
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "resource.h"

void proxy_init(void);
#ifdef FE_TEST
void testio_init(void);
void testio_mouse(int x, int y);
#endif

static char g_dir[MAX_PATH];
#ifndef FE_TEST
/* 配布版: ログは書かない（ログ・画面の撮影・入力の差し込みなどの試験用の仕組みは、build.bat test の試験版にだけ入る） */
#define dbg(...) ((void)0)
#define log_str(tag, s, n) ((void)0)
#else
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
#endif

/* ------------------------------------------------ DOSBox の中身 */
static uint32_t g_dbx, g_dbx_code;   /* Steam 0.74-3 = 0、GOG 0.74-2.1 = +0xe000 / -0x10、SVN 版 = +0x3b000 / +0xbc0 */
#define A_CB_HANDLERS (0x74d460u + g_dbx)
#define A_CB_ILLEGAL (0x415e40u + g_dbx_code)
#define A_REGS (0x74d1e0u + g_dbx)
#define A_SEGPHYS (0x74d110u + g_dbx)
#define A_TLB_READ (0x74d6d0u + g_dbx)
#define A_TLB_WRITE (0xb4d6d0u + g_dbx)
#define A_TLB_RH (0xf4d6d0u + g_dbx)
#define A_TLB_WH (0x134d6d0u + g_dbx)
typedef uint32_t (__cdecl *ph_rb)(void *self, uint32_t a);
typedef void (__cdecl *ph_wb)(void *self, uint32_t a, uint32_t v);
static int verify_dosbox(void)
{
    static const struct { uint32_t s1, s2, s3, dbx, dcode; } builds[] = {
        {0x416a26u, 0x41965fu, 0x431da2u, 0x3b000u, 0xbc0u},       /* SVN 版（Steam の Fantasy Empires に同梱） */
        {0x415e66u, 0x41897fu, 0x4351ccu, 0u, 0u},                 /* 0.74-3 */
        {0x415e56u, 0x41896fu, 0x4351bcu, 0xe000u, 0xfffffff0u},   /* GOG 0.74-2.1 */
    };
    HMODULE h = GetModuleHandleA(NULL);
    int i;
    if ((uintptr_t)h != 0x400000) return 0;
    for (i = 0; i < 3; i++) {
        uint32_t cb = 0x74d460u + builds[i].dbx, ill = 0x415e40u + builds[i].dcode;
        uint32_t tlb = 0x74d6d0u + builds[i].dbx, regs = 0x74d1e0u + builds[i].dbx;
        uint8_t s1[11] = {0x81, 0x3c, 0x85}, s2[7] = {0x8b, 0x04, 0x95}, s3[6] = {0x66, 0xa3};
        MEMORY_BASIC_INFORMATION mi;
        if (!VirtualQuery((void*)(uintptr_t)builds[i].s3, &mi, sizeof mi) || mi.State != MEM_COMMIT) continue;
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
static __inline void wd(uint32_t a, uint32_t v) { wb(a, (uint8_t)v); wb(a + 1, (uint8_t)(v >> 8)); wb(a + 2, (uint8_t)(v >> 16)); wb(a + 3, (uint8_t)(v >> 24)); }
#define REG32(i) (((uint32_t*)A_REGS)[i])
#define R_EAX REG32(0)
#define R_ECX REG32(1)
#define R_EDX REG32(2)
#define R_EBX REG32(3)
#define R_ESP REG32(4)
#define R_EBP REG32(5)
#define R_ESI REG32(6)
#define R_EDI REG32(7)
#define R_EIP REG32(8)
#define PHYS_CS (((uint32_t*)A_SEGPHYS)[1])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
static __inline void push32(uint32_t v) { R_ESP -= 4; wd(PHYS_SS + R_ESP, v); }
static __inline uint32_t pop32(void) { uint32_t v = rd(PHYS_SS + R_ESP); R_ESP += 4; return v; }
static __inline uint32_t arg(int k) { return rd(PHYS_SS + R_ESP + 4 * k); }   /* [esp + 4k]（k=0 は戻り先） */

/* ------------------------------------------------------- パッチデータ */
static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;
static const uint8_t *g_epatch;
static int g_cb_base = 0x70;
static int g_enabled;
static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xffffffffu, k;
    while (n--) { c ^= *p++; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1))); }
    return ~c;
}

typedef struct { uint64_t h; uint16_t enlen, jalen; uint8_t flags; const uint16_t *ja; } tentry;
typedef struct { const uint8_t *en; uint16_t enlen, jalen; uint8_t flags; const uint16_t *ja; } pentry;
static tentry *g_tr; static uint32_t g_ntr;
static int32_t *g_hash; static uint32_t g_hmask;
static pentry *g_pt; static uint32_t g_npt;
typedef struct { uint64_t h; uint32_t at; uint16_t enlen, jalen; const uint16_t *ja; } centry;   /* 呼び出し元ごとの訳 */
static centry *g_ct; static uint32_t g_nct;
static const uint16_t *g_voice[256]; static uint16_t g_voicelen[256];
static const uint8_t *g_gbits[65536];     /* 字形（8 バイト） */
static const uint8_t *g_gbits6[65536];    /* 字形（6 行） */
static uint8_t g_gflags[65536];           /* 1 行頭禁則 2 行末禁則 4 半角 */
static uint8_t g_minlen_ok;

static uint64_t fnv64(const uint8_t *p, int n)
{
    uint64_t h = 14695981039346656037ull;
    while (n--) h = (h ^ *p++) * 1099511628211ull;
    return h;
}
static void load_T(const uint8_t *d)
{
    uint32_t n = *(const uint32_t*)d, i, sz = 1;
    const uint8_t *p = d + 4;
    g_tr = (tentry*)calloc(n ? n : 1, sizeof(tentry));
    while (sz < n * 2 + 2) sz <<= 1;
    g_hash = (int32_t*)calloc(sz, 4);
    g_hmask = sz - 1;
    for (i = 0; i < n; i++) {
        tentry *e = &g_tr[i];
        uint32_t h;
        memcpy(&e->h, p, 8);
        e->enlen = *(const uint16_t*)(p + 8);
        e->flags = p[10];
        e->jalen = *(const uint16_t*)(p + 12);
        e->ja = (const uint16_t*)(p + 14);
        p += 14 + e->jalen * 2;
        h = (uint32_t)e->h & g_hmask;
        while (g_hash[h]) h = (h + 1) & g_hmask;
        g_hash[h] = (int32_t)i + 1;
    }
    g_ntr = n;
}
static void load_P(const uint8_t *d)
{
    uint32_t n = *(const uint32_t*)d, i;
    const uint8_t *p = d + 4;
    g_pt = (pentry*)calloc(n ? n : 1, sizeof(pentry));
    for (i = 0; i < n; i++) {
        pentry *e = &g_pt[i];
        e->enlen = *(const uint16_t*)p;
        e->flags = p[2];
        e->jalen = *(const uint16_t*)(p + 4);
        e->en = p + 6;
        p += 6 + e->enlen + (e->enlen & 1);
        e->ja = (const uint16_t*)p;
        p += e->jalen * 2;
    }
    g_npt = n;
}
static void load_C(const uint8_t *d)
{
    uint32_t n = *(const uint32_t*)d, i;
    const uint8_t *p = d + 4;
    g_ct = (centry*)calloc(n ? n : 1, sizeof(centry));
    for (i = 0; i < n; i++) {
        centry *e = &g_ct[i];
        memcpy(&e->h, p, 8);
        e->at = *(const uint32_t*)(p + 8);
        e->enlen = *(const uint16_t*)(p + 12);
        e->jalen = *(const uint16_t*)(p + 14);
        e->ja = (const uint16_t*)(p + 16);
        p += 16 + e->jalen * 2;
    }
    g_nct = n;
}
static void load_V(const uint8_t *d)
{
    uint32_t n = *(const uint32_t*)d, i;
    const uint8_t *p = d + 4;
    for (i = 0; i < n; i++) {
        uint16_t k = *(const uint16_t*)p, l = *(const uint16_t*)(p + 2);
        if (k < 256) { g_voice[k] = (const uint16_t*)(p + 4); g_voicelen[k] = l; }
        p += 4 + l * 2;
    }
}
static void load_G(const uint8_t *d)
{
    uint32_t n = *(const uint32_t*)d, i;
    const uint8_t *p = d + 4;
    for (i = 0; i < n; i++, p += 20) {
        uint16_t c = *(const uint16_t*)p;
        g_gflags[c] = p[2];
        g_gbits[c] = p + 4;          /* 8 行（美咲そのまま） */
        g_gbits6[c] = p + 12;        /* 6 行に縮めたもの（小さい字の場面） */
    }
}
static int load_blob(HMODULE self)
{
    HRSRC r = FindResourceA(self, MAKEINTRESOURCEA(IDR_PATCH), (LPCSTR)RT_RCDATA);
    HGLOBAL g;
    const uint8_t *p, *end;
    uint32_t ns, i;
    if (!r) return 0;
    g = LoadResource(self, r);
    g_blob = (const uint8_t*)LockResource(g);
    g_blob_len = SizeofResource(self, r);
    if (!g_blob || g_blob_len < 8 || memcmp(g_blob, "FEJ1", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len) ^ 0x2u;    /* 末尾の数 = DLL のコードの版（変えると EXE のコピーを作り直す） */
    ns = *(const uint32_t*)(g_blob + 4);
    p = g_blob + 8; end = g_blob + g_blob_len;
    for (i = 0; i < ns && p + 8 <= end; i++) {
        uint8_t t = p[0];
        uint32_t len = *(const uint32_t*)(p + 1);
        const uint8_t *d = p + 5;
        if (t == 'T') load_T(d);
        else if (t == 'P') load_P(d);
        else if (t == 'V') load_V(d);
        else if (t == 'C') load_C(d);
        else if (t == 'G') load_G(d);
        else if (t == 'E') g_epatch = d;
        p = d + len;
    }
    return g_tr && g_epatch;
}

static const tentry *lookup(const uint8_t *s, int n)
{
    uint64_t h;
    uint32_t i;
    if (!g_hash || n <= 0) return NULL;
    h = fnv64(s, n);
    i = (uint32_t)h & g_hmask;
    while (g_hash[i]) {
        const tentry *e = &g_tr[g_hash[i] - 1];
        if (e->h == h && e->enlen == n) return e;
        i = (i + 1) & g_hmask;
    }
    return NULL;
}
#ifdef FE_TEST
static void log_str(const char *tag, const uint8_t *s, int n)
{
    char buf[600];
    int i;
    if (!g_logon) return;
    for (i = 0; i < n && i < 590; i++) buf[i] = (char)((s[i] >= 0x20 && s[i] < 0x7f) ? s[i] : (s[i] == '\n' ? '|' : '.'));
    buf[i] = 0;
    dbg("%s [%s]\n", tag, buf);
}
#endif

/* ------------------------------------------------------------ 訳を引く */
#define OUTMAX 1024
typedef struct { uint16_t c[OUTMAX]; int n; uint8_t flags; } jstr;
static void js_put(jstr *o, uint16_t c) { if (o->n < OUTMAX) o->c[o->n++] = c; }
static void js_ascii(jstr *o, const uint8_t *s, int n) { int i; for (i = 0; i < n; i++) js_put(o, s[i]); }
static void js_cat(jstr *o, const uint16_t *s, int n) { int i; for (i = 0; i < n; i++) js_put(o, s[i]); }
static int is_alnum(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

static int translate(const uint8_t *s, int n, jstr *o, int depth);

/* 雛形との照合（%s = 何でも（空も可）, %d = 数, %c = 1 字）。args に [始め, 長さ] を入れる */
static int pmatch(const uint8_t *p, int pn, const uint8_t *s, int sn, int *args, int na)
{
    while (pn > 0) {
        if (p[0] == '%' && pn >= 2 && (p[1] == 's' || p[1] == 'd' || p[1] == 'c')) {
            int k;
            if (na >= 9) return -1;
            if (p[1] == 'd') {
                int m = 0;
                if (m < sn && s[m] == '-') m++;
                while (m < sn && s[m] >= '0' && s[m] <= '9') m++;
                if (m == 0) return -1;
                for (k = m; k >= 1; k--) {      /* 数は長い方から */
                    int r;
                    if (k == 1 && s[0] == '-') break;
                    r = pmatch(p + 2, pn - 2, s + k, sn - k, args, na + 1);
                    if (r >= 0) { args[na * 2] = sn; args[na * 2 + 1] = k; return r; }
                }
                return -1;
            }
            if (p[1] == 'c') {
                int r;
                if (sn < 1) return -1;
                r = pmatch(p + 2, pn - 2, s + 1, sn - 1, args, na + 1);
                if (r >= 0) { args[na * 2] = sn; args[na * 2 + 1] = 1; }
                return r;
            }
            for (k = 0; k <= sn; k++) {         /* %s は短い方から */
                int r = pmatch(p + 2, pn - 2, s + k, sn - k, args, na + 1);
                if (r >= 0) { args[na * 2] = sn; args[na * 2 + 1] = k; return r; }
            }
            return -1;
        }
        if (sn <= 0 || *p != *s) return -1;
        p++; pn--; s++; sn--;
    }
    return sn == 0 ? na : -1;
}
/* args[i*2] には「その引数の始まりでの残りの長さ」が入る → 始め = 全体の長さ - それ */
static int try_patterns(const uint8_t *s, int n, jstr *o, int depth, uint8_t *flags)
{
    uint32_t i;
    int args[18];
    for (i = 0; i < g_npt; i++) {
        const pentry *e = &g_pt[i];
        int na = pmatch(e->en, e->enlen, s, n, args, 0), k;
        if (na < 0) continue;
        for (k = 0; k < e->jalen; k++) {
            uint16_t c = e->ja[k];
            if (c >= 1 && c <= 9 && c <= na) {
                int st = n - args[(c - 1) * 2], ln = args[(c - 1) * 2 + 1];
                const uint8_t *a = s + st;
                while (ln > 0 && *a == ' ') { a++; ln--; }
                while (ln > 0 && a[ln - 1] == ' ') ln--;
                if (ln > 0 && !translate(a, ln, o, depth + 1)) js_ascii(o, a, ln);
            } else js_put(o, c);
        }
        *flags = e->flags;
        return 1;
    }
    return 0;
}
/* 知っている断片に分けて訳す（連結された文）。分からない所（名前など）は英語のまま */
static int try_segments(const uint8_t *s, int n, jstr *o)
{
    int i = 0, known = 0, unk = 0, save = o->n;
    uint8_t fl;
    while (i < n) {
        int best = 0, l;
        const tentry *be = NULL;
        if (i == 0 || !is_alnum(s[i - 1]) || !is_alnum(s[i])) {
            for (l = n - i; l >= 2; l--) {
                const tentry *e;
                if (i + l < n && is_alnum(s[i + l - 1]) && is_alnum(s[i + l])) continue;   /* 語の途中で切れる */
                e = lookup(s + i, l);
                if (e) { best = l; be = e; break; }
            }
        }
        if (be) {
            js_cat(o, be->ja, be->jalen);
            i += best; known += best;
        } else if ((i == 0 || !is_alnum(s[i - 1])) && i > 0 && try_patterns(s + i, n - i, o, 1, &fl)) {
            known += n - i;                 /* 残りが %d などの雛形（「   SEA TRAVEL - 」+「TURNS: %d」） */
            i = n;
        } else {
            int j = i + 1;
            while (j < n && is_alnum(s[j - 1]) && is_alnum(s[j])) j++;
            js_ascii(o, s + i, j - i);
            unk += j - i;
            i = j;
        }
    }
    if (known == 0 || known < unk) { o->n = save; return 0; }
    return 1;
}
static int translate(const uint8_t *s, int n, jstr *o, int depth)
{
    const tentry *e = lookup(s, n);
    uint8_t fl = 0;
    if (e) { js_cat(o, e->ja, e->jalen); if (depth == 0) o->flags = e->flags; return 1; }
    if (depth > 3) return 0;
    if (try_patterns(s, n, o, depth, &fl)) { if (depth == 0) o->flags = fl; return 1; }
    if (depth == 0 && try_segments(s, n, o)) {
        if (s[0] == ' ') o->flags |= 1;               /* 先頭に空白（中央寄せのつもりの英語） */
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------ 描く */
static uint32_t g_code_delta, g_data_delta;
static int g_delta_ok;
#define DATA(a) ((a) + g_data_delta)
static void set_delta(uint32_t site)
{
    if (g_delta_ok) return;
    g_code_delta = R_EIP - (site + 4);
    /* 0x15de7: mov eax, 0x780e8（実行時の番地に直されている） */
    g_data_delta = rd(PHYS_CS + 0x15de8 + g_code_delta) - 0x780e8;
    g_delta_ok = 1;
    dbg("code delta %x, data delta %x\n", g_code_delta, g_data_delta);
}

typedef struct { uint32_t sheet; int y0, h, large; uint8_t col[8]; } fontd;
static const struct { char c; short x0, x1; } LARGE_SP[] = {{'!', 311, 316}, {'+', 296, 301}, {'-', 288, 294}, {'.', 304, 309}};
static const struct { char c; short x0, x1; } SMALL_SP[] = {{'!', 156, 157}, {'\'', 169, 171}, {',', 172, 174}, {'+', 148, 150},
                                                         {'-', 144, 146}, {'.', 153, 156}, {':', 163, 166}, {'?', 159, 162}};
static int sheet_glyph(const fontd *f, uint8_t c, int *x0, int *x1)
{
    int i;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (f->large) {
        if (c >= 'A' && c <= 'Z') { *x0 = (c - 'A') * 8; *x1 = *x0 + 7; return 1; }
        if (c >= '0' && c <= '9') { *x0 = 0xd0 + (c - '0') * 8; *x1 = *x0 + 7; return 1; }
        for (i = 0; i < 4; i++) if (LARGE_SP[i].c == c) { *x0 = LARGE_SP[i].x0; *x1 = LARGE_SP[i].x1; return 1; }
    } else {
        if (c >= 'A' && c <= 'Z') { *x0 = (c - 'A') * 4; *x1 = *x0 + 3; return 1; }
        if (c >= '0' && c <= '9') { *x0 = 0x68 + (c - '0') * 4; *x1 = *x0 + 3; return 1; }
        for (i = 0; i < 8; i++) if (SMALL_SP[i].c == c) { *x0 = SMALL_SP[i].x0; *x1 = SMALL_SP[i].x1; return 1; }
    }
    return 0;
}
/* 字の色: 部品画像の字の行ごとに一番多い色 */
static void make_font(fontd *f, uint32_t sheet, int y0, int large)
{
    uint32_t ptr = rd(PHYS_DS + sheet), w = rd(PHYS_DS + sheet + 4);
    int r, x, xe;
    uint8_t last = 15;
    f->sheet = sheet; f->y0 = y0; f->large = large; f->h = large ? 7 : 5;
    xe = large ? 26 * 8 : 26 * 4;
    for (r = 0; r < f->h; r++) {
        int cnt[256], best = 0, bc = 0;
        memset(cnt, 0, sizeof cnt);
        for (x = 0; x < xe; x++) cnt[rb(PHYS_DS + ptr + (y0 + r) * w + x)]++;
        for (x = 1; x < 256; x++) if (cnt[x] > best) { best = cnt[x]; bc = x; }
        f->col[r] = best ? (uint8_t)bc : last;
        last = f->col[r];
    }
}
static int g_condense;          /* 1: 全角の送りを 7 ドットに詰める（狭い欄。訳の先頭の 0x07 で指定） */
static int adv_of(const fontd *f, uint16_t c)
{
    if (c >= 0x80) return (g_gflags[c] & 4) ? 4 : (g_condense ? 7 : 8);
    if (c == '.') return f->large ? 5 : 2;
    return f->large ? 7 : 4;
}
typedef struct { uint32_t ptr, w; int clipw, cliph; } dest;
static void dest_of(dest *d, uint32_t st)
{
    d->ptr = rd(PHYS_DS + st); d->w = rd(PHYS_DS + st + 4);
    d->clipw = d->w > 0 && d->w <= 1024 ? (int)d->w : 320; d->cliph = 200;
}
static void put_px(const dest *d, int x, int y, uint8_t c)
{
    if (x < 0 || y < 0 || x >= d->clipw || y >= d->cliph) return;
    wb(PHYS_DS + d->ptr + y * d->w + x, c);
}
static void draw_char(const dest *d, const fontd *f, int x, int y, uint16_t c)
{
    int x0, x1, r, k;
    if (c < 0x80) {
        uint32_t sp = rd(PHYS_DS + f->sheet), sw = rd(PHYS_DS + f->sheet + 4);
        if (c == ' ' || !sheet_glyph(f, (uint8_t)c, &x0, &x1)) {
            if (c != ' ' && g_gbits[c]) goto jp;
            return;
        }
        for (r = 0; r < f->h; r++)
            for (k = x0; k <= x1; k++) {
                uint8_t v = rb(PHYS_DS + sp + (f->y0 + r) * sw + k);
                if (v) put_px(d, x + k - x0, y + r, v);
            }
        return;
    }
jp:
    if (!g_gbits[c]) return;
    if (!f->large) {
        /* 小さい字（5 ドット、行の間は 6 ドット）: 6 行に縮めた字形を、下の端をそろえて描く */
        y -= 1;
        for (r = 0; r < 6; r++) {
            uint8_t bits = g_gbits6[c][r];
            int sr = r * (f->h - 1) / 5;
            if (!bits) continue;
            for (k = 0; k < 8; k++) if (bits & (0x80 >> k)) put_px(d, x + k, y + r, f->col[sr]);
        }
        return;
    }
    for (r = 0; r < 8; r++) {
        uint8_t bits = g_gbits[c][r];
        int sr = r >= 7 ? f->h - 1 : r * (f->h - 1) / 6;
        if (!bits) continue;
        for (k = 0; k < 8; k++) if (bits & (0x80 >> k)) put_px(d, x + k, y + r, f->col[sr]);
    }
}
/* 折り返し: 行の区切りを lines[] に入れる（[始め, 終わり)）。right <= 0 なら改行だけ */
typedef struct { int st, en, w; } line_t;
static int unit_len(const fontd *f, const jstr *s, int i, int *w)
{
    /* 英数字の語はひとかたまり。それ以外は 1 字 */
    int j = i, ww = 0;
    if (s->c[i] < 0x80 && s->c[i] != ' ' && s->c[i] != '\n') {
        while (j < s->n && s->c[j] < 0x80 && s->c[j] != ' ' && s->c[j] != '\n') { ww += adv_of(f, s->c[j]); j++; }
        *w = ww; return j - i;
    }
    *w = adv_of(f, s->c[i]);
    return 1;
}
static int layout(const fontd *f, const jstr *s, int maxw, line_t *lines, int maxl)
{
    int nl = 0, i = 0, st = 0, w = 0;
    while (i < s->n && nl < maxl) {
        uint16_t c = s->c[i];
        int uw, ul;
        if (c == '\n') { lines[nl].st = st; lines[nl].en = i; lines[nl].w = w; nl++; i++; st = i; w = 0; continue; }
        if (c == ' ' && i == st) { i++; st = i; continue; }          /* 行頭の空白は捨てる */
        ul = unit_len(f, s, i, &uw);
        if (maxw > 0 && w + uw > maxw && i > st) {
            int br = i;
            /* 行頭禁則: この字が行頭に来られないなら、前の字ごと送る */
            if (c >= 0x80 && (g_gflags[c] & 1) && br - 1 > st) br--;
            /* 行末禁則: 前の字が行末に来られないなら、それも送る */
            if (br - 1 > st && s->c[br - 1] >= 0x80 && (g_gflags[s->c[br - 1]] & 2)) br--;
            {
                int k, ww = 0;
                for (k = st; k < br; k++) ww += adv_of(f, s->c[k]);
                lines[nl].st = st; lines[nl].en = br; lines[nl].w = ww; nl++;
            }
            i = br; st = br; w = 0;
            while (i < s->n && s->c[i] == ' ') { i++; st = i; }
            continue;
        }
        w += uw; i += ul;
    }
    if (st < s->n && nl < maxl) { lines[nl].st = st; lines[nl].en = s->n; lines[nl].w = w; nl++; }
    /* 行末の空白は幅に入れない */
    { int k; for (k = 0; k < nl; k++) while (lines[k].en > lines[k].st && s->c[lines[k].en - 1] == ' ') { lines[k].en--; lines[k].w -= adv_of(f, ' '); } }
    return nl;
}
static int draw_line(const dest *d, const fontd *f, int x, int y, const jstr *s, int st, int en)
{
    int i;
    for (i = st; i < en; i++) { draw_char(d, f, x, y, s->c[i]); x += adv_of(f, s->c[i]); }
    return x;
}
static int en_width(const fontd *f, const uint8_t *s, int n)
{
    int i, w = 0;
    for (i = 0; i < n; i++) w += adv_of(f, s[i]);
    return w;
}

/* ------------------------------------------------------------ コールバック */
#define SMAX 1200
static int read_str(uint32_t a, uint8_t *s)
{
    int n = 0;
    while (n < SMAX - 1 && (s[n] = rb(PHYS_DS + a + n)) != 0) n++;
    s[n] = 0;
    return n;
}
static const uint8_t FONT_Y[6] = {0xc1, 0xb8, 0xab, 0xb1, 0xa5, 0x9f};

/* 文字列を訳す。訳がなければ 0。コピープロテクトの問いには答えを足す */
static jstr g_js;
static const uint8_t CP_PAT[] = "ON PAGE ";
static int get_jp(const uint8_t *s, int n, int copyprot_ok)
{
    uint32_t at = arg(0) - g_code_delta, i;
    g_js.n = 0; g_js.flags = 0; g_condense = 0;
    if (n <= 0) return 0;
    if (g_nct) {
        uint64_t h = fnv64(s, n);
        for (i = 0; i < g_nct; i++)
            if (g_ct[i].at == at && g_ct[i].h == h && g_ct[i].enlen == n) { js_cat(&g_js, g_ct[i].ja, g_ct[i].jalen); goto got; }
    }
#ifdef FE_TEST
    if (g_logon >= 2) {                     /* テスト用: 描いた文字列と呼び出し元を（1 回ずつ）記録 */
        static uint64_t seen[4096]; static int ns;
        uint64_t k = fnv64(s, n) ^ ((uint64_t)at << 32);
        int j;
        for (j = 0; j < ns && seen[j] != k; j++) ;
        if (j == ns && ns < 4096) { char t[24]; seen[ns++] = k; wsprintfA(t, "draw@%x", at); log_str(t, s, n); }
    }
#endif
    if (!translate(s, n, &g_js, 0)) {
#ifdef FE_TEST
        if (g_logon) { char t[16]; wsprintfA(t, "miss@%x", at); log_str(t, s, n); }
#endif
        return 0;
    }
got:
    g_condense = 0;
    if (g_js.n > 0 && g_js.c[0] == 7) { memmove(g_js.c, g_js.c + 1, (g_js.n - 1) * 2); g_js.n--; g_condense = 1; }
    if (copyprot_ok && n > 8 && !memcmp(s, CP_PAT, 8)) {
        /* 呼び出し元（0x12c..）: 答えの語 = [ebp + edi*4 + 6] */
        uint8_t a[40];
        uint32_t ap = rd(PHYS_SS + R_EBP + 6 + R_EDI * 4);
        int k, an = 0;
        static const uint16_t pre[] = {0xff08, 0x7b54, 0x3048, 0xff1a}, post[] = {0xff09};   /* （答え： ） */
        while (an < 30 && (a[an] = rb(PHYS_DS + ap + an)) >= 0x20 && a[an] < 0x7f) an++;
        if (an > 0) {
            js_put(&g_js, ' ');
            for (k = 0; k < 4; k++) js_put(&g_js, pre[k]);
            js_ascii(&g_js, a, an);
            js_put(&g_js, post[0]);
        }
    }
    return 1;
}

/* 0: 折り返しありの描画（入口: push esi; push edi; enter 1ch,0 の代わり） */
static uint32_t __cdecl cb_wrap(void)
{
    uint8_t s[SMAX];
    int n, fi, nl, k, pitch, y;
    fontd f;
    dest d;
    line_t lines[40];
    set_delta(0x15cc5);
    n = read_str(R_EAX, s);
    fi = (int)arg(1);
    if (fi < 0 || fi > 5 || !get_jp(s, n, 1)) {
        push32(R_ESI); push32(R_EDI); push32(R_EBP); R_EBP = R_ESP; R_ESP -= 0x1c;
        return 0;
    }
    make_font(&f, DATA(0x780e8), FONT_Y[fi], fi < 2);
    dest_of(&d, R_ECX);
    {
        int x = (int)R_EDX, right = (int)arg(2);
        int maxw = fi >= 2 ? right - x : 320 - x;
        if (maxw < 16) maxw = 16;
        nl = layout(&f, &g_js, maxw, lines, 40);
        pitch = f.large ? 8 : 7;
        y = (int)R_EBX;
        for (k = 0; k < nl; k++) draw_line(&d, &f, x, y + k * pitch, &g_js, lines[k].st, lines[k].en);
        wd(PHYS_DS + DATA(0x8ec88), (uint32_t)(x + (nl ? lines[nl - 1].w : 0)));
        R_EAX = (uint32_t)((nl * pitch + 5) / 6);
        if (R_EAX == 0) R_EAX = 1;
    }
    R_EIP = pop32(); R_ESP += 8;
    return 0;
}

/* 1: 1 行の描画（入口: push esi; push edi; enter 14h,0 の代わり） */
#ifdef FE_TEST
static int g_novoice;           /* テスト用（FE_NOVOICE=1）: DM の声を使わない（文字で語る経路を確かめる） */
#endif
static uint32_t __cdecl cb_line(void)
{
    uint8_t s[SMAX];
    int n, fi, nl, k, x, y, pitch;
    fontd f;
    dest d;
    line_t lines[20];
    set_delta(0x15ffc);
#ifdef FE_TEST
    if (g_novoice) wb(PHYS_DS + DATA(0x8ec09), 0);
#endif
    n = read_str(R_EAX, s);
    fi = (int)arg(1);
    if (fi < 0 || fi > 5 || !get_jp(s, n, 0)) {
        push32(R_ESI); push32(R_EDI); push32(R_EBP); R_EBP = R_ESP; R_ESP -= 0x14;
        return 0;
    }
    make_font(&f, DATA(0x780e8), FONT_Y[fi], fi < 2);
    dest_of(&d, R_ECX);
    x = (int)R_EDX; y = (int)R_EBX;
    nl = layout(&f, &g_js, 0, lines, 20);
    pitch = f.large ? 8 : 7;
    for (k = 0; k < nl; k++) {
        int xx = x;
        if (g_js.flags & 1) {                     /* 先頭に空白のある英語: その幅の中で中央に */
            int ew = en_width(&f, s, n);
            xx = x + (ew - lines[k].w) / 2;
            if (xx < x - 8) xx = x - 8;
        }
        draw_line(&d, &f, xx, y + k * pitch, &g_js, lines[k].st, lines[k].en);
    }
#ifdef FE_TEST
    if (g_logon && nl) {                          /* テスト用: 英語より 4 ドット以上広い訳を記録 */
        int ew = en_width(&f, s, n), k2, mw = 0;
        while (n > 0 && s[n - 1] == ' ') n--;
        ew = en_width(&f, s, n);
        for (k2 = 0; k2 < nl; k2++) if (lines[k2].w > mw) mw = lines[k2].w;
        if (mw > ew + 3) { char t[40]; wsprintfA(t, "wide@%x +%d", arg(0) - g_code_delta, mw - ew); log_str(t, s, n); }
    }
#endif
    R_EIP = pop32(); R_ESP += 4;
    return 0;
}

/* ---- 下の帯（DM の語りと声の字幕）: 0x780f4 の画像の行 0..6 の字形、帯は y 0xaa..0xc7 */
static void band_restore(void)
{
    g_condense = 0;
    dest d, sd;
    int x, y;
    dest_of(&d, DATA(0x78104));
    dest_of(&sd, DATA(0x780f4));
    for (y = 0xaa; y <= 0xc7; y++)
        for (x = 0; x <= 0x13f; x++) put_px(&d, x, y, rb(PHYS_DS + sd.ptr + y * sd.w + x));
}
static void band_draw(const jstr *s, int st, int en)
{
    fontd f;
    dest d;
    line_t lines[8];
    jstr t;
    int nl, k, y0;
    t.n = 0;
    for (k = st; k < en; k++) js_put(&t, s->c[k]);
    make_font(&f, DATA(0x780f4), 0, 1);
    dest_of(&d, DATA(0x78104));
    nl = layout(&f, &t, 300, lines, 8);
    if (nl > 3) nl = 3;
    y0 = nl >= 3 ? 0xac : 0xb4 - (nl - 1) * 4;
    for (k = 0; k < nl; k++) draw_line(&d, &f, 10 + (300 - lines[k].w) / 2, y0 + k * 9, &t, lines[k].st, lines[k].en);
}
/* ページに分ける: 文（。！？ のあと）ごと。短い文は 2 行に収まるまでまとめ、長い文は 3 行ずつに切る */
static int is_stop(uint16_t c) { return c == 0x3002 || c == 0xff01 || c == 0xff1f || c == '.' || c == '!' || c == '?'; }
static int band_lines(const jstr *s, int st, int en, line_t *lines, int maxl)
{
    fontd f;
    jstr t;
    int k;
    memset(&f, 0, sizeof f); f.large = 1; f.h = 7;
    t.n = 0;
    for (k = st; k < en; k++) js_put(&t, s->c[k]);
    return layout(&f, &t, 300, lines, maxl);
}
static int page_split(const jstr *s, int *pst, int *pen, int maxp)
{
    int sst[60], sen[60], ns = 0, i = 0, st = 0, np = 0, k;
    line_t L[40];
    while (i < s->n && ns < 60) {
        uint16_t c = s->c[i++];
        if (is_stop(c)) {
            while (i < s->n && (is_stop(s->c[i]) || s->c[i] == 0x300d)) i++;
            sst[ns] = st; sen[ns] = i; ns++;
            while (i < s->n && (s->c[i] == ' ' || s->c[i] == 0x3000)) i++;
            st = i;
        }
    }
    if (st < s->n && ns < 60) { sst[ns] = st; sen[ns] = s->n; ns++; }
    for (k = 0; k < ns && np < maxp; ) {
        int a = sst[k], b = sen[k], nl = band_lines(s, a, b, L, 40);
        if (nl > 3) {                    /* 長い文: 3 行ずつ */
            int j;
            for (j = 0; j < nl && np < maxp; j += 3) {
                int e = j + 3 < nl ? j + 3 : nl;
                pst[np] = a + L[j].st; pen[np] = a + L[e - 1].en; np++;
            }
            k++; continue;
        }
        while (k + 1 < ns && band_lines(s, a, sen[k + 1], L, 40) <= 2) { k++; b = sen[k]; }
        pst[np] = a; pen[np] = b; np++;
        k++;
    }
    return np;
}
static jstr g_dm;
static int g_dmpst[40], g_dmpen[40], g_dmnp, g_dmpage;
/* 2: DM の語りの準備（eax = 文字列） */
static uint32_t __cdecl cb_dm_start(void)
{
    uint8_t s[SMAX];
    int n;
    set_delta(0x1627c);
    n = read_str(R_EAX, s);
    g_dm.n = 0;
    if (n > 0 && translate(s, n, &g_dm, 0)) ;
    else { log_str("miss dm", s, n); g_dm.n = 0; js_ascii(&g_dm, s, n); }
    g_dmnp = page_split(&g_dm, g_dmpst, g_dmpen, 40);
    g_dmpage = 0;
    return 0;
}
/* 3: 1 ページ描く（前のページを消してから）。eax = 待つ長さ（画面の書き換えの回数）/ 0 終わり */
static uint32_t __cdecl cb_dm_page(void)
{
    int len;
    band_restore();
    if (g_dmpage >= g_dmnp) { R_EAX = 0; return 0; }
    band_draw(&g_dm, g_dmpst[g_dmpage], g_dmpen[g_dmpage]);
    len = g_dmpen[g_dmpage] - g_dmpst[g_dmpage];
    g_dmpage++;
    R_EAX = (uint32_t)(60 + len * 7);         /* 英語は 1 文 0xd2 回。日本語は字数に合わせて伸ばす */
    if (R_EAX < 0xd2) R_EAX = 0xd2;
    return 0;
}
/* 4: 声の口の動きの入口（enter 78h,0 の代わり） */
static int g_in_voice, g_sub_on;
static uint32_t __cdecl cb_voice_enter(void)
{
    set_delta(0x28af2);
    push32(R_EBP); R_EBP = R_ESP; R_ESP -= 0x78;
    g_in_voice = 1; g_sub_on = 0;
    return 0;
}
/* 5: 口の動きの終わりの判定（cmp byte [esi],0ah; je 0x268b1 の代わり） */
static uint32_t __cdecl cb_voice_loop(void)
{
    if (rb(PHYS_DS + R_ESI) == 0x0a) {
        if (g_sub_on) band_restore();
        g_in_voice = 0; g_sub_on = 0;
        R_EIP = 0x268b1 + g_code_delta;
    }
    return 0;
}
/* 6: 効果音・声の再生の入口（push ecx; push esi; push edi; push ebp の代わり）。al = VOC の番号 */
static uint32_t __cdecl cb_sample(void)
{
    uint32_t k = R_EAX & 0xff;
    set_delta(0x42c71);
    push32(R_ECX); push32(R_ESI); push32(R_EDI); push32(R_EBP);
    if (g_in_voice && g_voice[k]) {
        jstr t;
        t.n = 0;
        js_cat(&t, g_voice[k], g_voicelen[k]);
        band_restore();
        band_draw(&t, 0, t.n);
        g_sub_on = 1;
    }
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    static const void *fn[] = {cb_wrap, cb_line, cb_dm_start, cb_dm_page, cb_voice_enter, cb_voice_loop, cb_sample};
    if (g_cb_installed) return 1;
    for (i = 0; i < 7; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy\n", b + i); return 0; }
    for (i = 0; i < 7; i++) tab[b + i] = (uint32_t)(uintptr_t)fn[i];
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

/* ------------------------------------------------------------ EXE のコピーを作る */
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
/* 'E' 節: size, count, {off u32, len u16, orig[len], new[len]}（元のバイトが合わなければ作らない） */
static uint8_t *build_exe(uint8_t *b, uint32_t n)
{
    const uint8_t *p = g_epatch + 8;
    uint32_t cnt = ((const uint32_t*)g_epatch)[1], i;
    if (((const uint32_t*)g_epatch)[0] != n) { dbg("exe size mismatch\n"); return NULL; }
    for (i = 0; i < cnt; i++) {
        uint32_t off = *(const uint32_t*)p;
        uint16_t len = *(const uint16_t*)(p + 4);
        const uint8_t *o = p + 6, *nw = o + len;
        uint8_t tmp[64];
        if (off + len > n || memcmp(b + off, o, len) || len > sizeof tmp) { dbg("exe bytes mismatch at %x\n", off); return NULL; }
        memcpy(tmp, nw, len);
        { uint32_t k; for (k = 0; k + 3 < len; k++) if (tmp[k] == 0xfe && tmp[k + 1] == 0x38 && tmp[k + 3] == 0 && tmp[k + 2] >= 0x70 && tmp[k + 2] < 0x80) tmp[k + 2] = (uint8_t)(tmp[k + 2] - 0x70 + g_cb_base); }
        memcpy(b + off, tmp, len);
        p = nw + len;
    }
    return b;
}

typedef FILE *(__cdecl *fopen_t)(const char *, const char *);
static fopen_t real_fopen;
static CRITICAL_SECTION g_cs;
static const char *base_name(const char *p)
{
    const char *b = p, *q;
    for (q = p; *q; q++) if (*q == '\\' || *q == '/' || *q == ':') b = q + 1;
    return b;
}
static const char *patched_path(const char *orig_path, char *buf)
{
    const char *bn = base_name(orig_path);
    uint32_t n;
    uint8_t *b;
    if (_stricmp(bn, "FANTASY.EXE")) return NULL;
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) return buf;
    {   /* 前の版のパッチで作ったコピーは消す */
        char pat[MAX_PATH], old[MAX_PATH], cur[16];
        WIN32_FIND_DATAA fd;
        HANDLE fh;
        wsprintfA(pat, "%s\\????????_%s", g_dir, bn);
        wsprintfA(cur, "%08X_", g_blob_crc);
        fh = FindFirstFileA(pat, &fd);
        if (fh != INVALID_HANDLE_VALUE) {
            do {
                if (_strnicmp(fd.cFileName, cur, 9)) { wsprintfA(old, "%s\\%s", g_dir, fd.cFileName); DeleteFileA(old); }
            } while (FindNextFileA(fh, &fd));
            FindClose(fh);
        }
    }
    b = read_all(orig_path, &n);
    if (!b) return NULL;
    if (!build_exe(b, n) || !write_all(buf, b, n)) { free(b); return NULL; }
    free(b);
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

#ifdef FE_TEST
/* テスト用（FE_PEEK=1）: ゲームの変数（マウスの位置・ボタン・押されたボタンの番号）を 0.5 秒ごとにログへ */
static int g_peeklog;
static DWORD WINAPI peek_thread(LPVOID p)
{
    (void)p;
    int k = 0;
    for (;;) {
        Sleep(50);
        if (!g_delta_ok) continue;
        testio_mouse((int16_t)(rb(PHYS_DS + DATA(0x8ec2e)) | (rb(PHYS_DS + DATA(0x8ec2f)) << 8)),
                     (int16_t)(rb(PHYS_DS + DATA(0x8ec30)) | (rb(PHYS_DS + DATA(0x8ec31)) << 8)));
        if (++k % 10 || !g_peeklog) continue;
        dbg("peek x=%d y=%d btn=%d id=%d\n", (int16_t)(rb(PHYS_DS + DATA(0x8ec2e)) | (rb(PHYS_DS + DATA(0x8ec2f)) << 8)),
            (int16_t)(rb(PHYS_DS + DATA(0x8ec30)) | (rb(PHYS_DS + DATA(0x8ec31)) << 8)),
            (int16_t)(rb(PHYS_DS + DATA(0x8ec32)) | (rb(PHYS_DS + DATA(0x8ec33)) << 8)),
            (int16_t)(rb(PHYS_DS + DATA(0x63e85)) | (rb(PHYS_DS + DATA(0x63e86)) << 8)));
    }
}
#endif

static void init(HMODULE self)
{
    char la[MAX_PATH];
#ifdef FE_TEST
    char v[8];
#endif
    HMODULE crt;
    InitializeCriticalSection(&g_cs);
    if (!GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) return;
    wsprintfA(g_dir, "%s\\FE_JP", la);
    CreateDirectoryA(g_dir, NULL);
#ifdef FE_TEST
    g_logon = GetEnvironmentVariableA("FE_LOG", v, sizeof v) > 0 ? (v[0] == '2' ? 2 : 1) : 0;
    if (GetEnvironmentVariableA("FE_NOJP", v, sizeof v) > 0) return;      /* テスト用: 英語のまま */
#endif
    if (!verify_dosbox()) { dbg("unknown DOSBox build\n"); return; }
    if (!load_blob(self)) { dbg("no patch data\n"); return; }
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) { dbg("fopen import not found\n"); return; }
    g_enabled = 1;
#ifdef FE_TEST
    g_peeklog = GetEnvironmentVariableA("FE_PEEK", v, sizeof v) > 0;
    g_novoice = GetEnvironmentVariableA("FE_NOVOICE", v, sizeof v) > 0;
    if (GetEnvironmentVariableA("FE_TEST", v, sizeof v) > 0) CreateThread(NULL, 0, peek_thread, NULL, 0, NULL);
#endif
    dbg("Fantasy Empires JP patch active (dosbox %x), blob %08X, %u entries, %u patterns\n", g_dbx, g_blob_crc, g_ntr, g_npt);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        init(h);
#ifdef FE_TEST
        testio_init();
#endif
    }
    return TRUE;
}
