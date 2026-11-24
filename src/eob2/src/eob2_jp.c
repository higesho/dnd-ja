/*
 * Eye of the Beholder II 日本語化パッチ（実行時部分）
 * DOSBox 0.74-3（Forgotten Realms The Archives 同梱版）に winmm.dll の代理として読み込まれる。
 *  - DOSBox がゲームのファイル（START.EXE / MGA.OVL / フォント / 台本など）を開くとき、
 *    %LOCALAPPDATA%\EOB2_JP に作った日本語版のコピーへ差し替える
 *  - 画面ドライバ MGA.OVL の文字列描画と、ゲームの折り返し位置の計算を DOSBox の
 *    コールバック命令に置き換え、この DLL が日本語（美咲ゴシック第2）を描く
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "resource.h"

void proxy_init(void);

/* ---------------------------------------------------------------- ログ */
static char g_dir[MAX_PATH];
static FILE *g_log;
static int g_logon;
/* ログは試験版（DEVTEST）だけで書く。配布版では何もしない */
static void dbg(const char *fmt, ...)
{
#ifdef DEVTEST
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
#else
    (void)fmt;
#endif
}

/* ------------------------------------------- DOSBox 0.74-3 の内部アドレス */
static uint32_t g_dbx, g_dbx_code;   /* DOSBox build: Steam 0.74-3 = 0, GOG 0.74-2.1 = +0xe000 / -0x10 */
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
static __inline void ww(uint32_t a, uint16_t v) { wb(a, (uint8_t)v); wb(a + 1, (uint8_t)(v >> 8)); }
static __inline int16_t rws(uint32_t a) { return (int16_t)rw(a); }
#define REG32(i) (((uint32_t*)A_REGS)[i])
#define SET_AX(v) (*(uint16_t*)&REG32(0) = (uint16_t)(v))
#define SET_SI(v) (*(uint16_t*)&REG32(6) = (uint16_t)(v))
#define R_SP ((uint16_t)REG32(4))
#define R_BP ((uint16_t)REG32(5))
#define PHYS_CS (((uint32_t*)A_SEGPHYS)[1])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
static __inline uint32_t farptr(uint32_t a) { return ((uint32_t)rw(a + 2) << 4) + rw(a); }

/* ------------------------------------------------------- パッチデータ */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)

static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;

/* 日本語の文字: 1バイト目 0x81-0xfe, 2バイト目 0x80-0xfe（独自の符号。ゲームの英字処理と衝突しない） */
#define IS_LEAD(c) ((c) >= 0x81 && (c) <= 0xfe)
#define IS_TRAIL(c) ((c) >= 0x80 && (c) <= 0xfe)
static uint16_t *g_glyph_index;  /* 65536 -> グリフ番号+1 */
static const uint8_t *g_glyphs;  /* 8バイト/字（行ごと, bit7=左） */
static const uint8_t *g_gflags;  /* 1=行頭禁則 2=行末禁則 */
static const uint8_t *g_half;    /* 半角 128字 x 8バイト（4ドット幅, bit7=左） */
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

static void load_table(const uint8_t *d);

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
    if (memcmp(h->magic, "EJP2", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len);
    p = g_blob + sizeof(blob_hdr);
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == 'F') {            /* u32 n; n*u16 code; n*8 bitmap; n flags */
            uint32_t n = *(const uint32_t*)d, k;
            const uint16_t *codes = (const uint16_t*)(d + 4);
            g_glyphs = d + 4 + n * 2;
            g_gflags = g_glyphs + n * 8;
            g_glyph_index = (uint16_t*)calloc(65536, 2);
            for (k = 0; k < n; k++) g_glyph_index[codes[k]] = (uint16_t)(k + 1);
        } else if (s->type == 'H') {
            g_half = d;
        } else if (s->type == 'T') {
            load_table(d);
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0];
            g_logon = (int)v[1];
        }
        p = d + s->len;
    }
    return g_glyph_index && g_half;
}

/* ------------------------------------------------------------ 翻訳表 */
/* 'T' 節: u32 個数, 項目...
 *   u8 種類(0=完全一致, 1=書式), u8 旗(1=中央寄せし直す), u16 訳長, u16 英語長, u64 英語のハッシュ
 *   種類 1 はさらに u16 部品数, 部品 {u8 型(0=固定,1=指定); 固定: u16 長さ, u64 ハッシュ / 指定: u8 変換, u8 幅, u8 左寄せ}
 *   続いて訳。訳の中の "%1".."%9" は引数（英語側の書式指定の順番）, "%%" は % そのもの。
 * 英語の文そのものは持たない（原作の文章を配布物に入れないため、ハッシュで照合する）。 */
typedef struct { uint8_t type, conv, width, left; uint16_t len; uint64_t h; } ptok;
typedef struct { uint64_t h; const uint8_t *ja; uint16_t enlen, jalen; uint8_t flags; ptok *tok; int ntok; } tentry;
static tentry *g_tr;
static uint32_t g_ntr;
static int32_t *g_hash;          /* 開番地法: 項目番号+1 */
static uint32_t g_hmask;
static int *g_pat;               /* 書式（%）を含む項目 */
static int g_npat;

static uint32_t fnv(const uint8_t *p, int n)
{
    uint32_t h = 2166136261u;
    while (n--) h = (h ^ *p++) * 16777619u;
    return h;
}
static uint64_t fnv64(const uint8_t *p, int n)
{
    uint64_t h = 14695981039346656037ull;
    while (n--) h = (h ^ *p++) * 1099511628211ull;
    return h;
}

