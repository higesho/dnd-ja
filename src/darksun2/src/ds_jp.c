/*
 * Dark Sun: Wake of the Ravager 日本語化パッチ（実行時部分）
 * DOSBox 0.74-3（Dark Sun Series 同梱版）に winmm.dll の代理として読み込まれる。
 * ゲームのファイルは書き換えない。DOSBox がファイルを開くとき（fopen）に、次のファイルは手を入れたコピーを渡す:
 *  - DSUN.EXE: 次の場所に DOSBox のコールバック（FE 38 nn nn）を置く（オーバーレイごとファイルから読まれるので確実に入る）
 *      復号した GPL の文（0x98f0）   … 訳の表にあれば日本語に差し替える
 *      1 字描画（0x33935）           … 0x80 以上の字は、その字形をフォントの枠に書いてから描かせる
 *      折り返しの語の区切り（0x30dd1）… 日本語は 1 字ごとに区切る
 *      はみ出しの判定（0x30d12）      … 行頭禁則の字（。、」など）は行末にぶら下げる
 *      位置指定の printf（0x33ad9）  … 書式文字列と %s の文字列を訳す
 *    あわせて、字幅を測る 2 か所が字を符号付きで読むのを符号なしに直す
 *  - RESOURCE.GFF: フォント（FONT 100）の 0x80〜0xFE を幅 8 の空の枠にしたものをファイルの末尾に足し、目次を書き換える
 * 日本語の字はゲームの中では 1 字 1 バイト（0x80〜0xFE）の番号で表し、番号と字の対応は使うたびに割り当てる（LRU）。
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "resource.h"

void proxy_init(void);

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

/* ------------------------------------------------ DOSBox 0.74-3 の中身 */
static uint32_t g_dbx, g_dbx_code;   /* DOSBox の版: Steam 0.74-3 = 0、GOG 0.74-2.1 = +0xe000 / -0x10 */
#define A_CB_HANDLERS (0x74d460u + g_dbx)
#define A_CB_ILLEGAL (0x415e40u + g_dbx_code)
#define A_REGS (0x74d1e0u + g_dbx)
#define A_SEGPHYS (0x74d110u + g_dbx)
#define A_SEGVAL (0x74d100u + g_dbx)
#define A_TLB_READ (0x74d6d0u + g_dbx)
#define A_TLB_WRITE (0xb4d6d0u + g_dbx)
#define A_TLB_RH (0xf4d6d0u + g_dbx)
#define A_TLB_WH (0x134d6d0u + g_dbx)
typedef uint32_t (__cdecl *ph_rb)(void *self, uint32_t a);
typedef void (__cdecl *ph_wb)(void *self, uint32_t a, uint32_t v);
static int verify_dosbox(void)
{
    /* Steam（DOSBox 0.74-3）と GOG（DOSBox 0.74-2.1）: コードは同じで、変数の場所が g_dbx、IllegalHandler が g_dbx_code だけずれている */
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
#define REG32(i) (((uint32_t*)A_REGS)[i])
#define R_DX ((uint16_t)REG32(2))
#define R_SP ((uint16_t)REG32(4))
#define R_BP ((uint16_t)REG32(5))
#define R_SI ((uint16_t)REG32(6))
#define SET_DX(v) (*(uint16_t*)&REG32(2) = (uint16_t)(v))
#define SET_SP(v) (*(uint16_t*)&REG32(4) = (uint16_t)(v))
#define SET_BP(v) (*(uint16_t*)&REG32(5) = (uint16_t)(v))
#define SET_IP(v) (*(uint16_t*)&REG32(8) = (uint16_t)(v))
#define R_IP ((uint16_t)REG32(8))
#define PHYS_ES (((uint32_t*)A_SEGPHYS)[0])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
static void push16(uint16_t v) { SET_SP(R_SP - 2); ww(PHYS_SS + R_SP, v); }
static uint32_t farp(uint32_t a) { return ((uint32_t)rw(a + 2) << 4) + rw(a); }

/* ------------------------------------------------------- パッチデータ */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)
static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;
#define IS_LEAD(c) ((c) >= 0x81 && (c) <= 0xfe)
#define IS_TRAIL(c) ((c) >= 0x80 && (c) <= 0xfe)
static uint16_t *g_glyph_index;
static const uint8_t *g_glyphs;
static const uint8_t *g_gflags;
static int g_cb_base = 0x70;
static int g_enabled;
static const uint8_t *g_epatch; static uint32_t g_epatch_len;     /* 'E' 節: DSUN.EXE の書き換え */
static void load_table(const uint8_t *d);
static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xffffffffu, k;
    while (n--) { c ^= *p++; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1))); }
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
    if (memcmp(h->magic, "DSJ1", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len);
    p = g_blob + sizeof(blob_hdr);
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == 'F') {
            uint32_t n = *(const uint32_t*)d, k;
            const uint16_t *codes = (const uint16_t*)(d + 4);
            g_glyphs = d + 4 + n * 2;
            g_gflags = g_glyphs + n * 8;
            g_glyph_index = (uint16_t*)calloc(65536, 2);
            for (k = 0; k < n; k++) g_glyph_index[codes[k]] = (uint16_t)(k + 1);
        } else if (s->type == 'T') {
            load_table(d);
        } else if (s->type == 'E') {
            g_epatch = d; g_epatch_len = s->len;
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0];
            g_logon = (int)v[1];
        }
        p = d + s->len;
    }
    return g_glyph_index != NULL && g_epatch != NULL;
}

