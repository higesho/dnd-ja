/*
 * The Dark Queen of Krynn 日本語化パッチ（実行時部分）
 * DOSBox 0.74-3（Krynn Series 同梱版）に winmm.dll の代理として読み込まれる。
 * DQK.EXE（RTLink のオーバーレイつき）は書き換えず、ゲームがファイルを開くたびにメモリ上のコードを探してフックを入れる:
 *  - 位置指定の printf（書式文字列）: 書式文字列が訳の表にあれば差し替える（メニューのボタンは頭にキーの英字を付ける）
 *  - 範囲指定の折り返し表示（イベント文）: 文字列が表にあれば差し替える
 *  - 折り返しの区切り文字の判定: 0x80 以上（日本語の字）も区切りとして扱う（1 字ごとに折り返せるように）
 *  - 1 字描画の「字 & 0x7f」: 0x80 以上の番号はその字形をフォントの 0x7f の位置に書いて描かせる
 * 訳文は VGA（モード 13h）の画面外 A000:FA00〜FFFF に置く。
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "resource.h"

void proxy_init(void);
#ifdef DS_TEST
void testio_init(void);   /* テスト用（DS_TEST=1 のときだけ）: SDL_PushEvent でキーを入れる */
#endif

static char g_dir[MAX_PATH];
#ifndef DS_TEST
/* 配布版: ログは書かない（ログ・描画の記録・画面の撮影・キーの差し込みなどの試験用の仕組みは、build.bat test の試験版にだけ入る） */
#define dbg(...) ((void)0)
#define log_miss(tag, s, n) ((void)0)
#else
static FILE *g_log;
static int g_logon;
static volatile int g_trace;   /* テスト用: %LOCALAPPDATA%\UA_JP\trace.txt があるあいだ 1 字描画を記録する */
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

void ime_init(void);      /* ime.c: 日本語の入力欄 */
#ifdef DS_TEST
void ua_log(const char *fmt, ...)
{
    char b[2400];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(b, fmt, ap);
    va_end(ap);
    dbg("%s", b);
}
#endif

static uint32_t g_dbx, g_dbx_code;   /* DOSBox build: Steam 0.74-3 = 0, GOG 0.74-2.1 = +0xe000 / -0x10 */
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
    /* Steam (DOSBox 0.74-3) and GOG (DOSBox 0.74-2.1) builds: same code, globals shifted by g_dbx, IllegalHandler by g_dbx_code */
    static const struct { uint32_t s1, s2, s3, dbx, dcode; } builds[] = {
        {0x415e66u, 0x41897fu, 0x4351ccu, 0u, 0u},
        {0x415e56u, 0x41896fu, 0x4351bcu, 0xe000u, 0xfffffff0u},
        {0x416a26u, 0x41965fu, 0x431da2u, 0x3b000u, 0xbc0u},      /* Steam の UA に付いている SVN 版 */
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
static __inline void ww(uint32_t a, uint16_t v) { wb(a, (uint8_t)v); wb(a + 1, (uint8_t)(v >> 8)); }
#define REG32(i) (((uint32_t*)A_REGS)[i])
#define R_SP ((uint16_t)REG32(4))
#define R_BP ((uint16_t)REG32(5))
#define R_BX ((uint16_t)REG32(3))
#define R_SI ((uint16_t)REG32(6))
#define R_DI ((uint16_t)REG32(7))
#define SET_AL(v) (*(uint8_t*)&REG32(0) = (uint8_t)(v))
#define SET_SP(v) (*(uint16_t*)&REG32(4) = (uint16_t)(v))
#define SET_BP(v) (*(uint16_t*)&REG32(5) = (uint16_t)(v))
#define PHYS_ES (((uint32_t*)A_SEGPHYS)[0])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
static void push16(uint16_t v) { SET_SP(R_SP - 2); ww(PHYS_SS + R_SP, v); }

/* ------------------------------------------------------- パッチデータ */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)
static const uint8_t *g_blob;
static uint32_t g_blob_len;
#define IS_LEAD(c) ((c) >= 0x81 && (c) <= 0xfe)
#define IS_TRAIL(c) ((c) >= 0x80 && (c) <= 0xfe)
static const uint8_t *g_pats;      /* 'N' 節: 仲間の名前が入る文の型 */
static uint32_t g_npats;
static uint16_t *g_glyph_index;
static const uint8_t *g_glyphs;
static const uint8_t *g_gflags;
static int g_cb_base = 0x70;
static int g_enabled;
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
    if (memcmp(h->magic, "DQJ1", 4)) return 0;
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
        } else if (s->type == 'N') {
            g_npats = *(const uint32_t*)d;
            g_pats = d + 4;
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0];
#ifdef DS_TEST
            g_logon = (int)v[1];
#endif
        }
        p = d + s->len;
    }
    return g_glyph_index != NULL;
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

/* ------------------------------------------------ 日本語の字の動的な番号 */
static uint16_t g_dyn_code[128];
static uint32_t g_dyn_stamp[128];
static uint32_t g_clock = 1;
static int dyn_get(uint16_t code)
{
    int i, best = 0;
    for (i = 0; i < 128; i++)
        if (g_dyn_code[i] == code) { g_dyn_stamp[i] = g_clock++; return 0x80 + i; }
    for (i = 1; i < 128; i++)
        if (g_dyn_stamp[i] < g_dyn_stamp[best]) best = i;
    g_dyn_code[best] = code;
    g_dyn_stamp[best] = g_clock++;
    return 0x80 + best;
}
static int glyph_of(uint16_t code) { return g_glyph_index ? g_glyph_index[code] : 0; }

/* 独自符号の訳文 → ゲーム用（日本語 1 字 = 1 バイト）。0 終端を含む長さを返す */
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

#include "uajp.h"
/* デザインの文章に入っている日本語の符号（uajp.h: 記号 2〜3 個で 1 字）を内部符号に直す。日本語があれば長さ、なければ 0 */
static int ua_decode(const uint8_t *s, int n, uint8_t *out, int max)
{
    int i = 0, o = 0, found = 0;
    while (i < n && o < max - 2) {
        int l, t1, t2;
        uint16_t code;
        if ((l = uajp_lead2(s[i])) >= 0 && i + 1 < n && (t1 = uajp_tval(s[i + 1])) >= 0) {
            code = uajp_plane2(l * UAJP_NT + t1);
            if (code && glyph_of(code)) { out[o++] = (uint8_t)(code >> 8); out[o++] = (uint8_t)code; i += 2; found = 1; continue; }
        }
        if ((l = uajp_lead3(s[i])) >= 0 && i + 2 < n && (t1 = uajp_tval(s[i + 1])) >= 0 && (t2 = uajp_tval(s[i + 2])) >= 0) {
            code = uajp_plane3(l * UAJP_NT * UAJP_NT + t1 * UAJP_NT + t2);
            if (code && glyph_of(code)) { out[o++] = (uint8_t)(code >> 8); out[o++] = (uint8_t)code; i += 3; found = 1; continue; }
        }
        out[o++] = s[i++];
    }
    return found ? o : 0;
}
/* ゲーム用（日本語 1 字 = 1 バイト）まで直す。末尾の 0 込みの長さ。日本語がなければ 0 */
static int ua_to_game(const uint8_t *s, int n, uint8_t *gs, int max)
{
    static uint8_t cj[1100];
    int cn = (n > 1 && n <= 1090) ? ua_decode(s, n, cj, 1100) : 0;
    return cn > 0 ? to_game(cj, cn, gs, max) : 0;
}