static void load_table(const uint8_t *d)
{
    uint32_t n = *(const uint32_t*)d, i, sz = 1;
    const uint8_t *p = d + 4;
    g_tr = (tentry*)calloc(n ? n : 1, sizeof(tentry));
    while (sz < n * 2) sz <<= 1;
    g_hash = (int32_t*)calloc(sz, 4);
    g_hmask = sz - 1;
    g_pat = (int*)calloc(n ? n : 1, sizeof(int));
    for (i = 0; i < n; i++) {
        tentry *e = &g_tr[i];
        uint8_t kind = p[0];
        uint32_t h;
        e->flags = p[1];
        e->jalen = *(const uint16_t*)(p + 2);
        e->enlen = *(const uint16_t*)(p + 4);
        memcpy(&e->h, p + 6, 8);
        p += 14;
        if (kind == 1) {
            int k;
            e->ntok = *(const uint16_t*)p;
            p += 2;
            e->tok = (ptok*)calloc(e->ntok ? e->ntok : 1, sizeof(ptok));
            for (k = 0; k < e->ntok; k++) {
                ptok *t = &e->tok[k];
                t->type = *p++;
                if (t->type == 0) { t->len = *(const uint16_t*)p; memcpy(&t->h, p + 2, 8); p += 10; }
                else { t->conv = p[0]; t->width = p[1]; t->left = p[2]; p += 3; }
            }
            g_pat[g_npat++] = (int)i;
        }
        e->ja = p;
        p += e->jalen;
        h = (uint32_t)e->h & g_hmask;
        while (g_hash[h]) h = (h + 1) & g_hmask;
        g_hash[h] = (int32_t)i + 1;
    }
    g_ntr = n;
    {   /* 具体的な（長い）書式から先に試す */
        int a, b2;
        for (a = 1; a < g_npat; a++) {
            int v = g_pat[a], lv = g_tr[v].enlen;
            for (b2 = a; b2 > 0; b2--) {
                int u = g_pat[b2 - 1];
                if ((int)g_tr[u].enlen >= lv) break;
                g_pat[b2] = u;
            }
            g_pat[b2] = v;
        }
    }
}

static const tentry *tr_exact(const uint8_t *s, int n)
{
    uint64_t hh;
    uint32_t h;
    if (!g_hash) return NULL;
    hh = fnv64(s, n);
    h = (uint32_t)hh & g_hmask;
    while (g_hash[h]) {
        const tentry *e = &g_tr[g_hash[h] - 1];
        if (e->h == hh && e->enlen == n) return e;
        h = (h + 1) & g_hmask;
    }
    return NULL;
}

/* 書式の k 番目の指定の幅（%-18s なら 18, 左寄せ=1） */
static int spec_width(const tentry *e, int k, int *left)
{
    int i, idx = 0;
    for (i = 0; i < e->ntok; i++) {
        if (e->tok[i].type != 1) continue;
        if (idx++ == k) { *left = e->tok[i].left; return e->tok[i].width; }
    }
    *left = 0;
    return 0;
}