#include "trtable.inc"

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

/* ------------------------------------------------ 日本語の字の動的な番号（0x80〜0xFE の 127 字） */
#define NDYN 127
static uint16_t g_dyn_code[NDYN];
static uint32_t g_dyn_stamp[NDYN];
static uint32_t g_clock = 1;
static int dyn_get(uint16_t code)
{
    int i, best = 0;
    for (i = 0; i < NDYN; i++)
        if (g_dyn_code[i] == code) { g_dyn_stamp[i] = g_clock++; return 0x80 + i; }
    for (i = 1; i < NDYN; i++)
        if (g_dyn_stamp[i] < g_dyn_stamp[best]) best = i;
    g_dyn_code[best] = code;
    g_dyn_stamp[best] = g_clock++;
    return 0x80 + best;
}
static int glyph_of(uint16_t code) { return g_glyph_index ? g_glyph_index[code] : 0; }
static int dyn_flags(int c)
{
    int g;
    if (c < 0x80 || c > 0xfe) return 0;
    g = glyph_of(g_dyn_code[c - 0x80]);
    return g ? g_gflags[g - 1] : 0;
}

/* 表の符号の訳文 → ゲームの字（日本語 1 字 = 1 バイト）。0 終端を含む長さを返す */
static int to_game(const uint8_t *s, int n, uint8_t *out, int max)
{
    int i = 0, o = 0;
    while (i < n && o < max - 1) {
        if (IS_LEAD(s[i]) && i + 1 < n && IS_TRAIL(s[i + 1])) {
            out[o++] = (uint8_t)dyn_get((uint16_t)((s[i] << 8) | s[i + 1]));
            i += 2;
        } else out[o++] = s[i++];
    }
    out[o++] = 0;
    return o;
}

static int read_cstr(uint32_t a, uint8_t *buf, int max)
{
    int n = 0;
    while (n < max) { uint8_t c = rb(a + n); if (!c) break; buf[n++] = c; }
    return n;
}

/* ---- 訳して渡した文字列の覚え書き ----
 * 日本語の字は 0x80〜0xFE の 127 個の番号を使い回す。ゲームが 1 回だけ読み込んで後で何度も使う文（選択肢の見出し「何と言う？」など）は、
 * 後で番号が別の字に使い回されると字が化ける。訳して渡したときの並び（ゲームの番号の列）と訳の対応を覚えておき、
 * 表示の直前（sprintf / printf / 窓の文章）に、番号がまだ同じ字を指しているか確かめ、ずれていたら番号を取り直してその場で書き直す。 */
#define NMEMO 16384
typedef struct { uint64_t h; const tentry *e; } memo_t;
static memo_t g_memo[NMEMO];
static void memo_put(const uint8_t *gs, int n, const tentry *e)
{
    uint64_t h = fnv64(gs, n);
    memo_t *m = &g_memo[(uint32_t)h & (NMEMO - 1)];
    m->h = h; m->e = e;
}
static int has_hi(const uint8_t *s, int n)
{
    int k;
    for (k = 0; k < n; k++) if (s[k] >= 0x80) return 1;
    return 0;
}
/* addr にある文字列 s（長さ n、日本語の番号を含む）が覚え書きにあれば確かめ、ずれていたら書き直す。書き直したら 1 */
static int memo_fix(uint32_t addr, const uint8_t *s, int n)
{
    static uint8_t gs[600];
    uint64_t h;
    memo_t *m;
    const uint8_t *ja;
    int jl, i = 0, o = 0, bad = 0, k, len;
    if (n <= 0 || n > 580) return 0;
    h = fnv64(s, n);
    m = &g_memo[(uint32_t)h & (NMEMO - 1)];
    if (m->h != h || !m->e) return 0;
    ja = m->e->ja; jl = m->e->jalen;
    while (i < jl && o < n) {
        if (IS_LEAD(ja[i]) && i + 1 < jl && IS_TRAIL(ja[i + 1])) {
            uint16_t code = (uint16_t)((ja[i] << 8) | ja[i + 1]);
            uint8_t b = s[o];
            if (b < 0x80 || b > 0xfe) return 0;
            if (g_dyn_code[b - 0x80] != code) bad = 1;
            else g_dyn_stamp[b - 0x80] = g_clock++;        /* 使用中 */
            i += 2; o++;
        } else {
            if (s[o] != ja[i]) return 0;
            i++; o++;
        }
    }
    if (i != jl || o != n) return 0;
    if (!bad) return 0;
    len = to_game(ja, jl, gs, 590);
    if (len != n + 1) return 0;
    for (k = 0; k < n; k++) wb(addr + k, gs[k]);
    memo_put(gs, n, m->e);
    return 1;
}