/* ------------------------------------------------ 見つけたゲームの場所 */
static uint32_t g_ask;
static uint32_t g_fmt, g_wrap, g_punct, g_glyph, g_menu_ret;
/* 訳文は元の文字列の場所に書く（日本語は 1 字 1 バイトなので、ほとんどの場合は英文より短い）。
 * 元の英文は控えておき、同じフックが次に呼ばれたときに書き戻す（定数の文字列を何度でも訳し直せるように） */
typedef struct { uint32_t addr; int n, wn; uint8_t save[1100], wr[1100]; } restore_t;
static restore_t g_rs_fmt, g_rs_wrap, g_rs_arg[4];
static void restore(restore_t *r)
{
    int k;
    if (!r->n) return;
    for (k = 0; k < r->wn; k++) if (rb(r->addr + k) != r->wr[k]) break;   /* ゲームが書き換えた後なら戻さない */
    if (k == r->wn) for (k = 0; k < r->n; k++) wb(r->addr + k, r->save[k]);
    r->n = 0;
}
static int put_inplace(restore_t *r, uint32_t addr, const uint8_t *en, int n, const uint8_t *gs, int m, int over)
{
    int k;
    if (m > n + 1 && !over) return 0;       /* 入らない: 英語のまま */
    if (m > n + 1) { for (k = 0; k < m && k < 1000; k++) r->save[k] = rb(addr + k); r->n = m; }
    else { memcpy(r->save, en, n); r->save[n] = 0; r->n = n + 1; }
    r->addr = addr;
    for (k = 0; k < m; k++) wb(addr + k, gs[k]);
    memcpy(r->wr, gs, m); r->wn = m;
    return 1;
}

static int read_cstr(uint32_t a, uint8_t *buf, int max)
{
    int n = 0;
    while (n < max) { uint8_t c = rb(a + n); if (!c) break; buf[n++] = c; }
    return n;
}

#ifdef DS_TEST
#define NSEEN 8192
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
#endif

/* 語を並べた名前（"Long Sword +2 " のような品物の名前）: 表にある語句（長いものから 4 語まで）と数だけで
 * できていれば、訳をつないだ文字列を作る（UTF-8 ではなく表の符号のまま）。できなければ 0 */
static int compose(const uint8_t *en, int n, uint8_t *out, int max)
{
    int st[16], ln[16], nt = 0, i = 0, o = 0, k, hits = 0, col = 0;
    while (i < n && nt < 16) {
        while (i < n && en[i] == ' ') i++;
        if (i >= n) break;
        st[nt] = i;
        while (i < n && en[i] != ' ') i++;
        ln[nt] = i - st[nt]; nt++;
    }
    if (nt < 2 || i < n) return 0;
    for (k = 0; k < nt; ) {
        int w, ok = 0;
        const uint8_t *t = en + st[k];
        if (k > 0 && st[k] - (st[k - 1] + ln[k - 1]) >= 2) {      /* 桁揃えの空白: 次の語を元と同じ桁に置く */
            int pad = st[k] - col;
            if (pad < 1) pad = 1;
            while (pad-- > 0 && o < max - 1) { out[o++] = ' '; col++; }
        }
        int isnum = ln[k] > 0;
        for (w = 0; w < ln[k]; w++) if (!((t[w] >= '0' && t[w] <= '9') || ((t[w] == '+' || t[w] == '-') && w == 0))) isnum = 0;
        if (isnum) { if (o + ln[k] >= max) return 0; memcpy(out + o, t, ln[k]); o += ln[k]; col += ln[k]; k++; continue; }
        for (w = (nt - k < 4 ? nt - k : 4); w >= 1 && !ok; w--) {
            int len = st[k + w - 1] + ln[k + w - 1] - st[k];
            const tentry *e = lookup(t, len);
            if (e && e->jalen) {
                if (o + e->jalen >= max) return 0;
                memcpy(out + o, e->ja, e->jalen); o += e->jalen; k += w; ok = 1; hits++;
                for (len = 0; len < e->jalen; len++) { col++; if (IS_LEAD(e->ja[len]) && len + 1 < e->jalen && IS_TRAIL(e->ja[len + 1])) len++; }
            }
        }
        if (!ok) return 0;
    }
    if (!hits) return 0;
    if (n > 0 && en[n - 1] == ' ' && o < max) out[o++] = ' ';
    return o;
}

/* 1 行をタブで欄に分け、欄ごとに（前後の空白を除いて）表の訳か compose で訳す。どの欄も変わらなければ 0 */
static int tr_line(const uint8_t *en, int n, uint8_t *out, int max)
{
    int i = 0, o = 0, changed = 0;
    for (i = 0; i < n; i++) if (en[i] >= 0x80) return 0;   /* 訳済みの字（ゲームの番号）が混じる: tr_mixed に任せる */
    i = 0;
    while (i <= n) {
        int j = i, a, b;
        while (j < n && en[j] != '\t') j++;
        a = i; b = j;
        while (a < b && en[a] == ' ') a++;
        while (b > a && en[b - 1] == ' ') b--;
        if (o + (a - i) >= max) return 0;
        memcpy(out + o, en + i, a - i); o += a - i;
        if (b > a) {
            const tentry *e = lookup(en + a, b - a);
            int cn;
            if (e && e->jalen) {
                if (o + e->jalen >= max) return 0;
                memcpy(out + o, e->ja, e->jalen); o += e->jalen; changed = 1;
            } else if ((cn = compose(en + a, b - a, out + o, max - o)) > 0) { o += cn; changed = 1; }
            else { if (o + (b - a) >= max) return 0; memcpy(out + o, en + a, b - a); o += b - a; }
        }
        if (o + (j - b) + 1 >= max) return 0;
        memcpy(out + o, en + b, j - b); o += j - b;
        if (j < n) out[o++] = '\t';
        i = j + 1;
    }
    return changed ? o : 0;
}

/* 組み立て済みの文（"呪文:Lightning Bolt"、"…は Long Sword で…" のように日本語と英語が混じる）の中の英語の部分を訳す。
 * 区切り = ゲームの字の番号（0x80 以上: 訳済みの日本語）、タブ、':'。区切りの間の英語（前後の空白を除く）を
 * 表の訳か compose で置き換える。出力はゲームの字（to_game 済み、末尾 0 込みの長さ）。変わらなければ 0 */