/* 可変長の文字列 */
typedef struct { uint8_t *p; int n, cap; } sbuf;
static void sb_put(sbuf *b, const uint8_t *s, int n)
{
    if (b->n + n + 1 > b->cap) {
        int c = b->cap ? b->cap : 256;
        while (c < b->n + n + 1) c *= 2;
        b->p = (uint8_t*)realloc(b->p, c);
        b->cap = c;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}
static void sb_c(sbuf *b, uint8_t c) { sb_put(b, &c, 1); }

#define MAXARG 9
typedef struct { const uint8_t *p; int n; } span;

/* 訳の雛形に引数をはめる */
static void render(sbuf *out, const uint8_t *t, int tn, const span *a, int na)
{
    int i;
    for (i = 0; i < tn; i++) {
        if (t[i] == '%' && i + 1 < tn) {
            if (t[i + 1] == '%') { sb_c(out, '%'); i++; continue; }
            if (t[i + 1] >= '1' && t[i + 1] <= '9') {
                int k = t[i + 1] - '1';
                if (k < na) sb_put(out, a[k].p, a[k].n);
                i++;
                continue;
            }
        }
        sb_c(out, t[i]);
    }
}

/* 書式の部品と、組み立て済みの英文を突き合わせて引数を取り出す（%s は最短一致から） */
static int pmatch(const ptok *t, int nt, const uint8_t *s, int sn, span *a, int na)
{
    int j;
    if (nt == 0) return sn == 0 ? na + 1 : 0;     /* 引数の個数 + 1 */
    if (t->type == 0) {
        if (sn < t->len || fnv64(s, t->len) != t->h) return 0;
        return pmatch(t + 1, nt - 1, s + t->len, sn - t->len, a, na);
    }
    if (na >= MAXARG) return 0;
    if (t->conv == 's' || t->conv == 'c') {
        int lo = t->conv == 'c' ? 1 : t->width, hi = t->conv == 'c' ? 1 : sn;
        for (j = lo; j <= hi && j <= sn; j++) {
            int r;
            a[na].p = s; a[na].n = j;
            r = pmatch(t + 1, nt - 1, s + j, sn - j, a, na + 1);
            if (r) return r;
        }
        return 0;
    }
    if (strchr("duxi", t->conv)) {
        j = 0;
        while (j < sn && s[j] == ' ') j++;
        if (j < sn && (s[j] == '-' || s[j] == '+')) j++;
        if (j >= sn || s[j] < '0' || s[j] > '9') return 0;
        while (j < sn && ((s[j] >= '0' && s[j] <= '9') || (t->conv == 'x' && strchr("abcdefABCDEF", s[j])))) j++;
        while (t->left && j < sn && j < t->width && s[j] == ' ') j++;   /* %-3d の右側の空白（左寄せのときだけ） */
        a[na].p = s; a[na].n = j;
        return pmatch(t + 1, nt - 1, s + j, sn - j, a, na + 1);
    }
    return 0;
}

/* 幅指定どおりに空白で詰め直す（バイト数で数える。日本語 1 字 = 2 バイト = 半角 2 字分） */
static void repad(sbuf *b, int w, int left)
{
    int a = 0, z = b->n, pad;
    uint8_t *t;
    if (w <= 0) return;
    while (a < z && b->p[a] == ' ') a++;
    while (z > a && b->p[z - 1] == ' ') z--;
    pad = w - (z - a);
    if (pad < 0) pad = 0;
    t = (uint8_t*)malloc(w + (z - a) + 1);
    if (left) { memcpy(t, b->p + a, z - a); memset(t + (z - a), ' ', pad); }
    else { memset(t, ' ', pad); memcpy(t + pad, b->p + a, z - a); }
    free(b->p);
    b->p = t; b->n = (z - a) + pad; b->cap = b->n + 1;
    b->p[b->n] = 0;
}

static int is_space_ctl(uint8_t c) { return c == ' ' || c == 0x0d || c == 0x0a; }

/* 英文 s を訳す。訳が見つかれば out に入れて 1 */
static int tr_str(sbuf *out, const uint8_t *s, int n, int depth)
{
    const tentry *e;
    int a0 = 0, b0 = n, i;
    if (n <= 0 || depth > 3) return 0;
    for (i = 0; i < n; i++) if (s[i] >= 0x80) return 0;   /* 既に日本語 */
    e = tr_exact(s, n);
    if (e) { render(out, e->ja, e->jalen, NULL, 0); return 1; }
    /* 前後の空白・改行・色指定を外して再挑戦 */
    for (;;) {
        if (a0 < b0 && is_space_ctl(s[a0])) { a0++; continue; }
        if (a0 + 1 < b0 && (s[a0] == 0x02 || s[a0] == 0x06)) { a0 += 2; continue; }
        break;
    }
    while (b0 > a0 && is_space_ctl(s[b0 - 1])) b0--;
    if (a0 > 0 || b0 < n) {
        sbuf t = {0};
        if (b0 > a0 && tr_str(&t, s + a0, b0 - a0, depth + 1)) {
            sb_put(out, s, a0);
            sb_put(out, t.p, t.n);
            sb_put(out, s + b0, n - b0);
            free(t.p);
            return 1;
        }
        free(t.p);
        return 0;
    }
    /* 書式つきの項目と突き合わせる */
    for (i = 0; i < g_npat; i++) {
        span a[MAXARG];
        int k, na;
        e = &g_tr[g_pat[i]];
        na = pmatch(e->tok, e->ntok, s, n, a, 0);
        if (na > 0) {
            sbuf args[MAXARG];
            span ta[MAXARG];
            na--;
            memset(args, 0, sizeof args);
            for (k = 0; k < na; k++) {
                int left, w = spec_width(e, k, &left);
                if (tr_str(&args[k], a[k].p, a[k].n, depth + 1)) {
                    if (w) repad(&args[k], w, left);
                    ta[k].p = args[k].p; ta[k].n = args[k].n;
                } else ta[k] = a[k];
            }
            {
                int start = out->n;
                render(out, e->ja, e->jalen, ta, na);
                for (k = 0; k < na; k++) free(args[k].p);
                if (out->n - start == n && !memcmp(out->p + start, s, n)) {   /* 何も訳せていない */
                    out->n = start;
                    if (out->p) out->p[start] = 0;
                    continue;
                }
            }
            return 1;
        }
    }
    return 0;
}

/* 'X' / 'S' 節: 先頭16バイトがファイル名 */
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

/* ---------------------------------------------------------- 文字の描画 */
static int glyph_of(uint16_t code) { return g_glyph_index ? g_glyph_index[code] : 0; }
static int gflags(uint16_t code) { int g = glyph_of(code); return g ? g_gflags[g - 1] : 0; }

typedef struct {
    uint32_t dest;      /* 描画先ページの先頭（線形アドレス） */
    uint32_t font;      /* ゲームのフォント（線形） */
    int fw, fh;
    uint8_t fg, bg;
} drawctx;

static __inline void px(const drawctx *c, int x, int y, int on)
{
    if (x < 0 || x >= 320 || y < 0 || y >= 200) return;
    if (on) { if (c->fg) wb(c->dest + y * 320 + x, c->fg); }
    else if (c->bg) wb(c->dest + y * 320 + x, c->bg);
}

/* 全角: 幅 2*fw のマスに 8x8 の字を置く */
static void draw_wide(const drawctx *c, uint16_t code, int x, int y)
{
    static const uint8_t tofu[8] = {0xfe,0x82,0x82,0x82,0x82,0x82,0xfe,0};
    int g = glyph_of(code), r, k, w = c->fw * 2, ox = (w - 8) / 2;
    const uint8_t *bits = g ? g_glyphs + (g - 1) * 8 : tofu;
    if (ox < 0) ox = 0;
    for (r = 0; r < c->fh; r++) {
        uint8_t row = r < 8 ? bits[r] : 0;
        for (k = 0; k < w; k++) {
            int gx = k - ox;
            px(c, x + k, y + r, gx >= 0 && gx < 8 && ((row >> (7 - gx)) & 1));
        }
    }
}

/* 半角: 美咲の 4x8（フォント幅が 4 のとき）、それ以外はゲームの元フォント */
static void draw_half(const drawctx *c, uint8_t ch, int x, int y)
{
    int r, k;
    if (c->fw == 4 && g_half) {
        const uint8_t *bits;
        if (ch >= 'a' && ch <= 'z') ch -= 0x20;   /* ゲームのフォントは小文字も大文字の形 */
        bits = g_half + (ch & 0x7f) * 8;
        for (r = 0; r < c->fh; r++) {
            uint8_t row = r < 8 ? bits[r] : 0;
            for (k = 0; k < 4; k++) px(c, x + k, y + r, (row >> (7 - k)) & 1);
        }
        return;
    }
    {
        uint32_t g = c->font + rw(c->font + 2 + (ch & 0x7f) * 2);
        int bw = (c->fw + 7) / 8;
        for (r = 0; r < c->fh; r++)
            for (k = 0; k < c->fw; k++) {
                uint8_t b = rb(g + r * bw + k / 8);
                px(c, x + k, y + r, (b >> (7 - (k & 7))) & 1);
            }
    }
}

#define SEGVAL(i) (((uint16_t*)0x74d100u)[i])

/* Window_Print の中から呼ばれた Print か（その文字列は DLL が既に訳したもの） */
static uint16_t g_wp_cs = 0xffff;
#define WP_FLUSH_RET 0x078a        /* Window_Print の行出力 lcall [65f2] の戻り先 */

/* 翻訳結果の小さなキャッシュ（Print は同じ文字列で何度も呼ばれる） */
#define NCACHE 128
typedef struct { uint32_t h; int n; uint8_t *en; sbuf out; int hit, flags; } centry;
static centry g_cache[NCACHE];
static int g_cache_next;

static const sbuf *tr_cached(const uint8_t *s, int n, int *flags)
{
    uint32_t h = fnv(s, n);
    int i;
    centry *c;
    for (i = 0; i < NCACHE; i++) {
        c = &g_cache[i];
        if (c->en && c->h == h && c->n == n && !memcmp(c->en, s, n)) { *flags = c->flags; return c->hit ? &c->out : NULL; }
    }
    c = &g_cache[g_cache_next];
    g_cache_next = (g_cache_next + 1) % NCACHE;
    free(c->en); free(c->out.p);
    memset(c, 0, sizeof *c);
    c->h = h; c->n = n;
    c->en = (uint8_t*)malloc(n + 1);
    memcpy(c->en, s, n);
    c->hit = tr_str(&c->out, s, n, 0);
    {
        const tentry *e = tr_exact(s, n);
        c->flags = e ? e->flags : 0;
    }
    *flags = c->flags;
    dbg("print %s: %.*s\n", c->hit ? "tr" : "--", n, s);
    return c->hit ? &c->out : NULL;
}

/* コピープロテクト: 「この記号のページの○行目の○語目」の答えを出す。
 * 出題の番号は呼び出し元（START.EXE 24d06 の関数）が [bp-6] に退避した SI。
 * 表（4 バイト x 50: 記号, 行, 語, 0）と答えの語（表+0xd0 から NUL 区切り）をゲームのメモリから探す。 */
static uint32_t g_cp_tab;
static int g_cp_searched;
static uint32_t find_cp_table(void)
{
    static const uint8_t sig[12] = {9,3,4,0, 9,13,2,0, 9,7,7,0};
    uint32_t a;
    if (g_cp_searched) return g_cp_tab;
    g_cp_searched = 1;
    for (a = 0x1000; a < 0xa0000 - 0x200; a++) {
        int k;
        if (rb(a) != 9) continue;
        for (k = 1; k < 12 && rb(a + k) == sig[k]; k++) ;
        if (k == 12) { g_cp_tab = a; break; }
    }
    dbg("copy protection table %x\n", g_cp_tab);
    return g_cp_tab;
}

static int cp_answer(const uint8_t *en, int n, sbuf *out)
{
    const uint8_t *p = NULL;
    int line = -1, word = -1, i, idx = -1, si;
    uint32_t tab, w;
    const tentry *e;
    for (i = 0; i + 10 <= n; i++) if (!memcmp(en + i, "Find line ", 10)) { line = atoi((const char*)en + i + 10); break; }
    for (i = 0; i + 11 <= n; i++) if (!memcmp(en + i, "Enter word ", 11)) { word = atoi((const char*)en + i + 11); p = en + i; break; }
    (void)p;
    if (line < 0 || word < 0) return 0;
    tab = find_cp_table();
    if (!tab) return 0;
    si = rws(PHYS_SS + R_BP - 6);
    if (si >= 0 && si < 50 && rb(tab + si * 4 + 1) == line && rb(tab + si * 4 + 2) == word) idx = si;
    else {
        int k, cnt = 0;
        for (k = 0; k < 50; k++) if (rb(tab + k * 4 + 1) == line && rb(tab + k * 4 + 2) == word) { idx = k; cnt++; }
        if (cnt != 1) idx = -1;
    }
    dbg("copy protection: si=%d line=%d word=%d -> %d\n", si, line, word, idx);
    if (idx < 0) return 0;
    w = tab + 0xd0;
    for (i = 0; i < idx; i++) { int k = 0; while (rb(w) && k++ < 40) w++; w++; }
    e = tr_exact((const uint8_t*)"@ANSWER", 7);
    {
        sbuf ans = {0};
        span a;
        for (i = 0; i < 40 && rb(w + i); i++) sb_c(&ans, rb(w + i));
        a.p = ans.p; a.n = ans.n;
        sb_c(out, 0x0d);
        if (e) render(out, e->ja, e->jalen, &a, 1);
        else sb_put(out, ans.p, ans.n);
        free(ans.p);
    }
    return 1;
}

/* MGA.OVL の Print(char far *s, int x, int y, int fg, int bg) の置き換え（先頭で retf する） */
static uint32_t __cdecl cb_print(void)
{
    uint32_t sp = PHYS_SS + R_SP, cs = PHYS_CS, sa, fp;
    int x, y, x0, n = 0, i, center = 0;
    static uint8_t buf[2048];
    const uint8_t *s;
    drawctx c;
    sa = farptr(sp + 4);
    x = rws(sp + 8); y = rws(sp + 10);
    c.fg = rb(sp + 12); c.bg = rb(sp + 14);
    while (n < (int)sizeof buf - 1 && (buf[n] = rb(sa + n)) != 0) n++;
    buf[n] = 0;
    s = buf;
    if (!(rw(sp) == WP_FLUSH_RET && rw(sp + 2) == g_wp_cs)) {
        int fl = 0, n0 = n;
        const sbuf *t = tr_cached(buf, n, &fl);
        if (t) { s = t->p ? t->p : (const uint8_t*)""; n = t->n; }
        if (t && (fl & 1)) center = 1;
        if (n0 > 20 && memchr(buf, 'E', n0) && strstr((const char*)buf, "Enter word ")) {
            static sbuf q;
            q.n = 0;
            sb_put(&q, s, n);
            while (q.n && q.p[q.n - 1] == 0x0d) q.n--;
            if (cp_answer(buf, n0, &q)) { s = q.p; n = q.n; }
        }
    }
    fp = farptr(cs + 3);                      /* フォントへのポインタ変数 */
    c.font = farptr(fp);
    c.fh = rb(c.font + 0x102);
    c.fw = rb(c.font + 0x103);
    {
        uint32_t pg = farptr(cs + 0x0b), tab = farptr(cs + 7);
        c.dest = (uint32_t)rw(tab + (rb(pg) & 0x1e) * 2) << 4;
    }
    if (c.fw <= 0 || c.fh <= 0) return 0;
    if (center) x = (320 - n * c.fw) / 2;   /* オープニング等: 訳文の長さで画面の中央に置き直す */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= 320 || y >= 200) return 0;
    x0 = x;
    for (i = 0; i < n;) {
        uint8_t ch = s[i];
        if (ch == 0x0d) { x = x0; y += c.fh; i++; continue; }
        if (IS_LEAD(ch) && i + 1 < n && IS_TRAIL(s[i + 1])) {
            uint16_t code = (uint16_t)((ch << 8) | s[i + 1]);
            if (x + 2 * c.fw > 320) { x = x0; y += c.fh; }
            if (y >= 200) break;
            draw_wide(&c, code, x, y);
            x += 2 * c.fw;
            i += 2;
            continue;
        }
        if (x + c.fw > 320) { x = x0; y += c.fh; }
        if (y >= 200) break;
        draw_half(&c, ch, x, y);
        x += c.fw;
        i++;
    }
    return 0;
}