#define NSEEN 16384
static uint64_t g_seen[NSEEN];
static void log_miss(const char *tag, const uint8_t *s, int n)
{
    uint64_t h;
    uint32_t i;
    char buf[1200];
    int k, o = 0;
    if (!g_logon || n <= 0) return;
    h = fnv64(s, n);
    i = (uint32_t)h & (NSEEN - 1);
    while (g_seen[i]) { if (g_seen[i] == h) return; i = (i + 1) & (NSEEN - 1); }
    g_seen[i] = h;
    for (k = 0; k < n && o < 1100; k++) {
        uint8_t c = s[k];
        if (c < 0x20 || c >= 0x7f) o += wsprintfA(buf + o, "{%02x}", c); else buf[o++] = (char)c;
    }
    buf[o] = 0;
    dbg("%s %s\n", tag, buf);
}

/* 訳文は元の文字列の場所に書き、同じフックが次に呼ばれたときに書き戻す（定数の文字列を何度でも訳し直せるように） */
typedef struct { uint32_t addr; int n, wn; uint8_t save[600], wr[600]; } restore_t;
static restore_t g_rs_fmt, g_rs_arg[4];
static void restore(restore_t *r)
{
    int k;
    if (!r->n) return;
    for (k = 0; k < r->wn; k++) if (rb(r->addr + k) != r->wr[k]) break;
    if (k == r->wn) for (k = 0; k < r->n; k++) wb(r->addr + k, r->save[k]);
    r->n = 0;
}
static int put_inplace(restore_t *r, uint32_t addr, const uint8_t *en, int n, const uint8_t *gs, int m)
{
    int k;
    if (m > n + 1 || m > 590) return 0;
    memcpy(r->save, en, n); r->save[n] = 0; r->n = n + 1;
    r->addr = addr;
    for (k = 0; k < m; k++) wb(addr + k, gs[k]);
    memcpy(r->wr, gs, m); r->wn = m;
    return 1;
}

/* ================================================================ コールバック */

/* 0: GPL の文を復号した後（mov dx,es:[25b] の代わり）。es:[259] = 文字列の far ptr（バッファ 300 バイト） */
static uint32_t __cdecl cb_text(void)
{
    uint32_t es = PHYS_ES, src = farp(es + 0x259);
    static uint8_t en[320], gs[320];
    int n = read_cstr(src, en, 299);
    const tentry *e = n > 0 ? lookup(en, n) : NULL;
    if (e && e->jalen) {
        int m = to_game(e->ja, e->jalen, gs, 299), k;
        for (k = 0; k < m; k++) wb(src + k, gs[k]);
        memo_put(gs, m - 1, e);
    } else if (e) wb(src, 0);                     /* 訳が "-": 何も出さない（前の行に訳をまとめた） */
    else if (n > 1) log_miss("G", en, n);
    SET_DX(rw(es + 0x25b));
    return 0;
}

/* フォント（RESOURCE.GFF で枠を足したもの: 頭の +4 に 'JP'）の字 ch（0x80〜0xFE）の枠に、その番号の日本語の字形を書く */
static void put_glyph(uint32_t font, int ch, int squash)
{
    static const uint8_t tofu[8] = {0x00,0x7e,0x42,0x42,0x42,0x42,0x7e,0x00};
    uint32_t slot;
    uint16_t code;
    int g, r, c;
    const uint8_t *bits;
    uint8_t px[9][8];
    if (rb(font + 4) != 'J' || rb(font + 5) != 'P') return;
    slot = font + rw(font + 0x108 + ch * 2);
    if (rb(slot) != 8) return;
    code = g_dyn_code[ch - 0x80];
    g = code ? glyph_of(code) : 0;
    bits = g ? g_glyphs + (g - 1) * 8 : tofu;
    if (squash) {      /* 行の詰まった欄用: 7 行の字形の、いちばん似た隣り合う 2 行を重ねて 6 行にし、英大文字と同じ 2～7 行目に置く */
        static uint8_t sq[8];
        int best = 1, bd = 99, k, o = 2;
        for (k = 0; k < 6; k++) {
            int x = bits[k] ^ bits[k + 1], d = 0;
            while (x) { d += x & 1; x >>= 1; }
            if (d < bd) { bd = d; best = k; }
        }
        memset(sq, 0, 8);
        for (k = 0; k < 7; k++) {
            if (k == best) { sq[o++] = (uint8_t)(bits[k] | bits[k + 1]); k++; }
            else sq[o++] = bits[k];
            if (o > 7) break;
        }
        bits = sq;
    }
    memset(px, 0, sizeof px);
    /* 美咲フォントの 8 行を 0～7 行目に置き、右下に影（0x14）を付ける（英字と同じ）。字の色は 0xFE */
    for (r = 0; r < 8; r++)
        for (c = 0; c < 7; c++)
            if (bits[r] & (0x80 >> c)) px[r + 1][c + 1] = 0x14;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            if (bits[r] & (0x80 >> c)) px[r][c] = 0xfe;
    for (r = 0; r < 9; r++)
        for (c = 0; c < 8; c++) wb(slot + 2 + r * 8 + c, px[r][c]);
}