static int tr_mixed(const uint8_t *en, int n, uint8_t *gs, int max)
{
    static uint8_t cj[520];
    int i = 0, o = 0, changed = 0;
    while (i < n) {
        int j = i, a, b, letters = 0, k;
        if (en[i] >= 0x80 || en[i] == '\t' || en[i] == ':') { if (o >= max - 1) return 0; gs[o++] = en[i++]; continue; }
        while (j < n && en[j] < 0x80 && en[j] != '\t' && en[j] != ':') j++;
        a = i; b = j;
        while (a < b && en[a] == ' ') a++;
        while (b > a && en[b - 1] == ' ') b--;
        for (k = a; k < b; k++) if ((en[k] | 0x20) >= 'a' && (en[k] | 0x20) <= 'z') letters++;
        if (o + (a - i) >= max - 1) return 0;
        memcpy(gs + o, en + i, a - i); o += a - i;
        if (b > a) {
            const tentry *e = letters >= 2 ? lookup(en + a, b - a) : 0;
            int cn = 0, m;
            if (e && e->jalen) { memcpy(cj, e->ja, e->jalen); cn = e->jalen; }   /* ボタンの語の印があっても、組み立て済みの文の中なら訳す */
            else if (letters >= 2 && !e) cn = compose(en + a, b - a, cj, 500);
            if (cn > 0) {
                m = to_game(cj, cn, gs + o, max - o);      /* 末尾の 0 込み */
                o += m - 1; changed = 1;
            } else { if (o + (b - a) >= max - 1) return 0; memcpy(gs + o, en + a, b - a); o += b - a; }
        }
        if (o + (j - b) >= max - 1) return 0;
        memcpy(gs + o, en + b, j - b); o += j - b;
        i = j;
    }
    if (!changed) return 0;
    gs[o++] = 0;
    return o;
}
static int has_mixed(const uint8_t *s, int n)
{
    int k, hi = 0, lo = 0;
    for (k = 0; k < n; k++) { if (s[k] >= 0x80 || s[k] == ':') hi = 1; else if ((s[k] | 0x20) >= 'a' && (s[k] | 0x20) <= 'z') lo = 1; }
    return hi && lo;
}

/* 日本語入力（ime.c）が送ったキーの進み具合を知るためのもの */
static volatile LONG g_ed_acks;                       /* 入力欄のカーソルが描かれた回数 */
unsigned ua_ed_acks(void) { return (unsigned)g_ed_acks; }
int ua_kbd_empty(void) { return rw(0x41a) == rw(0x41c); }   /* BIOS のキーバッファが空か（40:1A = 先頭, 40:1C = 末尾） */
static uint32_t g_ed_S;                               /* 最後にカーソルを描いた入力欄の構造体（先頭が文章、+ebh がカーソルの位置） */
uint32_t ua_ed_struct(void) { return g_ed_S; }
static int glyph_of(uint16_t code);
/* カーソルの前（where = 0）／カーソルの位置（where = 1）の 1 字が記号いくつ分か（1〜3）。わからなければ 0。
 * Backspace や矢印キーを日本語 1 字ぶん動かすために、日本語入力（ime.c）が使う */
int ua_ed_charlen(int where)
{
    uint32_t S = g_ed_S;
    int p, i = 0, n;
    if (!S) return 0;
    p = rb(S + 0xeb);
    while (i < 236) {
        uint8_t c = rb(S + i);
        int l, t1, t2;
        uint16_t code;
        if (!c) break;
        n = 1;
        if ((l = uajp_lead2(c)) >= 0 && (t1 = uajp_tval(rb(S + i + 1))) >= 0 &&
            (code = uajp_plane2(l * UAJP_NT + t1)) != 0 && glyph_of(code)) n = 2;
        else if ((l = uajp_lead3(c)) >= 0 && (t1 = uajp_tval(rb(S + i + 1))) >= 0 && (t2 = uajp_tval(rb(S + i + 2))) >= 0 &&
                 (code = uajp_plane3(l * UAJP_NT * UAJP_NT + t1 * UAJP_NT + t2)) != 0 && glyph_of(code)) n = 3;
        if (where == 1 && i == p) return n;
        if (where == 0 && i + n == p) return n;
        if (i >= p) break;
        i += n;
    }
    return 1;
}

/* --- エディタの入力欄（文章を打つ欄）
 * エディタは編集中の文章を 1 行ずつ（打っている最中は変わった桁から行末までを）、その文字列そのものを書式にして表示する。
 * 日本語 1 字は記号 2〜3 個なので、そのまま描くと 1 字が 2〜3 マスを使って字の間が空く。編集中の文章は書き換えられない
 * ので、表示の入口（cb_fmt）で次のようにする:
 *   - 行の中の字を数え直し、「何番目の字か」の桁へ描く位置を左へずらす（位置の変数 DS:d588 を直す）
 *   - 1 字描画（cb_glyph）が順に使う「マスごとの中身」を作る: 先に字を詰めて並べ、余ったマスは空ける
 *   - 行末の空白埋め（"%*s"）も、詰めた後の行末から埋めるように桁と数を直す
 *   - カーソル（"%c" で 1 字を反転して描く）も、詰めた後の桁に、その位置の字で描く
 * 入力欄の構造体: 先頭が文章そのもの、+ebh カーソルの位置、+ech+2L 行 L の始まり、+edh+2L 行 L の終わり。 */