/* ------------------------------------------------ Window_Print の文章 */
/* 入口（START.EXE 24f8b, lea ax,[bp+0a] / mov [bp-12],ss の代わり）で
 * 書式と引数から英文を組み立てて訳し、その後の文字取得（2352:08b4）を DLL から供給する */
static sbuf g_msg;
static int g_msgpos;

/* 書式 f と引数 va から文を組み立てる（ゲームの %d / %s と同じ扱い） */
static void compose(sbuf *out, uint32_t f, uint32_t va, int translate_args)
{
    int i;
    for (i = 0; i < 4096; i++) {
        uint8_t ch = rb(f + i);
        if (!ch) break;
        if (ch == '%') {
            uint8_t t = rb(f + i + 1) | 0x20;
            if (t == 'd') {
                char num[16];
                int16_t v = rws(va); va += 2;
                wsprintfA(num, "%d", v);
                sb_put(out, (const uint8_t*)num, lstrlenA(num));
                i++;
                continue;
            }
            if (t == 's') {
                uint32_t a = farptr(va), k;
                int nonnull = rw(va) || rw(va + 2);
                sbuf tmp = {0};
                va += 4;
                i++;
                if (nonnull) {
                    for (k = 0; k < 4096 && rb(a + k); k++) sb_c(&tmp, rb(a + k));
                    if (tmp.n) {
                        sbuf t2 = {0};
                        if (translate_args && tr_str(&t2, tmp.p, tmp.n, 1)) sb_put(out, t2.p, t2.n);
                        else sb_put(out, tmp.p, tmp.n);
                        free(t2.p);
                    }
                }
                free(tmp.p);
                continue;
            }
        }
        sb_c(out, ch);
    }
}