/* 1: 1 字描画の入口（push bp / mov bp,sp / sub sp,1c の代わり）。[sp+4] = 字。今のフォントは DS:a34e */
static uint32_t __cdecl cb_drawch(void)
{
    int ch = rb(PHYS_SS + R_SP + 4);
    if (ch >= 0x80 && ch <= 0xfe) { g_dyn_stamp[ch - 0x80] = g_clock++; put_glyph(farp(PHYS_DS + 0xa34e), ch, 0); }   /* 描いた字は「使用中」: 枠の使い回しで後回しにする */
    push16(R_BP);
    SET_BP(R_SP);
    SET_SP(R_SP - 0x1c);
    return 0;
}

/* 2: 折り返しの語の区切り（cmp word [bp-24],0 / je 30dea の代わり。この位置は 30dd1）。
 * [bp-a] = 字, si = 字の位置, [bp-10] = 字幅, [bp-24] = 語の中か, [bp-1c] = 今の語の頭, [bp-1a] = 前に終わった語, [bp-1e] = 今の語の幅
 * 日本語の字は、その字だけで終わった語にする（次の字の前で折り返せる）。行頭禁則の字は前の字に付ける */
static uint32_t __cdecl cb_wrap(void)
{
    uint32_t bp = PHYS_SS + R_BP;
    int c = rb(bp - 0x0a);
    int in_word = rw(bp - 0x24) != 0;
    uint16_t ip0 = (uint16_t)(R_IP - 4);         /* 30dd1 に当たる区分内の位置 */
    if (c >= 0x80 && c <= 0xfe && (dyn_flags(c) & 2)) {   /* 行末禁則（「（など）: 次の字と 1 つの語にする */
        if (in_word) ww(bp - 0x1a, rw(bp - 0x1c));
        ww(bp - 0x1c, R_SI);
        ww(bp - 0x1e, rw(bp - 0x10));
        ww(bp - 0x24, 1);
        SET_IP(ip0 + (0x30dfe - 0x30dd1));
        return 0;
    }
    if (c >= 0x80 && c <= 0xfe) {             /* 日本語の字（行頭禁則の字も）: その字で終わった語にする。はみ出しは cb_over でぶら下げる */
        if (in_word && !(dyn_flags(c) & 1)) ww(bp - 0x1a, rw(bp - 0x1c));
        else if (in_word) { SET_IP(ip0 + (0x30dfe - 0x30dd1)); return 0; }   /* 「の直後の。など: 語を続ける */
        ww(bp - 0x1a, R_SI);
        ww(bp - 0x1c, 0xffff);
        ww(bp - 0x1e, 0);
        ww(bp - 0x24, 0);
        SET_IP(ip0 + (0x30dfe - 0x30dd1));
        return 0;
    }
    SET_IP(ip0 + (in_word ? (0x30dd7 - 0x30dd1) : (0x30dea - 0x30dd1)));
    return 0;
}

/* 3: はみ出しの判定（cmp ax,[bp-12] / ja 30d1a の代わり。この位置は 30d12）。ax = 行の幅 + 字幅。
 * 行頭禁則の字は 8 ドットまではみ出してよい */
static uint32_t __cdecl cb_over(void)
{
    uint32_t bp = PHYS_SS + R_BP;
    int c = rb(bp - 0x0a);
    uint16_t ax = (uint16_t)REG32(0), mx = rw(bp - 0x12);
    uint16_t ip0 = (uint16_t)(R_IP - 4);
    int over = ax > mx;
    if (over && c >= 0x80 && c <= 0xfe && (dyn_flags(c) & 1) && ax <= mx + 8) over = 0;
    SET_IP(ip0 + (over ? (0x30d1a - 0x30d12) : (0x30d17 - 0x30d12)));
    return 0;
}

/* %s の文字列を訳の表で引く。そのままで見つからなければ、終わりの改行・空白と、頭の「+1 」「-2 」などを外して引き、
 * 見つかったら外した部分を訳の前後に付け直したものを out に作る（武器の名前 "Sling\r"、"+1 Longsword\r" など）。out の長さ（0 終端を含む）を返す */