static struct { int active, n, i; uint8_t ch[80]; uint16_t code[80]; } g_ov;   /* code: 0 そのまま, 2 空き, 80h 未満 = その英数字, ほか = 日本語の字 */
typedef struct { int n; uint16_t done[48]; int cell[49]; } edline;           /* done[i]: 行の i 番目の記号で完成する字（0 = なし） */
static void ed_line(uint32_t S, int L, edline *e)
{
    int ls = rb(S + 0xec + 2 * L), le = rb(S + 0xed + 2 * L), n = le - ls, i, k, l, t;
    int pn = 0, need = 0, lead = 0, t1 = 0, pstart = 0;      /* 途中まで読んだ字（pstart = この行の何番目から始まったか） */
    if (n < 0) n = 0;
    if (n > 40) n = 40;
    e->n = n;
    if (ls >= 1) {      /* 前の行の終わりで途切れた字: 手前の 2 字でわかる（先頭の記号は後ろの字に使わないため） */
        uint8_t p1 = rb(S + ls - 1), p2 = ls >= 2 ? rb(S + ls - 2) : 0;
        if ((l = uajp_lead3(p2)) >= 0 && (t = uajp_tval(p1)) >= 0) { pn = 2; need = 3; lead = l; t1 = t; }
        else if ((l = uajp_lead2(p1)) >= 0) { pn = 1; need = 2; lead = l; }
        else if ((l = uajp_lead3(p1)) >= 0) { pn = 1; need = 3; lead = l; }
    }
    for (i = 0; i < n; i++) {
        uint8_t c = rb(S + ls + i);
        e->done[i] = 0;
        if (pn && (t = uajp_tval(c)) >= 0) {
            if (pn + 1 == need) {
                uint16_t code = need == 2 ? uajp_plane2(lead * UAJP_NT + t) : uajp_plane3(lead * UAJP_NT * UAJP_NT + t1 * UAJP_NT + t);
                pn = 0;
                if (code && glyph_of(code)) e->done[i] = code;
                else for (k = pstart; k <= i; k++) e->done[k] = rb(S + ls + k);      /* 字にならない並び: 記号のまま見せる */
            } else { t1 = t; pn = 2; }
            continue;
        }
        if (pn) { for (k = pstart; k < i; k++) e->done[k] = rb(S + ls + k); pn = 0; }   /* 途切れた: 記号のまま見せる */
        if ((l = uajp_lead2(c)) >= 0) { pn = 1; need = 2; lead = l; pstart = i; }
        else if ((l = uajp_lead3(c)) >= 0) { pn = 1; need = 3; lead = l; pstart = i; }
        else e->done[i] = c ? c : ' ';
    }
    e->cell[0] = 0;
    for (i = 0; i < n; i++) e->cell[i + 1] = e->cell[i] + (e->done[i] != 0);
}
/* 表示の入口から見て、呼び出し元がエディタの入力欄のどこか（lcall の直後の命令の並びで見分ける）。
 * 1 = 行の文字を描く, 2 = 行末を空白で埋める, 3 = カーソルを描く, 0 = ほか。*pg = 位置指定の表示の枠, *pe = エディタ側の枠 */
static int editor_site(uint16_t *pg, uint16_t *pe)
{
    static const uint8_t sa[] = {0x83,0xc4,0x0a,0x8b,0x46,0xf8,0xd1,0xe0,0xc4,0x5e,0xfc,0x03,0xd8,0x26,0x8a,0x87,0xec,0x00};
    static const uint8_t sb[] = {0x83,0xc4,0x10,0x8b,0x46,0xf8,0xd1,0xe0,0xc4,0x5e,0xfc,0x03,0xd8,0x26,0x80,0xbf,0xed,0x00,0x00};
    static const uint8_t sc[] = {0x83,0xc4,0x0c,0x8b,0xe5,0x5d,0xcb,0x55,0x8b,0xec,0x83,0xec,0x08,0xc4,0x5e,0x0e};
    uint32_t ss = PHYS_SS, ret;
    uint16_t f = R_BP;      /* 書式の関数を呼んだ関数の枠 → その上 → 位置指定の表示の枠 */
    int k, site = 0;
    if (!f || !(f = rw(ss + f)) || !(f = rw(ss + f))) return 0;
    ret = ((uint32_t)rw(ss + f + 4) << 4) + rw(ss + f + 2);
    if (ret < 0x1000 || ret > 0x9f000 || rb(ret - 5) != 0x9a) return 0;
    for (k = 0; k < (int)sizeof sa && rb(ret + k) == sa[k]; k++) {}
    if (k == (int)sizeof sa) site = 1;
    if (!site) { for (k = 0; k < (int)sizeof sb && rb(ret + k) == sb[k]; k++) {} if (k == (int)sizeof sb) site = 2; }
    if (!site) { for (k = 0; k < (int)sizeof sc && rb(ret + k) == sc[k]; k++) {} if (k == (int)sizeof sc) site = 3; }
    *pg = f; *pe = rw(ss + f);
    return site;
}
/* 入力欄の表示を詰める。site = editor_site の返り値。src/en/n = 書式文字列（行の文字を描くとき、その行の残り） */
static void editor_compact(int site, uint16_t bg, uint16_t be, const uint8_t *en, int n)
{
    static edline e;
    uint32_t ss = PHYS_SS, S = ((uint32_t)rw(ss + be - 2) << 4) + rw(ss + be - 4);
    uint32_t xvar = PHYS_DS + 0xd588;                 /* 描く位置（横、ピクセル） */
    int L, col, dc, i, k;
    if (site == 1 || site == 2) {
        L = (int16_t)rw(ss + be - 8);
        if (L < 0 || L > 5) return;
        ed_line(S, L, &e);
    }
    if (site == 1) {              /* 行の col 桁目から行末まで */
        col = rb(ss + be + 0x0a);
        if (col > e.n || n > 80) return;
        dc = e.cell[col];
        memcpy(g_ov.ch, en, n);
        for (i = col, k = 0; i < e.n && k < n; i++) if (e.done[i]) g_ov.code[k++] = e.done[i];
        for (; k < n; k++) g_ov.code[k] = 2;
        g_ov.n = n; g_ov.i = 0; g_ov.active = 1;
        /* エディタは「行の区切りの位置が変わらなければ下の行は描き直さない」。空白のない日本語の文は決まった桁で
         * 折り返されるので、1 字足しても区切りの位置が変わらず、下の行が古いまま残る。描き直す行数（+f9h）を
         * 最後の行までに直す（行を描くたびに読み直されている） */
        {   /* 文章のある最後の行まで（その下の空の行は描き直さない: 1 字ごとに全部の行を描くと遅くなり、キーを取りこぼす） */
            int last = 0;
            for (i = 0; i < 6; i++) if (rb(S + 0xed + 2 * i)) last = i + 1;
            if (rb(S + 0xf9) < last) wb(S + 0xf9, (uint8_t)last);
        }
#ifdef DS_TEST
        if (g_trace) dbg("ED1 L%d col%d n%d len%d dc%d K%d p1=%02x\n", L, col, n, e.n, dc, e.cell[e.n], rb(S + rb(S + 0xec + 2 * L) - 1));
#endif
        ww(xvar, (uint16_t)(rw(xvar) - 8 * (col - dc)));
    } else if (site == 2) {       /* 行末の空白埋め: 詰めた後の行末から 38 桁目まで */
        int K = e.cell[e.n];
        uint32_t ap = ((uint32_t)rw(ss + R_SP + 10) << 4) + rw(ss + R_SP + 8);     /* "%*s" の数 */
        ww(ap, (uint16_t)(0x26 - K));
        ww(xvar, (uint16_t)(rw(xvar) - 8 * (e.n - K)));
    } else if (site == 3) {       /* カーソル: 行は枠の [bp-6]、桁は [bp+0ah] */
        uint16_t d = ' ';
        g_ed_acks++;              /* 入力欄がキーを 1 つ処理するたびにカーソルを描き直す（日本語入力がこれを見て次のキーを送る） */
        g_ed_S = S;
        L = (int16_t)rw(ss + be - 6);
        if (L < 0 || L > 5) return;
        ed_line(S, L, &e);
        col = rb(ss + be + 0x0a);
        if (col > e.n) return;
        dc = e.cell[col];
        for (i = col; i < e.n; i++) if (e.done[i]) { d = e.done[i]; break; }
        g_ov.ch[0] = rb(ss + bg + 0x10); g_ov.code[0] = d;
        g_ov.n = 1; g_ov.i = 0; g_ov.active = 1;
        ww(xvar, (uint16_t)(rw(xvar) - 8 * (col - dc)));
    }
}