/* 書式そのものに訳があれば、それに訳した引数をはめる */
static int compose_fmt(sbuf *out, uint32_t f, uint32_t va)
{
    sbuf fm = {0};
    const tentry *e;
    span a[MAXARG];
    sbuf args[MAXARG];
    int na = 0, i, k;
    for (i = 0; i < 4096 && rb(f + i); i++) sb_c(&fm, rb(f + i));
    e = fm.n ? tr_exact(fm.p, fm.n) : NULL;
    if (!e || !memchr(fm.p, '%', fm.n)) { free(fm.p); return 0; }
    memset(args, 0, sizeof args);
    for (i = 0; i + 1 < fm.n && na < MAXARG; i++) {
        if (fm.p[i] != '%') continue;
        if ((fm.p[i + 1] | 0x20) == 'd') {
            char num[16];
            wsprintfA(num, "%d", rws(va)); va += 2;
            sb_put(&args[na], (const uint8_t*)num, lstrlenA(num));
            na++; i++;
        } else if ((fm.p[i + 1] | 0x20) == 's') {
            uint32_t p = farptr(va);
            sbuf raw = {0};
            int nonnull = rw(va) || rw(va + 2);
            va += 4;
            if (nonnull) for (k = 0; k < 4096 && rb(p + k); k++) sb_c(&raw, rb(p + k));
            if (!raw.n || !tr_str(&args[na], raw.p, raw.n, 1)) sb_put(&args[na], raw.p ? raw.p : (const uint8_t*)"", raw.n);
            free(raw.p);
            na++; i++;
        }
    }
    for (k = 0; k < na; k++) { a[k].p = args[k].p; a[k].n = args[k].n; }
    render(out, e->ja, e->jalen, a, na);
    for (k = 0; k < na; k++) free(args[k].p);
    free(fm.p);
    return 1;
}

static uint32_t __cdecl cb_wprint(void)
{
    uint32_t ssl = PHYS_SS, bp = R_BP, f, va;
    sbuf en = {0};
    ww(ssl + bp - 0x12, SEGVAL(2));
    SET_AX(bp + 0x0a);
    g_wp_cs = SEGVAL(1);
    f = farptr(ssl + bp + 6);
    va = ssl + bp + 0x0a;
#ifdef DEVTEST
    {   /* 試験版だけ: 環境変数 EOB2JP_FMTTEST の書式（\xNN 可, %s = "Tenmiyana"）で文を置き換える */
        static char tv[512];
        static int tl = -1;
        if (tl < 0) tl = (int)GetEnvironmentVariableA("EOB2JP_FMTTEST", tv, sizeof tv - 1);
        if (tl > 0) {
            const uint32_t F = 0x10fc00, N = 0x10fe00, V = 0x10ff00;
            const char *nm = "Tenmiyana";
            int j, k = 0;
            for (j = 0; j < tl; j++) {
                if (tv[j] == '\\' && tv[j + 1] == 'x' && j + 3 < tl) {
                    char h[3] = {tv[j + 2], tv[j + 3], 0};
                    wb(F + k++, (uint8_t)strtol(h, NULL, 16)); j += 3;
                } else wb(F + k++, (uint8_t)tv[j]);
            }
            wb(F + k, 0);
            for (j = 0; nm[j]; j++) wb(N + j, (uint8_t)nm[j]);
            wb(N + j, 0);
            ww(V, (uint16_t)(N - 0xffff0)); ww(V + 2, 0xffff);
            f = F; va = V;
        }
    }
#endif
    g_msg.n = 0;
    g_msgpos = 0;
    compose(&en, f, va, 0);
    if (!compose_fmt(&g_msg, f, va)) {
        if (!(en.n && tr_str(&g_msg, en.p, en.n, 0))) {
            g_msg.n = 0;
            compose(&g_msg, f, va, 1);
        }
    }
    sb_put(&g_msg, (const uint8_t*)"", 0);
    dbg("wprint: %.*s\n", en.n, en.p ? (char*)en.p : "");
    free(en.p);
    return 0;
}