static int lookup_flex(const uint8_t *en, int n, uint8_t *out, int max)
{
    const tentry *e;
    int h = 0, t = n, m, k;
    if (n <= 0) return 0;
    while (t > 0 && (en[t - 1] == 13 || en[t - 1] == 10 || en[t - 1] == ' ')) t--;
    if (t >= 3 && (en[0] == '+' || en[0] == '-') && en[1] >= '0' && en[1] <= '9') {
        h = 1;
        while (h < t && en[h] >= '0' && en[h] <= '9') h++;
        if (h < t && en[h] == ' ') h++; else h = 0;
    }
    if (h == 0 && t == n) return 0;
    e = lookup(en + h, t - h);
    if (!e || !e->jalen) return 0;
    for (k = 0; k < h; k++) out[k] = en[k];
    m = to_game(e->ja, e->jalen, out + h, max - h - (n - t)) - 1;    /* 0 終端を除いた長さ */
    for (k = 0; k < n - t; k++) out[h + m + k] = en[t + k];
    out[h + m + (n - t)] = 0;
    return h + m + (n - t) + 1;
}

/* 元の文字列より長い訳（"NO" → 「いいえ」など）は元の場所に書けない。フォントの後ろに足した作業用の場所に書き、
 * スタックの上の far ポインタ（ptr_at）をそこへ向け直す。場所は 5 つ（書式 1 + 引数 4）で、呼ばれるたびに使い回す */
#define SCRATCH_SLOT 200
static int put_scratch(uint32_t ptr_at, int slot, const uint8_t *gs, int m)
{
    uint32_t font = farp(PHYS_DS + 0xa34e), s;
    int k;
    if (m > SCRATCH_SLOT || slot < 0 || slot > 4) return 0;
    if (rb(font + 4) != 'J' || rb(font + 5) != 'P') return 0;
    s = font + rw(font + 0x108 + 0xfe * 2) + 2 + 8 * 9;         /* 最後の字の枠の次 */
    if (rb(s) != 'S' || rb(s + 1) != 'C' || rb(s + 2) != 'R' || rb(s + 3) != 'A') return 0;
    s += 8 + slot * SCRATCH_SLOT;
    for (k = 0; k < m; k++) wb(s + k, gs[k]);
    ww(ptr_at, (uint16_t)(s & 15)); ww(ptr_at + 2, (uint16_t)(s >> 4));
    return 1;
}

/* 4: 位置指定の printf（push bp / mov bp,sp / sub sp,1e の代わり）。[sp+4]=x [sp+6]=y [sp+8]=書式 far ptr、[sp+c] から引数 */
/* 書式文字列（fmt_at の far ptr）と、args_at からの %s の文字列を訳す（元の場所に書き、次の呼び出しで戻す） */
typedef struct { restore_t f, a[4]; } fmt_rs_t;
static void tr_format(fmt_rs_t *rs, uint32_t fmt_at, uint16_t ap, int do_args, const char *tag)
{
    uint32_t ss = PHYS_SS;
    uint32_t src = farp(fmt_at);
    static uint8_t en[600], gs[600], f[120];
    int n, k, ai = 0, si = 0, fn;
    const tentry *e;
    restore(&rs->f);
    for (k = 0; k < 4; k++) restore(&rs->a[k]);
    fn = read_cstr(src, f, 119);
    for (k = 0; k < fn && ai < 4 && do_args; k++) {        /* %s の文字列 */
        if (f[k] != '%') continue;
        k++;
        while (k < fn && strchr("-+ #0123456789.", f[k])) k++;
        if (k < fn && (f[k] == 'l' || f[k] == 'F' || f[k] == 'N')) {
            if (f[k] == 'l') { k++; ap += 4; continue; }
            k++;                                  /* %Fs = far の文字列 */
        }
        if (k >= fn || f[k] == '%') continue;
        if (f[k] == 's') {
            uint32_t a = farp(ss + ap);
            int an = read_cstr(a, en, 580);
            const tentry *ae;
            if (has_hi(en, an)) { memo_fix(a, en, an); ap += 4; continue; }   /* もう訳してある文: 字の番号を確かめる */
            ae = an > 0 ? lookup(en, an) : NULL;
            if (ae && ae->jalen) {
                int m = to_game(ae->ja, ae->jalen, gs, 590);
                if (put_inplace(&rs->a[ai], a, en, an, gs, m)) ai++;
                else if (si < 4 && put_scratch(ss + ap, si, gs, m)) si++;
                else log_miss("S>", en, an);
            } else if (!ae && an > 1) {
                int m = lookup_flex(en, an, gs, 590);
                if (m > 0) {
                    if (put_inplace(&rs->a[ai], a, en, an, gs, m)) ai++;
                    else if (si < 4 && put_scratch(ss + ap, si, gs, m)) si++;
                } else log_miss("S", en, an);
            }
            ap += 4;
        } else if (f[k] == 'C') ap += 4;            /* %C（色）は語 2 つを取る */
        else if (f[k] == 'c' || f[k] == 'd' || f[k] == 'u' || f[k] == 'x' || f[k] == 'X' || f[k] == 'i') ap += 2;
        else break;
    }
    n = read_cstr(src, en, 580);
    if (has_hi(en, n)) { memo_fix(src, en, n); return; }
    e = n > 0 ? lookup(en, n) : NULL;
    if (e && e->jalen) {
        int m = to_game(e->ja, e->jalen, gs, 590);
        if (!put_inplace(&rs->f, src, en, n, gs, m) && !put_scratch(fmt_at, 4, gs, m)) log_miss("F>", en, n);
    } else if (!e && n > 1) log_miss(tag, en, n);
}