/* --- 位置指定 printf の入口（push bp / mov bp,sp / sub sp,1c の代わり）。[sp+4] = 書式文字列 far ptr */
static uint32_t __cdecl cb_fmt(void)
{
    uint32_t ss = PHYS_SS;
    uint16_t sp = R_SP;
    uint32_t src = ((uint32_t)rw(ss + sp + 6) << 4) + rw(ss + sp + 4);
    static uint8_t en[520], gs[520];
    int n;
    const tentry *e;
    int to_buf;
    restore(&g_rs_fmt);
    g_ov.active = 0;
    {   /* エディタの入力欄: 文章は書き換えず、描く位置とマスの中身だけ直す */
        uint16_t bg, be;
        int site = editor_site(&bg, &be);
        if (site) {
            n = read_cstr(src, en, 100);
            editor_compact(site, bg, be, en, n);
            push16(R_BP); SET_BP(R_SP); SET_SP(R_SP - 0x1c);
            return 0;
        }
    }
    {   /* 出力先: putch が「バッファに 1 字書く」関数（55 8b ec 8a 46 06 c4 1e .. .. 26 88 07）なら sprintf 相当 */
        uint32_t pc = ((uint32_t)rw(ss + sp + 0x0e) << 4) + rw(ss + sp + 0x0c);
        static const uint8_t bufput[] = {0x55,0x8b,0xec,0x8a,0x46,0x06,0xc4,0x1e};
        int k;
        to_buf = 1;
        for (k = 0; k < 8; k++) if (rb(pc + k) != bufput[k]) to_buf = 0;
        if (to_buf && (rb(pc + 10) != 0x26 || rb(pc + 11) != 0x88 || rb(pc + 12) != 0x07)) to_buf = 0;
    }
    {   /* 書式の %s に渡された文字列（"%s" でメニューの問いを出すなど）も訳す */
        int k, ai = 0, please = 0, nsarg = 0;
        uint32_t ap = ((uint32_t)rw(ss + sp + 10) << 4) + rw(ss + sp + 8);   /* [sp+8] = 引数の並びへの far ptr */
        uint8_t f[80];
        int fn = read_cstr(src, f, 79);
        for (k = 0; k < 4; k++) restore(&g_rs_arg[k]);
        /* "%s~%s" はボタンの並び（"PALACE~TEMPLE~HALL…"）を作る書式。ここで訳すと頭の英字がなくなりボタンが出ない
         * （v1.3 まで: パランサスの「どこへ行くか？」で行き先が出なかった）。ボタンは描くときに訳す */
        if (to_buf && memchr(f, '~', fn)) fn = 0;
        for (k = 0; k < fn && ai < 4; k++) {     /* 組み立て途中（sprintf）では、表にそのまま一致する部品だけ訳す（ボタンの語は除く） */
            if (f[k] != '%') continue;
            k++;
            while (k < fn && strchr("-+ #0123456789.", f[k])) k++;
            if (k < fn && f[k] == 'l') { k++; ap += 4; continue; }
            if (k >= fn || f[k] == '%') continue;
            if (f[k] == 's') {
                uint32_t a = ((uint32_t)rw(ap + 2) << 4) + rw(ap);
                int an = read_cstr(a, en, 500);
                {   /* 見出し "Please Select a/an ～:" は語（Please / Select / a …）を %s で並べて作る。日本語では
                     * 「～を選択:」にする（書式の訳がそうなっている）ので、前に並ぶ語は消す */
                    static const char *const drop[] = {"Please", "Select", "a", "an", "any", "this", "A", "An", 0};
                    if (nsarg++ == 0) please = (an == 6 && !memcmp(en, "Please", 6));
                    if (please) {
                        int d, hit = 0;
                        for (d = 0; drop[d]; d++) if ((int)strlen(drop[d]) == an && !memcmp(en, drop[d], an)) hit = 1;
                        if (hit) { gs[0] = 0; if (put_inplace(&g_rs_arg[ai], a, en, an, gs, 1, 0)) ai++; ap += 4; continue; }
                    }
                }
                if (!to_buf) {      /* デザインの文章に入っている日本語 */
                    int jm = ua_to_game(en, an, gs, 500);
                    if (jm > 0) { if (put_inplace(&g_rs_arg[ai], a, en, an, gs, jm, 0)) ai++; ap += 4; continue; }
                }
                const tentry *ae = an > 1 ? lookup(en, an) : 0;
                static uint8_t cj[520];
                int cn = 0;
                if (ae && ae->jalen && !(to_buf && (ae->flags & 4))) { memcpy(cj, ae->ja, ae->jalen); cn = ae->jalen; }
                else if (an > 1 && !to_buf && !ae) cn = tr_line(en, an, cj, 500);
                if (cn > 0) {
                    int m = to_game(cj, cn, gs, 500);
                    if (!put_inplace(&g_rs_arg[ai], a, en, an, gs, m, 0)) log_miss("S>", en, an); else log_miss("S+", en, an);
                    ai++;
                } else if (!ae && an > 1 && !to_buf && has_mixed(en, an)) {
                    int m = tr_mixed(en, an, gs, 500);
                    if (m > 0) { if (put_inplace(&g_rs_arg[ai], a, en, an, gs, m, 0)) ai++; else log_miss("S>", en, an); }
                    else log_miss("S", en, an);
                } else if (!ae && an > 1 && !to_buf) log_miss("S", en, an);
                ap += 4;
            } else if (strchr("dicuxX", f[k])) ap += 2;
            else break;
        }
    }
    n = read_cstr(src, en, 500);
    if (!to_buf) {      /* デザインの文章に入っている日本語 */
        int jm = ua_to_game(en, n, gs, 500);
        if (jm > 0) { put_inplace(&g_rs_fmt, src, en, n, gs, jm, 0); n = 0; }
    }
    e = lookup(en, n);
    static int after_heading;
    int cp_now = 0;   /* UA: コピープロテクトの答えの表示はまだない */
    int is_heading = after_heading && cp_now && n > 0 && !memchr(en, '%', n);
    if (is_heading || !cp_now) after_heading = 0;
    if (cp_now && n == 19 && !memcmp(en, " after the heading ", 19)) after_heading = 1;
    if (is_heading && n > 0 && n < 40) {     /* 見出しの後ろに答えを付ける */
        uint8_t ans[24], q[120];
        int al = read_cstr(PHYS_DS + 0xc6f9, ans, 20), k, ok = al > 0, qn = 0, m;
        for (k = 0; k < al; k++) if (!(ans[k] >= 'A' && ans[k] <= 'Z') && !(ans[k] >= 'a' && ans[k] <= 'z') && ans[k] != '-' && ans[k] != '\'') ok = 0;
        if (ok) {
            const tentry *a = lookup((const uint8_t*)"@ANSWER", 7);
            memcpy(q, en, n); qn = n; q[qn++] = ' ';
            if (a) {
                for (k = 0; k < a->jalen && qn < 100; k++) {
                    if (a->ja[k] == '%' && k + 1 < a->jalen && a->ja[k + 1] == 's') { memcpy(q + qn, ans, al); qn += al; k++; }
                    else q[qn++] = a->ja[k];
                }
            } else { q[qn++] = '('; memcpy(q + qn, ans, al); qn += al; q[qn++] = ')'; }
            m = to_game(q, qn, gs, 110);
            put_inplace(&g_rs_fmt, src, en, n, gs, m, 1);
            dbg("copy protection answer %.*s on [%.*s] at %x\n", al, ans, n, en, src);
            push16(R_BP); SET_BP(R_SP); SET_SP(R_SP - 0x1c);
            return 0;
        }
    }
    if (e && e->jalen) {
        int menu = 0, m, k;
        {   /* メニューのボタン（ラベルの後でその頭の英字を重ねて描く）から呼ばれたか: 呼び出しの鎖を 3 段たどる */
            uint16_t f = R_BP;
            int lv;
            for (lv = 0; lv < 4 && f; lv++) {
                uint32_t r = ((uint32_t)rw(ss + f + 4) << 4) + rw(ss + f + 2);
                if (r == g_menu_ret) {
                    /* ボタンを描く関数の枠: [bp+6] = ボタンの構造体。+1ch の旗が (旗 & 41h) == 0 かつ (旗 & 20h) != 0 のときだけ、
                     * ゲームは頭の英字を重ねて描く（設定欄の値のように、キーのない枠には英字を付けない） */
                    uint16_t bbp = rw(ss + f);
                    uint32_t bs = ((uint32_t)rw(ss + bbp + 8) << 4) + rw(ss + bbp + 6);
                    uint8_t fl = rb(bs + 0x1c);
                    menu = !(fl & 0x41) && (fl & 0x20);
                    break;
                }
                f = rw(ss + f);
            }
        }
        m = 0;
        if (menu && en[0] >= 'A' && en[0] <= 'Z' && e->ja[0] != en[0]) gs[m++] = en[0];
        m += to_game(e->ja, e->jalen, gs + m, 500 - m);
        (void)k;
        if (!put_inplace(&g_rs_fmt, src, en, n, gs, m, 0)) log_miss("F>", en, n);
    } else if (!e && n > 1) {
        static uint8_t cj[520];
        int cn = tr_line(en, n, cj, 500), m;
        if (cn > 0) {
            m = to_game(cj, cn, gs, 500);
            if (!put_inplace(&g_rs_fmt, src, en, n, gs, m, 0)) log_miss("F>", en, n);
        } else if (has_mixed(en, n) && (m = tr_mixed(en, n, gs, 500)) > 0) {
            if (!put_inplace(&g_rs_fmt, src, en, n, gs, m, 0)) log_miss("F>", en, n);
        } else log_miss("F", en, n);
    }
    push16(R_BP);
    SET_BP(R_SP);
    SET_SP(R_SP - 0x1c);
    return 0;
}