/* 文字取得（Window_Print 専用）: AL に次の 1 バイト */
static uint32_t __cdecl cb_fetch(void)
{
    uint8_t c = 0;
    if (g_msg.p && g_msgpos < g_msg.n) c = g_msg.p[g_msgpos++];
    *(uint8_t*)&REG32(0) = c;
    return 0;
}

/* ---------------------------------------------------------- 折り返し */
/* Window_Print の行あふれ処理（START.EXE 2542a）の改行位置を決める。
 * 語バッファ ds:f1ec（長さ [f254]）、カーソル列 [f094]、使える列数 [bp-2]。結果は SI。 */
static int ascii_no_start(uint8_t c) { return c && strchr(",.!?)]:;'%>\"", c) != NULL; }
static int ascii_no_end(uint8_t c) { return c && strchr("([<\"", c) != NULL; }

static uint32_t __cdecl cb_wrap(void)
{
    uint32_t ds = PHYS_DS, buf = ds + 0xf1ec;
    int len = rws(ds + 0xf254), cur = rws(ds + 0xf094), avail = rws(PHYS_SS + R_BP - 2);
    int si = len;
    if (cur + len > avail) {
        int room = avail - cur, p = 0, best = -1, bound = 0, first = 1;
        int prev_jp = 0, prev_noend = 0;
        while (p < len && p <= room) {
            uint8_t ch = rb(buf + p);
            int n = 1, jp = 0, nostart = 0, noend = 0;
            if (IS_LEAD(ch) && p + 1 < len && IS_TRAIL(rb(buf + p + 1))) {
                uint16_t code = (uint16_t)((ch << 8) | rb(buf + p + 1));
                int f = gflags(code);
                n = 2; jp = 1; nostart = f & 1; noend = (f & 2) != 0;
            } else {
                nostart = ascii_no_start(ch);
                noend = ascii_no_end(ch);
            }
            if (p == 0) first = n;
            else {                         /* p の直前で改行できるか（行は [0,p)） */
                bound = p;
                if (ch == ' ') best = p;
                else if ((jp || prev_jp) && !nostart && !prev_noend) best = p;
            }
            prev_jp = jp; prev_noend = noend;
            p += n;
        }
        if (best > 0) si = best;
        else if (cur != 0) si = 0;
        else si = bound > 0 ? bound : first;
    }
    SET_SI(si);
    return 0;
}

static int g_ncalls;
#define WRAP(name) static uint32_t __cdecl w_##name(void) { uint32_t r; \
    if (g_ncalls < 40) dbg("cb %s sp=%x bp=%x\n", #name, R_SP, R_BP); \
    r = name(); g_ncalls++; return r; }