/* 4: 位置指定の printf（push bp / mov bp,sp / sub sp,1e の代わり）。[sp+4]=x [sp+6]=y [sp+8]=書式 far ptr、[sp+c] から引数 */
static fmt_rs_t g_rs_print, g_rs_print2, g_rs_spr;
static uint32_t __cdecl cb_print(void)
{
    tr_format(&g_rs_print, PHYS_SS + R_SP + 8, (uint16_t)(R_SP + 0x0c), 1, "F");
    push16(R_BP); SET_BP(R_SP); SET_SP(R_SP - 0x1e);
    return 0;
}
/* 5: 枠の中の printf（30322: push bp / mov bp,sp / sub sp,1a の代わり）。[sp+c]=書式 far ptr、[sp+10] から引数 */
static uint32_t __cdecl cb_print2(void)
{
    tr_format(&g_rs_print2, PHYS_SS + R_SP + 0x0c, (uint16_t)(R_SP + 0x10), 1, "P");
    push16(R_BP); SET_BP(R_SP); SET_SP(R_SP - 0x1a);
    return 0;
}
/* 6: sprintf（2150e: push bp / mov bp,sp / sub sp,230 の代わり）。[sp+4]=出力先, [sp+8]=書式。書式だけ訳す */
static uint32_t __cdecl cb_sprintf(void)
{
    tr_format(&g_rs_spr, PHYS_SS + R_SP + 8, (uint16_t)(R_SP + 0x0c), 1, "R");
    push16(R_BP); SET_BP(R_SP); SET_SP(R_SP - 0x230);
    return 0;
}
/* 7: 描画ライブラリ（区分 980）の字幅・1 字描画の入口（push bp / mov bp,sp / push ds の代わり）。[sp+4] = 字（語）。
 * 呼び出し側が字を符号付きで渡すので 8 ビットに直し、0x80 以上なら字形をフォントの枠に書く。フォント = (cs:1052 の far ptr)+6 の far ptr */
static uint32_t __cdecl cb_libch(void)
{
    uint32_t ss = PHYS_SS, cs = ((uint32_t*)A_SEGPHYS)[1];
    uint16_t sp = R_SP;
    int ch = rb(ss + sp + 4);
    ww(ss + sp + 4, (uint16_t)ch);
    if (ch >= 0x80 && ch <= 0xfe) { g_dyn_stamp[ch - 0x80] = g_clock++; put_glyph(farp(farp(cs + 0x1052) + 6), ch, 1); }
    push16(R_BP); SET_BP(R_SP); push16(((uint16_t*)A_SEGVAL)[3]);
    return 0;
}

/* 8: 窓に文章を入れる関数（31150: push bp / mov bp,sp / sub sp,8 の代わり）。[sp+4]=窓 far ptr, [sp+8]=文章 far ptr。
 * 呪文の説明など、GPL を通らない文章を訳す（元の場所に書き、次の呼び出しで戻す） */
static restore_t g_rs_win;
static uint32_t __cdecl cb_wintext(void)
{
    uint32_t src = farp(PHYS_SS + R_SP + 8);
    static uint8_t en[600], gs[600];
    int n, k, hi = 0;
    const tentry *e;
    restore(&g_rs_win);
    n = read_cstr(src, en, 580);
    for (k = 0; k < n; k++) if (en[k] >= 0x80) hi = 1;
    if (hi) memo_fix(src, en, n);
    if (!hi && n > 1) {
        int t = n;
        while (t > 0 && (en[t - 1] == 13 || en[t - 1] == 10 || en[t - 1] == 32)) t--;   /* 説明の終わりの改行・空白 */
        int h = 0;
        while (h < t && en[h] == ' ') h++;                 /* 頭の字下げ（絵の横を空ける） */
        e = lookup(en, n);
        if (!e) { e = lookup(en + h, t - h); } else h = 0;
        if (e && e->jalen) {
            int m;
            memset(gs, ' ', h);
            m = h + to_game(e->ja, e->jalen, gs + h, 590 - h);
            if (!put_inplace(&g_rs_win, src, en, n, gs, m)) log_miss("W>", en, n);
        } else if (!e) log_miss("W", en, n);
    }
    push16(R_BP); SET_BP(R_SP); SET_SP(R_SP - 8);
    return 0;
}