/* --- 問いとボタンのメニューの入口（push bp / mov bp,sp / sub sp,52 の代わり）。[sp+4] = 問いの文字列 far ptr */
static restore_t g_rs_ask;
static uint32_t __cdecl cb_ask(void)
{
    uint32_t ss = PHYS_SS;
    uint16_t sp = R_SP;
    uint32_t src = ((uint32_t)rw(ss + sp + 6) << 4) + rw(ss + sp + 4);
    static uint8_t en[520], gs[520];
    int n;
    const tentry *e;
    restore(&g_rs_ask);
    n = read_cstr(src, en, 500);
    e = n > 1 ? lookup(en, n) : 0;
    if (e && e->jalen) {
        int m = to_game(e->ja, e->jalen, gs, 500);
        if (!put_inplace(&g_rs_ask, src, en, n, gs, m, 0)) log_miss("A>", en, n);
    } else if (!e && n > 1) log_miss("A", en, n);
    push16(R_BP);
    SET_BP(R_SP);
    SET_SP(R_SP - 0x52);
    return 0;
}

/* --- 折り返し表示の入口（push bp / mov bp,sp / sub sp,2 の代わり）。[sp+12] = 文字列 far ptr */
/* ^（仲間の名前に置き換わる記号）を含む文: 名前が入った形（"… HANDS ELOWYN A SMALL STONE …"）を、名前の前後の部分で
 * 照合する。'N' 節の項目 = u16 前の長さ, u16 後ろの長さ, u64 前のハッシュ, u64 後ろのハッシュ, u16 訳の長さ, 訳（^ が名前の位置）。
 * 合えばゲーム用の訳（末尾 0 込みの長さ）、合わなければ 0 */
static int match_name_pat(const uint8_t *en, int n, uint8_t *gs, int max)
{
    static uint8_t cj[1100];
    const uint8_t *p = g_pats;
    uint32_t i;
    while (n > 0 && en[n - 1] == ' ') n--;
    for (i = 0; i < g_npats; i++) {
        int pl = *(const uint16_t*)p, sl = *(const uint16_t*)(p + 2), jl = *(const uint16_t*)(p + 20), k, o = 0, nl;
        uint64_t hp = *(const uint64_t*)(p + 4), hs = *(const uint64_t*)(p + 12);
        const uint8_t *ja = p + 22;
        p += 22 + jl;
        nl = n - pl - sl;
        if (nl < 1 || nl > 20) continue;                 /* 名前は 15 字まで */
        if (fnv64(en, pl) != hp || fnv64(en + n - sl, sl) != hs) continue;
        for (k = 0; k < jl && o < 1000; k++) {
            if (ja[k] == '^') { memcpy(cj + o, en + pl, nl); o += nl; }
            else cj[o++] = ja[k];
        }
        return to_game(cj, o, gs, max);
    }
    return 0;
}