WRAP(cb_print) WRAP(cb_wrap) WRAP(cb_wprint)

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    if (g_cb_installed) return 1;
    for (i = 0; i < 4; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy (%x)\n", b + i, tab[b + i]); return 0; }
    tab[b + 0] = (uint32_t)w_cb_print;
    tab[b + 1] = (uint32_t)w_cb_wrap;
    tab[b + 2] = (uint32_t)w_cb_wprint;
    tab[b + 3] = (uint32_t)cb_fetch;
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

/* ----------------------------------------------------- 差し替えファイルの作成 */
typedef FILE *(__cdecl *fopen_t)(const char *, const char *);
static fopen_t real_fopen;

static uint8_t *read_all(const char *path, uint32_t *len)
{
    FILE *f = fopen(path, "rb");   /* 自前の CRT（msvcrt の FILE* と混ぜない） */
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

/* 'X' 節: u32 元サイズ, u32 元CRC, {u32 位置, u32 長さ, バイト列}... （同じ大きさのまま書き換え） */
static uint8_t *build_x(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1];
    const uint8_t *p = sec + 8, *e = sec + slen;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("X mismatch\n"); return NULL; }
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

/* Westwood LCW（format 80）展開 */
static uint32_t lcw_decode(const uint8_t *s, uint32_t slen, uint8_t *d, uint32_t dmax)
{
    uint32_t i = 0, o = 0, n, pos, k;
    while (i < slen) {
        uint8_t c = s[i++];
        if (c == 0x80) break;
        if (!(c & 0x80)) {
            n = ((c & 0x70) >> 4) + 3;
            pos = ((c & 0x0f) << 8) | s[i++];
            for (k = 0; k < n && o < dmax; k++, o++) d[o] = (o >= pos) ? d[o - pos] : 0;
        } else if (!(c & 0x40)) {
            n = c & 0x3f;
            for (k = 0; k < n && o < dmax && i < slen; k++) d[o++] = s[i++];
        } else if (c == 0xfe) {
            n = s[i] | (s[i + 1] << 8); i += 2;
            for (k = 0; k < n && o < dmax; k++) d[o++] = s[i];
            i++;
        } else if (c == 0xff) {
            n = s[i] | (s[i + 1] << 8); pos = s[i + 2] | (s[i + 3] << 8); i += 4;
            for (k = 0; k < n && o < dmax; k++, o++) d[o] = d[pos + k];
        } else {
            n = (c & 0x3f) + 3; pos = s[i] | (s[i + 1] << 8); i += 2;
            for (k = 0; k < n && o < dmax; k++, o++) d[o] = d[pos + k];
        }
    }
    return o;
}

/* 'S' 節: u32 元サイズ, u32 元CRC, u32 種別(1=CPS/LCW), u32 個数, {u32 始, u32 終, u32 長さ, バイト列}...
 * 元の（展開後の）中身の [始,終) を差し替える。配布物に元データを含めないための形式。 */
static uint8_t *build_s(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    const uint32_t *h = (const uint32_t*)sec;
    uint32_t osz = h[0], ocrc = h[1], kind = h[2], n = h[3], k, cur = 0, total, ulen;
    const uint8_t *p, *src;
    uint8_t *un = NULL, *mid, *out;
    (void)slen;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("S mismatch\n"); return NULL; }
    if (kind == 1) {
        uint32_t usz = orig[4] | (orig[5] << 8) | (orig[6] << 16) | (orig[7] << 24);
        uint32_t pal = orig[8] | (orig[9] << 8);
        un = (uint8_t*)malloc(usz + 16);
        ulen = lcw_decode(orig + 10 + pal, olen - 10 - pal, un, usz);
        src = un;
    } else {
        src = orig; ulen = olen;
    }
    total = ulen;
    p = sec + 16;
    for (k = 0; k < n; k++) {
        const uint32_t *r = (const uint32_t*)p;
        total = total - (r[1] - r[0]) + r[2];
        p += 12 + r[2];
    }
    mid = (uint8_t*)malloc(total + 1);
    p = sec + 16;
    total = 0;
    for (k = 0; k < n; k++) {
        const uint32_t *r = (const uint32_t*)p;
        if (r[0] < cur || r[1] > ulen || r[0] > r[1]) { free(mid); free(un); dbg("bad splice\n"); return NULL; }
        memcpy(mid + total, src + cur, r[0] - cur); total += r[0] - cur;
        memcpy(mid + total, p + 12, r[2]); total += r[2];
        cur = r[1];
        p += 12 + r[2];
    }
    memcpy(mid + total, src + cur, ulen - cur); total += ulen - cur;
    free(un);
    if (kind != 1) { *outlen = total; return mid; }
    {   /* CPS に包み直す（LCW のそのまま転記だけで符号化） */
        uint32_t chunks = (total + 62) / 63, clen = total + chunks + 1, o = 10, q;
        out = (uint8_t*)malloc(10 + clen);
        out[2] = 4; out[3] = 0;
        out[4] = (uint8_t)total; out[5] = (uint8_t)(total >> 8); out[6] = (uint8_t)(total >> 16); out[7] = (uint8_t)(total >> 24);
        out[8] = 0; out[9] = 0;
        for (q = 0; q < total; q += 63) {
            uint32_t m = total - q > 63 ? 63 : total - q;
            out[o++] = (uint8_t)(0x80 | m);
            memcpy(out + o, mid + q, m); o += m;
        }
        out[o++] = 0x80;
        out[0] = (uint8_t)(o - 2); out[1] = (uint8_t)((o - 2) >> 8);
        free(mid);
        *outlen = o;
        return out;
    }
}

static const char *base_name(const char *p)
{
    const char *b = p, *q;
    for (q = p; *q; q++) if (*q == '\\' || *q == '/' || *q == ':') b = q + 1;
    return b;
}

static CRITICAL_SECTION g_cs;
static int g_exe_ok;

static const char *patched_path(const char *orig_path, char *buf)
{
    const char *bn = base_name(orig_path);
    uint32_t slen, olen, nlen;
    const uint8_t *sx, *ss;
    uint8_t *orig, *out = NULL;
    if (!g_enabled) return NULL;
    sx = find_sect('X', bn, &slen);
    ss = sx ? NULL : find_sect('S', bn, &slen);
    if (!sx && !ss) return NULL;
    if (_stricmp(bn, "START.EXE") && !g_exe_ok) return NULL;   /* 対応版の START.EXE のときだけ、ほかも差し替える */
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) {
        if (!_stricmp(bn, "START.EXE")) {       /* 元のファイルが変わっていないか（大きさで）確かめる */
            WIN32_FILE_ATTRIBUTE_DATA fa, fb;
            if (!(GetFileAttributesExA(orig_path, GetFileExInfoStandard, &fa) && GetFileAttributesExA(buf, GetFileExInfoStandard, &fb)
                  && fa.nFileSizeLow == fb.nFileSizeLow)) return NULL;
            g_exe_ok = 1;
        }
        return buf;
    }
    {   /* 古い版のコピーを消す */
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
    out = sx ? build_x(orig, olen, sx, slen, &nlen) : build_s(orig, olen, ss, slen, &nlen);
    free(orig);
    if (!out) return NULL;
    if (!write_all(buf, out, nlen)) { free(out); return NULL; }
    if (!_stricmp(bn, "START.EXE")) g_exe_ok = 1;
    free(out);
    dbg("built %s\n", buf);
    return buf;
}

/* ------------------------------------------- 自動地図 Where Are We? との相性 */
/*
 * Where Are We? は、START.EXE の環境ブロックの「\1\0C:\START.EXE\0」を目印に、そこから決まった距離
 * （+0x398D8）でパーティーを読む。DOSBox（0.74）の環境ブロックは「環境変数の長さ+83」を 16 バイトに
 * 切り上げた大きさなので、目印とゲームのデータの距離は環境変数の長さで 0〜15 バイト変わり、
 * Where Are We? の想定どおりになるのは長さ（最後の \0\0 まで）が 16 の倍数のときだけ。
 * DOSBox の既定（PATH・COMSPEC・BLASTER）は 58 バイトで 10 バイトずれるので、START.EXE を起動する
 * 直前に、親（シェル）の環境へ詰め物の変数 EOB2JP を足して長さを 16 の倍数にする。
 */