#define NCB 9
static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    if (g_cb_installed) return 1;
    for (i = 0; i < NCB; i++) if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot busy\n"); return 0; }
    tab[b + 0] = (uint32_t)cb_text;
    tab[b + 1] = (uint32_t)cb_drawch;
    tab[b + 2] = (uint32_t)cb_wrap;
    tab[b + 3] = (uint32_t)cb_over;
    tab[b + 4] = (uint32_t)cb_print;
    tab[b + 5] = (uint32_t)cb_print2;
    tab[b + 6] = (uint32_t)cb_sprintf;
    tab[b + 7] = (uint32_t)cb_libch;
    tab[b + 8] = (uint32_t)cb_wintext;
    g_cb_installed = 1;
    return 1;
}

/* ================================================================ ファイルの差し替え */
static const char *base_name(const char *p)
{
    const char *b = p, *q;
    for (q = p; *q; q++) if (*q == '\\' || *q == '/') b = q + 1;
    return b;
}
static uint8_t *read_all(const char *path, uint32_t *len)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD sz, got;
    uint8_t *b;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL);
    b = (uint8_t*)malloc(sz + 32768);
    if (!b || !ReadFile(h, b, sz, &got, NULL) || got != sz) { CloseHandle(h); free(b); return NULL; }
    CloseHandle(h);
    *len = sz;
    return b;
}
static int write_all(const char *path, const uint8_t *b, uint32_t n)
{
    char tmp[MAX_PATH];
    HANDLE h;
    DWORD put;
    wsprintfA(tmp, "%s.tmp", path);
    h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (!WriteFile(h, b, n, &put, NULL) || put != n) { CloseHandle(h); DeleteFileA(tmp); return 0; }
    CloseHandle(h);
    DeleteFileA(path);
    return MoveFileA(tmp, path);
}

/* 'E' 節: u32 元の大きさ, u32 数, { u32 位置, u8 長さ, u8 種類(0=バイト列, 1=コールバック k), u8 k, 元[長さ], 新[長さ] } */
static uint8_t *build_exe(uint8_t *b, uint32_t n, uint32_t *outn)
{
    const uint8_t *p = g_epatch;
    uint32_t size = *(const uint32_t*)p, cnt = *(const uint32_t*)(p + 4), i;
    p += 8;
    if (n != size) { dbg("DSUN.EXE: size %u (expected %u)\n", n, size); return NULL; }
    for (i = 0; i < cnt; i++) {
        uint32_t off = *(const uint32_t*)p;
        int len = p[4], kind = p[5], k = p[6];
        const uint8_t *orig = p + 7, *nw = p + 7 + len;
        if (off + len > n || memcmp(b + off, orig, len)) { dbg("DSUN.EXE: mismatch at %x\n", off); return NULL; }
        if (kind == 1) {
            int j;
            b[off] = 0xfe; b[off + 1] = 0x38;
            b[off + 2] = (uint8_t)(g_cb_base + k); b[off + 3] = (uint8_t)((g_cb_base + k) >> 8);
            for (j = 4; j < len; j++) b[off + j] = 0x90;
        } else memcpy(b + off, nw, len);
        p += 7 + 2 * len;
    }
    *outn = n;
    return b;
}