static uint32_t __cdecl cb_wrap(void)
{
    uint32_t ss = PHYS_SS;
    uint16_t sp = R_SP;
    uint32_t src = ((uint32_t)rw(ss + sp + 0x14) << 4) + rw(ss + sp + 0x12);
    static uint8_t en[1100], gs[1030];
    int n;
    const tentry *e;
    restore(&g_rs_wrap);
    n = read_cstr(src, en, 1090);
    {   /* デザインの文章に入っている日本語 */
        int jm = ua_to_game(en, n, gs, 1020);
        if (jm > 0) { put_inplace(&g_rs_wrap, src, en, n, gs, jm, 0); dbg("WJ %d -> %d\n", n, jm); n = 0; }
    }
    e = lookup(en, n);
    if (!e) {      /* 後ろの空白の有無だけが違う形 */
        int n2 = n;
        while (n2 > 0 && en[n2 - 1] == ' ') n2--;
        if (n2 != n && n2 > 1) e = lookup(en, n2);
    }
    if (e && e->jalen) {
        int m = to_game(e->ja, e->jalen, gs, 1020);
        int over = 0;
        if (!put_inplace(&g_rs_wrap, src, en, n, gs, m, over)) log_miss("W>", en, n);
    } else if (!e && n > 1) {
        int m = match_name_pat(en, n, gs, 1020);
        if (m <= 0) m = has_mixed(en, n) ? tr_mixed(en, n, gs, 1020) : 0;
        if (m <= 0 && n < 60) {      /* 品物の名前（"Long Sword +4 " のように語を並べたもの）: 語ごとの訳をつなぐ */
            static uint8_t cj[520];
            int cn = tr_line(en, n, cj, 500);
            if (cn > 0) m = to_game(cj, cn, gs, 1020);
        }
        if (m > 0) { if (!put_inplace(&g_rs_wrap, src, en, n, gs, m, 0)) log_miss("W>", en, n); }
        else log_miss("W", en, n);
    }
    push16(R_BP);
    SET_BP(R_SP);
    SET_SP(R_SP - 2);
    return 0;
}

/* --- 区切り文字の判定の結果（mov al,es:[bx] / pop bp の代わり）。[bp+6] = 字
 * 折り返し表示は 1 語ずつ進む（si = 見ている位置, di = 語の頭, 文字列は呼び出し元の [bp+14h]）:
 *   (1) +c3: 語の頭の区切り文字を飛ばす  (2) +db: 区切り文字か空白まで進む  (3) +fc: 続く区切り文字を語に入れる
 *   そのあと、次が空白でなければ 1 字戻る → 語は di から si まで。
 * 日本語（0x80 以上）は 1 字で 1 語にしたい: (1) では区切りでない、(2) では区切り（そこで止まる）、
 * (3) では「語の頭の 1 字」だけ区切り扱いにして語に入れる。行頭に来てほしくない字（、。」など）と、
 * 行末に残したくない字（「 など）の次の字は、前の語に続ける。 */
static int jp_flags(uint8_t c)
{
    uint16_t code = c >= 0x80 ? g_dyn_code[c - 0x80] : 0;
    int g = code ? glyph_of(code) : 0;
    return g ? g_gflags[g - 1] : 0;
}
static uint32_t __cdecl cb_punct(void)
{
    uint32_t ss = PHYS_SS;
    uint8_t c = rb(ss + R_BP + 6);
    if (c >= 0x80) {
        uint32_t ret = ((uint32_t)rw(ss + R_BP + 4) << 4) + rw(ss + R_BP + 2);
        int v = 0;
        if (ret == g_wrap + 0xdb) v = 1;
        else if (ret == g_wrap + 0xfc) {
            uint16_t wbp = rw(ss + R_BP), si = R_SI, di = R_DI;
            uint32_t s = ((uint32_t)rw(ss + wbp + 0x16) << 4) + rw(ss + wbp + 0x14);
            if (si == di) v = 1;                                             /* 語の頭の 1 字 */
            else if (jp_flags(c) & 1) v = 1;                                 /* 行頭に来てほしくない字 */
            else if (si > 0 && (jp_flags(rb(s + si - 1)) & 2)) v = 1;        /* 前の字が行末に残したくない字 */
        }
        SET_AL(v);
    } else SET_AL(rb(PHYS_ES + R_BX));
    SET_BP(rw(PHYS_SS + R_SP));
    SET_SP(R_SP + 2);
    return 0;
}

/* 字形（8x8）をフォントの 0x7f の位置に書く。VGA の描画は 12 行のうち 0,1,3,4,6,8,9,10 行目を拾って 8 行で描くので、
 * その行に美咲フォントの 8 行を置く */
static void put_glyph(const uint8_t *bits)
{
    static const int rows[12] = {0, 1, 1, 2, 3, 3, 4, 4, 5, 6, 7, -1};
    uint32_t slot = PHYS_DS + 0x97e0 + (0x7f - 0x20) * 12;
    int k;
    for (k = 0; k < 12; k++) wb(slot + k, rows[k] < 0 ? 0 : bits[rows[k]]);
}

/* --- 1 字描画の and byte [bp+a],7f の代わり */
static uint32_t __cdecl cb_glyph(void)
{
    uint32_t a = PHYS_SS + R_BP + 0x0a;
    uint8_t c = rb(a);
#ifdef DS_TEST
    if (g_logon) {      /* テスト用のビルドだけ: trace.txt があるかを 16 回に 1 回見る */
        static int cnt;
        if (!(cnt++ & 15)) {
            char tp[MAX_PATH];
            wsprintfA(tp, "%s%ctrace.txt", g_dir, 92);
            g_trace = GetFileAttributesA(tp) != INVALID_FILE_ATTRIBUTES;
        }
    }
    if (g_trace) {      /* テスト用: だれが 1 字描画を呼んだか */
        uint32_t ss = PHYS_SS;
        uint16_t f = R_BP;
        char buf[200];
        int o = wsprintfA(buf, "G %02x x=%d y=%d", c, (int16_t)rw(ss + f + 6), (int16_t)rw(ss + f + 8)), lv;
        for (lv = 0; lv < 6 && f; lv++) {
            o += wsprintfA(buf + o, " %04x:%04x", rw(ss + f + 4), rw(ss + f + 2));
            if (rw(ss + f) <= f) break;
            f = rw(ss + f);
        }
        dbg("%s\n", buf);
    }
#endif
    if (g_ov.active) {      /* エディタの入力欄の 1 行を描いている途中（cb_fmt の ov_build が作った「マスごとの中身」を順に使う） */
        static const uint8_t blank[8] = {0};
        int i = g_ov.i++;
        if (i >= g_ov.n || g_ov.ch[i] != c) g_ov.active = 0;      /* 並びが合わない: やめる */
        else if (g_ov.code[i]) {
            uint16_t code = g_ov.code[i];
            if (code >= 0x20 && code < 0x80) { wb(a, (uint8_t)code); return 0; }      /* 英数字はそのまま（位置だけ詰まる） */
            {
                int g = code > 0x80 ? glyph_of(code) : 0;
                put_glyph(g ? g_glyphs + (g - 1) * 8 : blank);
            }
            wb(a, 0x7f);
            return 0;
        }
    }
    if (c >= 0x80) {
        uint16_t code = g_dyn_code[c - 0x80];
        if (code) {
            static const uint8_t tofu[8] = {0x00,0x7e,0x42,0x42,0x42,0x42,0x7e,0x00};
            int g = glyph_of(code);
            g_dyn_stamp[c - 0x80] = g_clock++;
            put_glyph(g ? g_glyphs + (g - 1) * 8 : tofu);
            wb(a, 0x7f);
            return 0;
        }
    }
    wb(a, (uint8_t)(c & 0x7f));
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    if (g_cb_installed) return 1;
    for (i = 0; i < 5; i++) if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot busy\n"); return 0; }
    tab[b + 0] = (uint32_t)cb_fmt;
    tab[b + 1] = (uint32_t)cb_wrap;
    tab[b + 2] = (uint32_t)cb_punct;
    tab[b + 3] = (uint32_t)cb_glyph;
    tab[b + 4] = (uint32_t)cb_ask;
    g_cb_installed = 1;
    return 1;
}