#define PAD_NAME "EOB2JP="
#define ENV_MAX 4096
static int env_has_start_exe(const uint8_t *e, int tail, int n)
{
    /* 環境の後ろのプログラム名（\1\0 のあと）が START.EXE か */
    int i;
    if (tail + 2 >= n || e[tail] != 1 || e[tail + 1] != 0) return 0;
    for (i = tail + 2; i < n && e[i]; i++) ;
    return i - (tail + 2) >= 9 && !_strnicmp((const char*)e + i - 9, "START.EXE", 9);
}
static void env_pad(void)
{
    static uint8_t e[ENV_MAX], o[ENV_MAX];
    uint32_t psp = rw(0xb30);                 /* DOS の SDA（0xB2:0000）の現在の PSP */
    uint32_t seg, mcb, cap, base;
    int i, len, no, tail, tlen, pad;
    if (psp < 0x40) return;
    seg = rw(psp * 16 + 0x2c);
    if (seg < 0x40) return;
    mcb = (seg - 1) * 16;
    if (rb(mcb) != 'M' && rb(mcb) != 'Z') return;
    cap = (uint32_t)rw(mcb + 3) * 16;
    if (cap > ENV_MAX) cap = ENV_MAX;
    base = seg * 16;
    for (i = 0; i < (int)cap; i++) e[i] = rb(base + i);
    /* 変数を読み、前に足した EOB2JP は除いて o に並べ直す */
    for (i = 0, no = 0; i < (int)cap && e[i]; i += len + 1) {
        for (len = 0; i + len < (int)cap && e[i + len]; len++) ;
        if (i + len >= (int)cap) return;
        if (!_strnicmp((const char*)e + i, PAD_NAME, sizeof(PAD_NAME) - 1)) continue;
        memcpy(o + no, e + i, len + 1); no += len + 1;
    }
    if (i >= (int)cap) return;
    tail = i + 1;                               /* \0\0 のあと（\1\0 とプログラム名） */
    if (env_has_start_exe(e, tail, cap)) return;   /* ゲーム自身の環境には触らない */
    for (tlen = 0; tail + tlen < (int)cap; tlen++)
        if (tlen >= 2 && !e[tail + tlen]) { tlen++; break; }
    if (tail + 2 > (int)cap || e[tail] != 1 || e[tail + 1] != 0) tlen = 0;
    pad = (16 - (no + 1) % 16) % 16;             /* 子の環境の長さ = 変数 + 最後の \0 */
    if (pad && pad < (int)sizeof(PAD_NAME) + 1) pad += 16;
    if (pad) {
        memcpy(o + no, PAD_NAME, sizeof(PAD_NAME) - 1);
        memset(o + no + sizeof(PAD_NAME) - 1, '0', pad - sizeof(PAD_NAME));
        o[no + pad - 1] = 0;
        no += pad;
    }
    o[no++] = 0;
    if (no + tlen > (int)cap) return;
    memcpy(o + no, e + tail, tlen);
    for (i = 0; i < no + tlen; i++) if (rb(base + i) != o[i]) wb(base + i, o[i]);
    dbg("env pad %d (vars %d)\n", pad, no - 1);
}

/* 窓の題名の「Program:    START」を「Program:     EOB2」に替える（Where Are We? は START を EoB1 と見る） */
typedef void (__cdecl *setcap_t)(const char *, const char *);
static setcap_t real_setcap;
static void __cdecl hook_setcap(const char *title, const char *icon)
{
    char buf[256];
    size_t n = title ? strlen(title) : 0;
    if (g_exe_ok && n >= 8 && n < sizeof(buf) && !strcmp(title + n - 8, "   START")) {
        memcpy(buf, title, n - 8);
        memcpy(buf + n - 8, "    EOB2", 9);
        title = buf;
    }
    real_setcap(title, icon);
}

static FILE *__cdecl hook_fopen(const char *name, const char *mode)
{
    char buf[MAX_PATH];
    const char *pp;
    FILE *f;
    if (!name || !g_enabled || (mode && strchr(mode, 'w'))) return real_fopen(name, mode);
    EnterCriticalSection(&g_cs);
    if (!_stricmp(base_name(name), "START.EXE")) env_pad();
    pp = patched_path(name, buf);
    if (pp && !install_callbacks()) pp = NULL;
    LeaveCriticalSection(&g_cs);
    if (pp) {
        /* コピーにゲームが書き込まないようにする */
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
        int crt = !_stricmp(dll, "msvcrt.dll"), sdl = !_stricmp(dll, "SDL.dll");
        if (!crt && !sdl) continue;
        ot = (IMAGE_THUNK_DATA*)(base + (id->OriginalFirstThunk ? id->OriginalFirstThunk : id->FirstThunk));
        ft = (IMAGE_THUNK_DATA*)(base + id->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;
            if (ot->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME*)(base + ot->u1.AddressOfData);
            if (crt && !strcmp((const char*)ibn->Name, "fopen")) {
                VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
                ft->u1.Function = (DWORD)(uintptr_t)hook_fopen;
                VirtualProtect(&ft->u1.Function, 4, old, &old);
                done = 1;
            } else if (sdl && !strcmp((const char*)ibn->Name, "SDL_WM_SetCaption")) {
                real_setcap = (setcap_t)(uintptr_t)ft->u1.Function;
                VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
                ft->u1.Function = (DWORD)(uintptr_t)hook_setcap;
                VirtualProtect(&ft->u1.Function, 4, old, &old);
            }
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
    wsprintfA(g_dir, "%s\\EOB2_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
#ifdef DEVTEST
    {
        void dev_init(const char *gamedir);
        char gd[MAX_PATH], *q;
        GetModuleFileNameA(self, gd, MAX_PATH);
        if ((q = strrchr(gd, '\\')) != NULL) *q = 0;
        if ((q = strrchr(gd, '\\')) != NULL) *q = 0;
        dev_init(gd);
    }
#endif
    dbg("EOB2 JP patch active, blob %08X\n", g_blob_crc);
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