#define SCRATCH_LEN (8 + 5 * 200)
/* RESOURCE.GFF: FONT 100 の 0x80～0xFE を幅 8・高さ 9 の空の枠にしたフォントを末尾に足す */
static uint8_t *build_gff(uint8_t *b, uint32_t n, uint32_t *outn)
{
    uint32_t tloc, p, ntypes, t, fl = 0, floc = 0, entry = 0;
    uint32_t gffi_p = 0, gffi_cnt = 0, seg_gid = 0, seg_idx = 0, seg_total = 0;
    int seg_font = 0;
    uint8_t *nb;
    if (n < 0x1c || memcmp(b, "GFFI", 4)) return NULL;
    tloc = *(uint32_t*)(b + 12);
    p = tloc + *(uint32_t*)(b + tloc);
    ntypes = *(uint16_t*)(b + p); p += 2;
    for (t = 0; t < ntypes && p + 8 <= n; t++) {
        uint32_t cnt = *(uint32_t*)(b + p + 4), k;
        int is_font = !memcmp(b + p, "FONT", 4), is_gffi = !memcmp(b + p, "GFFI", 4);
        p += 8;
        if (cnt & 0x80000000u) {
            /* 区切り付き（2 作目）: 位置の表は GFFI の別の塊にある。{総数, GFFI の番号, 区切りの数, {最初の番号, 数}...} */
            uint32_t nseg = *(uint32_t*)(b + p + 8), s, idx = 0;
            if (is_font) {
                seg_gid = *(uint32_t*)(b + p + 4);
                for (s = 0; s < nseg; s++) {
                    uint32_t first = *(uint32_t*)(b + p + 12 + s * 8), num = *(uint32_t*)(b + p + 16 + s * 8);
                    if (100 >= first && 100 < first + num) { seg_idx = idx + (100 - first); seg_font = 1; }
                    idx += num;
                }
                seg_total = idx;
            }
            p += 12 + nseg * 8;
            continue;
        }
        if (is_gffi) { gffi_p = p; gffi_cnt = cnt; }
        for (k = 0; k < cnt; k++, p += 12)
            if (is_font && *(uint32_t*)(b + p) == 100) { entry = p + 4; floc = *(uint32_t*)(b + p + 4); fl = *(uint32_t*)(b + p + 8); }
    }
    if (seg_font && gffi_p) {
        uint32_t k;
        for (k = 0; k < gffi_cnt; k++) {
            uint32_t q = gffi_p + k * 12;
            if (*(uint32_t*)(b + q) == seg_gid) {
                uint32_t tl = *(uint32_t*)(b + q + 4), tn = *(uint32_t*)(b + q + 8);
                if (tn < 8 * seg_total || tl + tn > n) break;
                entry = tl + tn - 8 * seg_total + 8 * seg_idx;
                floc = *(uint32_t*)(b + entry); fl = *(uint32_t*)(b + entry + 4);
                break;
            }
        }
    }
    if (!entry || floc + fl > n || fl < 8 + 256 + 512) return NULL;
    {
        const uint32_t SLOT = 2 + 8 * 9;
        uint32_t nl = fl + 127 * SLOT + SCRATCH_LEN, c;   /* 字の枠の後ろに、長い訳を置く作業用の場所を足す */
        uint8_t *f;
        nb = (uint8_t*)malloc(n + nl);
        if (!nb) return NULL;
        memcpy(nb, b, n);
        f = nb + n;
        memcpy(f, b + floc, fl);
        memset(f + fl, 0, nl - fl);
        if (f[2] != 9) { free(nb); return NULL; }           /* 高さ 9 のフォントのはず */
        f[4] = 'J'; f[5] = 'P';
        for (c = 0x80; c <= 0xfe; c++) {
            uint32_t o = fl + (c - 0x80) * SLOT;
            *(uint16_t*)(f + 8 + 256 + c * 2) = (uint16_t)o;
            f[o] = 8; f[o + 1] = 0;
        }
        memcpy(f + fl + 127 * SLOT, "SCRA", 4);          /* 作業用の場所の目印 */
        *(uint32_t*)(nb + entry) = n;
        *(uint32_t*)(nb + entry + 4) = nl;
        *outn = n + nl;
    }
    return nb;
}

typedef FILE *(__cdecl *fopen_t)(const char *, const char *);
static fopen_t real_fopen;
static CRITICAL_SECTION g_cs;
static int g_exe_ok;

static const char *patched_path(const char *orig_path, char *buf)
{
    const char *bn = base_name(orig_path);
    int is_exe = !_stricmp(bn, "DSUN.EXE"), is_res = !_stricmp(bn, "RESOURCE.GFF");
    uint8_t *orig, *out;
    uint32_t olen, nlen;
    if (!is_exe && !is_res) return NULL;
    if (is_res && !g_exe_ok) return NULL;           /* 対応版の DSUN.EXE を差し替えたときだけ */
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) {
        if (is_exe) {
            WIN32_FILE_ATTRIBUTE_DATA fa;
            if (!(GetFileAttributesExA(orig_path, GetFileExInfoStandard, &fa) && fa.nFileSizeLow == *(const uint32_t*)g_epatch)) return NULL;
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
                if (_strnicmp(fd.cFileName, cur, 9)) { wsprintfA(old, "%s\\%s", g_dir, fd.cFileName); DeleteFileA(old); }
            } while (FindNextFileA(fh, &fd));
            FindClose(fh);
        }
    }
    orig = read_all(orig_path, &olen);
    if (!orig) return NULL;
    out = is_exe ? build_exe(orig, olen, &nlen) : build_gff(orig, olen, &nlen);
    if (!out) { free(orig); dbg("could not build %s\n", bn); return NULL; }
    if (!write_all(buf, out, nlen)) { if (out != orig) free(out); free(orig); return NULL; }
    if (out != orig) free(out);
    free(orig);
    if (is_exe) g_exe_ok = 1;
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
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    wsprintfA(g_dir, "%s\\DarkSun2_JP", la);
    CreateDirectoryA(g_dir, NULL);
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen || !hook_iat()) return;
    g_enabled = 1;
    dbg("Dark Sun 2 JP patch active, blob %08X\n", g_blob_crc);
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