static const uint8_t SIG_FMT[] = {0x55,0x8b,0xec,0x83,0xec,0x1c,0x56,0x57,0xc4,0x5e,0x0a,0x8c,0x46,0xf2,0x89,0x5e,0xf0,0xc7,0x46,0xf6,0x00,0x00,0xe9};
static const uint8_t SIG_WRAP[] = {0x55,0x8b,0xec,0x83,0xec,0x02,0x56,0x57,0x9a};                 /* +0x0d: SIG_WRAP2 */
static const uint8_t SIG_WRAP2[] = {0x80,0x7e,0x10,0x00,0x75,0x04,0xc6,0x46,0x10,0x08,0x80,0x7e,0x06,0x27};
static const uint8_t SIG_PUNCT[] = {0x55,0x8b,0xec,0xff,0x76,0x06,0x1e,0xb8};                       /* +0x16: 26 8a 07 5d cb */
static const uint8_t SIG_GLYPH[] = {0x80,0x66,0x0a,0x7f,0x80,0x7e,0x0a,0x20,0x7d,0x0f,0xa1};
static const uint8_t SIG_ASK[] = {0x55,0x8b,0xec,0x83,0xec,0x52,0xff,0x36};                       /* +0x0a: ff 36, +0x0e: ff 76 0c ff 76 0a 9a */
static const uint8_t SIG_ASK2[] = {0xff,0x76,0x0c,0xff,0x76,0x0a,0x9a};
static const uint8_t SIG_MENU[] = {0x26,0xff,0x77,0x0e,0x26,0xff,0x77,0x0c,0x57,0xff,0x76,0xfa,0x8b,0x46,0xf8,0x40,0x50,0x9a};
#define MEM_LO 0x1000u
#define MEM_HI 0x9f000u
static uint32_t find(const uint8_t *mb, const uint8_t *sig, int n, uint32_t from)
{
    uint32_t a;
    for (a = from; a + n < MEM_HI; a++) {
        const uint8_t *q = (const uint8_t*)memchr(mb + a, sig[0], MEM_HI - a);
        if (!q) return 0;
        a = (uint32_t)(q - mb);
        if (a + n < MEM_HI && !memcmp(q, sig, n)) return a;
    }
    return 0;
}
static void put_cb(uint32_t p, int k, int pad)
{
    int i;
    wb(p, 0xfe); wb(p + 1, 0x38); wb(p + 2, (uint8_t)(g_cb_base + k)); wb(p + 3, (uint8_t)((g_cb_base + k) >> 8));
    for (i = 0; i < pad; i++) wb(p + 4 + i, 0x90);
}
static void try_patch(void)
{
    const uint8_t *mb = ((uint8_t**)A_TLB_READ)[0x20];
    uint32_t p, q;
    if (!mb) return;
    if (g_fmt && mb[g_fmt] == 0xfe && mb[g_fmt + 1] == 0x38) return;
    p = find(mb, SIG_FMT, sizeof SIG_FMT, MEM_LO);
    if (!p) return;
    if (!install_callbacks()) return;
    g_fmt = p; put_cb(p, 0, 2);
    for (q = MEM_LO; (q = find(mb, SIG_WRAP, sizeof SIG_WRAP, q)) != 0; q++)
        if (!memcmp(mb + q + 0x0d, SIG_WRAP2, sizeof SIG_WRAP2)) { g_wrap = q; put_cb(q, 1, 2); break; }
    for (q = MEM_LO; (q = find(mb, SIG_PUNCT, sizeof SIG_PUNCT, q)) != 0; q++)
        if (mb[q + 0x16] == 0x26 && mb[q + 0x17] == 0x8a && mb[q + 0x18] == 0x07 && mb[q + 0x19] == 0x5d && mb[q + 0x1a] == 0xcb &&
            q + 0x1b == g_wrap) { g_punct = q + 0x16; put_cb(g_punct, 2, 0); break; }
    q = find(mb, SIG_GLYPH, sizeof SIG_GLYPH, MEM_LO);
    if (q) { g_glyph = q; put_cb(q, 3, 0); }
    for (q = MEM_LO; (q = find(mb, SIG_ASK, sizeof SIG_ASK, q)) != 0; q++)
        if (mb[q + 0x0a] == 0xff && mb[q + 0x0b] == 0x36 && !memcmp(mb + q + 0x0e, SIG_ASK2, sizeof SIG_ASK2)) { g_ask = q; put_cb(q, 4, 2); break; }
    q = find(mb, SIG_MENU, sizeof SIG_MENU, MEM_LO);
    if (q) g_menu_ret = q + sizeof SIG_MENU + 4;
    dbg("patched: fmt %x wrap %x punct %x glyph %x menu %x ask %x\n", g_fmt, g_wrap, g_punct, g_glyph, g_menu_ret, g_ask);
}

typedef FILE *(__cdecl *fopen_t)(const char *, const char *);
static fopen_t real_fopen;
static CRITICAL_SECTION g_cs;
static FILE *__cdecl hook_fopen(const char *name, const char *mode)
{
    if (g_enabled) { EnterCriticalSection(&g_cs); try_patch(); LeaveCriticalSection(&g_cs); }
#ifdef DS_TEST
    if (g_logon && name && strstr(name, ".DSN")) dbg("fopen %s %s\n", name, mode);      /* テスト用: デザインのどのファイルを開いたか */
#endif
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
#ifdef DS_TEST
    wsprintfA(g_dir, "%s\\UA_JP", la);
    CreateDirectoryA(g_dir, NULL);      /* ログ（log.txt）と trace.txt の置き場。試験版だけ */
#endif
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen || !hook_iat()) return;
    g_enabled = 1;
    dbg("Unlimited Adventures JP patch active\n");
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); proxy_init(); init(h);
#ifdef DS_TEST
        testio_init();
#endif
        if (g_enabled) ime_init(); }
    return TRUE;
}
