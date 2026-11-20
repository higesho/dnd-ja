/*
 * Hillsfar 日本語化パッチ（実行時部分）
 * DOSBox 0.74-3（Forgotten Realms The Archives 同梱版）に winmm.dll の代理として読み込まれる。
 * MAIN.EXE は SEA-AXE で圧縮されているので、ファイルは書き換えず、展開後のメモリを書き換える:
 *  - ゲームがデータファイルを開くたび（fopen）に、展開済みのコードを探してフックを入れる
 *  - 文章の表示関数（2 字を 1 バイトに詰めた Westwood 式の文字列を復号しながら単語単位で折り返す）の入口で、
 *    英文を復号し、改行・制御記号で区切った断片ごとに訳へ差し替えた文字列を、DOS から借りた領域に置いて渡す。
 *    日本語の字は 0x80-0xff の 1 バイト（動的に割り当てる番号）。訳した文字列のときは 2 字詰めの復号を止める
 *  - 文字列描画の「字 & 0x7f」を「& 0xff」にし、1 字描画の入口で 0x80 以上の番号の字形をフォントの 0 番に書いてから描かせる
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "resource.h"

void proxy_init(void);

/* ---------------------------------------------------------------- ログ */
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

/* ------------------------------------------- DOSBox 0.74-3 の内部アドレス */
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
#define R_AX ((uint16_t)REG32(0))
#define R_SP ((uint16_t)REG32(4))
#define R_BP ((uint16_t)REG32(5))
#define R_DI ((uint16_t)REG32(7))
#define SET_AX(v) (*(uint16_t*)&REG32(0) = (uint16_t)(v))
#define SET_SP(v) (*(uint16_t*)&REG32(4) = (uint16_t)(v))
#define SET_BP(v) (*(uint16_t*)&REG32(5) = (uint16_t)(v))
#define SEGVAL(i) (((uint16_t*)A_SEGVAL)[i])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])

static void push16(uint16_t v)
{
    SET_SP(R_SP - 2);
    ww(PHYS_SS + R_SP, v);
}

/* ------------------------------------------------------- パッチデータ */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)

static const uint8_t *g_blob;
static uint32_t g_blob_len;

#define IS_LEAD(c) ((c) >= 0x81 && (c) <= 0xfe)
#define IS_TRAIL(c) ((c) >= 0x80 && (c) <= 0xfe)
static uint16_t *g_glyph_index;  /* 65536 -> グリフ番号+1 */
static const uint8_t *g_glyphs;  /* 8バイト/字（行ごと, bit7=左） */
static const uint8_t *g_gflags;  /* 1=行頭禁則 2=行末禁則 */
static int g_cb_base = 0x70;
static char gp_dirname[32] = "Hillsfar_JP";
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
    if (memcmp(h->magic, "HFJ1", 4)) return 0;
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
        } else if (s->type == 'T') {
            load_table(d);
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0];
            g_logon = (int)v[1];
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
static uint16_t g_dyn_code[128];   /* 番号 -> 独自符号（0=空き） */
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
static int gflags(uint16_t code) { int g = glyph_of(code); return g ? g_gflags[g - 1] : 0; }

/* ------------------------------------------------ 見つけたゲームの場所 */
static uint32_t g_dec;        /* 文章表示関数の先頭（線形） */
static uint32_t g_dec_imm;    /* test byte [bp-6],80 の 80 */
static uint32_t g_str_imm;    /* 文字列描画の and ax,7f の 7f */
static uint32_t g_blit;       /* 1 字描画の先頭 */
static uint32_t g_str;        /* 文字列描画の先頭 */
static uint32_t g_font;       /* フォント（線形） */
static uint32_t g_buf;        /* 訳文を置く領域（線形, 4KB） */
#define BUFSZ 3072         /* 文章表示用。残り 1KB は文字列描画用 */
static int g_patched;

#define ARGC(c) ((c) == 2 || (c) == 6 || (c) == 9 || (c) == 0x13)

/* 英文の復号: far ptr の圧縮文字列 → 平文（制御記号と引数はそのまま） */
static int decode_en(uint32_t src, uint8_t *out, int max, int *rawlen)
{
    uint32_t ds = PHYS_DS;
    int o = 0, i = 0;
    for (;;) {
        uint8_t c = rb(src + i);
        if (c == 0 || o + 2 >= max || i > 4000) break;
        i++;
        if (c & 0x80) {
            out[o++] = rb(ds + 0xaaa4 + ((c & 0x7f) >> 3));
            out[o++] = rb(ds + 0xaab4 + (c & 0x7f));
        } else if (ARGC(c)) {
            out[o++] = c;
            out[o++] = rb(src + i);
            i++;
        } else out[o++] = c;
    }
    out[o] = 0;
    if (rawlen) *rawlen = i;
    return o;
}

/* 訳: 改行・制御記号で区切った断片ごとに表を引く。訳があれば out（独自符号の列）に置いて 1 を返す */
static int has_ja(const uint8_t *s, int n) { int k; for (k = 0; k < n; k++) if (s[k] >= 0x80) return 1; return 0; }

static int g_width = 38;          /* いまの文章欄の幅（文章表示の入口で読む） */
static int translate(const uint8_t *en, int n, sbuf *out)
{
    int i = 0, any = 0, last9 = -1;
    while (i < n) {
        int j = i, a, b, lead, trail;
        const tentry *e;
        uint8_t c = en[i];
        if (c == 0x0d || c == 0x10) { sb_c(out, c); i++; last9 = -1; continue; }
        if (ARGC(c)) { sb_c(out, c); last9 = c == 9 ? out->n : -1; if (i + 1 < n) sb_c(out, en[i + 1]); i += 2; continue; }
        while (j < n && en[j] != 0x0d && en[j] != 0x10 && !ARGC(en[j])) j++;
        a = i; b = j;
        while (a < b && en[a] == ' ') a++;
        while (b > a && en[b - 1] == ' ') b--;
        lead = a - i; trail = j - b;
        e = lookup(en + a, b - a);
        if (e && e->jalen && !(e->jalen == 1 && e->ja[0] == '-')) {
            int ja = has_ja(e->ja, e->jalen), cells = 0, k;
            /* 英語の訳、または同じ文字列の中で制御記号をはさんで前後に続きがあるときは、元の空白を残す */
            int keepl = !ja || (lead && i > 0 && en[i - 1] != 0x0d);
            int keept = !ja || (trail == 1 && j < n && en[j] != 0x0d);
            if (keepl) { int q; for (q = 0; q < lead; q++) sb_c(out, ' '); }
            sb_put(out, e->ja, e->jalen);
            for (k = 0; k < e->jalen; k++) { if (IS_LEAD(e->ja[k]) && k + 1 < e->jalen) k++; cells++; }
            if (keept) { int q; for (q = 0; q < trail; q++) sb_c(out, ' '); }
            else if (trail >= 2) { int q; for (q = cells; q < j - i; q++) sb_c(out, ' '); }   /* 桁そろえの空白 */
            if (ja && last9 >= 0 && (j >= n || en[j] == 0x0d) &&     /* 桁指定の直後の 1 行で、その桁が中央寄せの位置（見出し）: 中央をそろえ直す */
                abs((int)out->p[last9] - (g_width - (j - i)) / 2) <= 1) {
                int nc = out->p[last9] + ((j - i) - cells) / 2;
                if (nc >= 0 && nc < 60) out->p[last9] = (uint8_t)nc;
            }
            any = 1;
        } else if (e) {
            any = 1;                     /* 訳が空: 断片ごと消す */
        } else {
            sb_put(out, en + i, j - i);
        }
        last9 = -1;
        i = j;
    }
    return any;
}

/* 独自符号の列 → ゲーム用（日本語 1 字 = 1 バイト）。日本語の続く所は、行の残りの桁に収まるよう改行を入れる */
static int to_game_wrap(const uint8_t *s, int n, uint8_t *out, int max, int col, int width)
{
    int i = 0, o = 0;
    int lim = width - 1;
    while (i < n && o < max - 2) {
        uint8_t c = s[i];
        if (IS_LEAD(c) && i + 1 < n && IS_TRAIL(s[i + 1])) {
            uint16_t code = (uint16_t)((c << 8) | s[i + 1]);
            if (col >= lim - 1) {        /* 行末: 句読点などの行頭禁則の字は 1 字だけはみ出して置く */
                if (!(gflags(code) & 1) || col >= lim) { out[o++] = 0x0d; col = 0; }
            }
            out[o++] = (uint8_t)dyn_get(code);
            col++;
            i += 2;
            continue;
        }
        if (c == 0x0d) { out[o++] = c; col = 0; i++; continue; }
        if (c == 9 || c == 0x13) { out[o++] = c; if (i + 1 < n) { out[o++] = s[i + 1]; col = s[i + 1]; } i += 2; continue; }
        if (ARGC(c)) { out[o++] = c; if (i + 1 < n) out[o++] = s[i + 1]; i += 2; continue; }
        if (c == 0x10) { out[o++] = c; i++; continue; }
        out[o++] = c;
        col++;
        i++;
    }
    out[o++] = 0;
    return o;
}

/* 記録用: 一度見た英文は書かない */
#define NSEEN 8192
static uint64_t g_seen[NSEEN];
static void log_en2(const char *tag, const uint8_t *s, int n, int tr);
static void log_en(const uint8_t *s, int n, int tr) { log_en2(tr ? "TR" : "EN", s, n, tr); }
static void log_en2(const char *tag, const uint8_t *s, int n, int tr)
{
    uint64_t h = fnv64(s, n);
    uint32_t i = (uint32_t)h & (NSEEN - 1);
    char buf[4096];
    int k, o = 0;
    if (!g_logon) return;
    while (g_seen[i]) { if (g_seen[i] == h) return; i = (i + 1) & (NSEEN - 1); }
    g_seen[i] = h;
    for (k = 0; k < n && o < 4000; k++) {
        uint8_t c = s[k];
        if (c == 0x0d) { buf[o++] = '\\'; buf[o++] = 'r'; }
        else if (c < 0x20 || c >= 0x7f) o += wsprintfA(buf + o, "{%02x}", c);
        else buf[o++] = (char)c;
    }
    buf[o] = 0;
    (void)tr;
    dbg("%s %s\n", tag, buf);
}

/* --- 文章表示関数の入口（push bp / mov bp,sp / mov ax,66 の代わり） */
static uint32_t __cdecl cb_dec(void)
{
    uint32_t ss = PHYS_SS, ds = PHYS_DS;
    uint16_t sp = R_SP;
    uint16_t off = rw(ss + sp + 4), seg = rw(ss + sp + 6);
    static uint8_t en[4200], gs[BUFSZ];
    int n, tr = 0;
    n = decode_en(((uint32_t)seg << 4) + off, en, sizeof en - 1, NULL);
    {   /* コピープロテクト（翻訳ホイール）: 入力欄の上の空き行に答えを黄色で出す。出題番号は呼び出し元の [bp-6] */
        const char *key = " Input code word:";
        uint8_t *p = (uint8_t*)strstr((const char*)en, key);
        if (p && p > en && p[-1] == 0x0d && n + 40 < (int)sizeof en) {
            uint8_t idx = rb(ss + R_BP - 6), ins[40];
            int k, m = 0, at = (int)(p - en) - 1;
            if (idx < 16) {
                ins[m++] = 6; ins[m++] = 0x0e;
                memcpy(ins + m, "Answer:", 7); m += 7;
                ins[m++] = 6; ins[m++] = 0x0f;
                ins[m++] = ' ';
                for (k = 0; k < 6; k++) ins[m++] = (uint8_t)(((rb(ds + 0xebe + idx * 6 + k) + 0x11) & 0xff) ^ 0xe9);
                memmove(en + at + m, en + at, n - at + 1);
                memcpy(en + at, ins, m);
                n += m;
            }
        }
    }
    if (g_ntr && g_buf) {
        sbuf t = {0};
        {
            int w = (int16_t)rw(((uint32_t)rw(ds + 0xb0aa) << 4) + 0x37a);
            g_width = (w >= 8 && w <= 80) ? w : 38;
        }
        if (translate(en, n, &t)) {
            uint16_t es1 = rw(ds + 0xb0aa), es2 = rw(ds + 0xb0b4);
            int width = (int16_t)rw(((uint32_t)es1 << 4) + 0x37a);
            int col = (int16_t)rw(((uint32_t)es2 << 4) + 0x29e);
            int m, k;
            if (width < 8 || width > 80) width = 38;
            if (col < 0 || col >= width) col = 0;
            m = to_game_wrap(t.p, t.n, gs, BUFSZ, col, width);
            for (k = 0; k < m; k++) wb(g_buf + k, gs[k]);
            ww(ss + sp + 4, 0);
            ww(ss + sp + 6, (uint16_t)(g_buf >> 4));
            tr = 1;
        }
        free(t.p);
    }
    wb(g_dec_imm, tr ? 0x00 : 0x80);
    log_en(en, n, tr);
    /* push bp / mov bp,sp / mov ax,66 */
    push16(R_BP);
    SET_BP(R_SP);
    SET_AX(0x66);
    return 0;
}

/* --- 1 字描画の入口（push bp / mov bp,sp / push di の代わり）。引数 [sp+4] = 字 * 8 */
static uint32_t __cdecl cb_blit(void)
{
    uint32_t ss = PHYS_SS;
    uint16_t sp = R_SP;
    uint16_t fo = rw(ss + sp + 4);
    if (fo >= 0x400 && fo < 0x800) {
        int idx = (fo >> 3) - 0x80, k;
        uint16_t code = g_dyn_code[idx];
        if (code) {
            static const uint8_t tofu[8] = {0x00,0x7e,0x42,0x42,0x42,0x42,0x7e,0x00};
            int g = glyph_of(code);
            const uint8_t *bits = g ? g_glyphs + (g - 1) * 8 : tofu;
            g_dyn_stamp[idx] = g_clock++;
            for (k = 0; k < 8; k++) wb(g_font + k, bits[k]);
            fo = 0;
        } else fo &= 0x3ff;
        ww(ss + sp + 4, fo);
    }
    push16(R_BP);
    SET_BP(R_SP);
    push16(R_DI);
    return 0;
}

/* --- 文字列描画（平文）の入口（push bp / mov bp,sp / mov ax,0e の代わり）。文章表示から来たものはそのまま */
static uint32_t __cdecl cb_str(void)
{
    uint32_t ss = PHYS_SS;
    uint16_t sp = R_SP;
    uint32_t ret = ((uint32_t)rw(ss + sp + 2) << 4) + rw(ss + sp);
    if (ret != g_dec + 0x7c0 && ret != g_dec + 0x831) {   /* 文章表示の 1 行出力・選択中の行の描き直しは訳済み */
        uint16_t off = rw(ss + sp + 4), seg = rw(ss + sp + 6);
        uint32_t src = ((uint32_t)seg << 4) + off;
        static uint8_t en[400], gs[1024];
        int n = 0, tr = 0;
        while (n < 380) { uint8_t c = rb(src + n); if (!c) break; en[n++] = c; }
        en[n] = 0;
        if (g_ntr && g_buf && n) {
            sbuf t = {0};
            if (translate(en, n, &t)) {
                int m = to_game_wrap(t.p, t.n, gs, sizeof gs, 0, 200), k;
                for (k = 0; k < m; k++) wb(g_buf + BUFSZ + k, gs[k]);
                ww(ss + sp + 4, (uint16_t)BUFSZ);
                ww(ss + sp + 6, (uint16_t)(g_buf >> 4));
                tr = 1;
            }
            free(t.p);
        }
        if (g_logon) {
            char tag[32];
            wsprintfA(tag, "S%05x", ret);
            log_en2(tag, en, n, tr);
        }
    }
    push16(R_BP);
    SET_BP(R_SP);
    SET_AX(0x0e);
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
    tab[b + 0] = (uint32_t)cb_dec;
    tab[b + 1] = (uint32_t)cb_blit;
    tab[b + 2] = (uint32_t)cb_str;
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

/* ----------------------------------------------------- 展開後のコードを探して書き換える */
static const uint8_t SIG_DEC[] = {0x55,0x8b,0xec,0xb8,0x66,0x00,0x9a};
static const uint8_t SIG_DEC2[] = {0xf6,0x46,0xfa,0x80,0x74,0x37,0x8a,0x46,0xfa,0x24,0x07};       /* +0x6f */
static const uint8_t SIG_STR[] = {0x26,0x8a,0x07,0x25,0x7f,0x00,0xb1,0x03,0xd3,0xe0,0x89,0x46,0xf4};
static const uint8_t SIG_BLIT[] = {0x55,0x8b,0xec,0x57,0x56,0x1e,0xb8};
static const uint8_t SIG_BLIT2[] = {0xb9,0x08,0x00,0xb0,0x08,0xee,0x42,0x8a,0x84};                /* +0x39 */
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

/* DOS のメモリ管理ブロックの鎖をたどり、空きブロックの末尾 4KB（+MCB）を切り出して DOS の持ち物にする */
static uint32_t reserve_dos(const uint8_t *mb)
{
    uint32_t s0, s, best = 0;
    for (s0 = 0x40; s0 < 0x400; s0++) {
        int cnt = 0, ok = 0;
        s = s0;
        while (cnt < 200 && s < 0xa000) {
            uint8_t t = mb[s * 16];
            uint16_t size = *(const uint16_t*)(mb + s * 16 + 3);
            if (t != 'M' && t != 'Z') break;
            cnt++;
            if (t == 'Z') { ok = cnt >= 3; break; }
            s += size + 1u;
        }
        if (ok) break;
    }
    if (s0 >= 0x400) { dbg("MCB chain not found\n"); return 0; }
    /* 既に切り出した印（持ち主 8, 大きさ 0x100, 名前 HFJP）があればそれを使う */
    s = s0;
    for (;;) {
        uint8_t t = mb[s * 16];
        uint16_t own = *(const uint16_t*)(mb + s * 16 + 1), size = *(const uint16_t*)(mb + s * 16 + 3);
        if (own == 8 && size == 0x100 && !memcmp(mb + s * 16 + 8, "HFJP", 4)) return (s + 1) * 16u;
        if (own == 0 && size >= 0x180) best = s;
        if (t == 'Z') break;
        s += size + 1u;
    }
    if (!best) { dbg("no free DOS block\n"); return 0; }
    {
        uint32_t a = best * 16;
        uint8_t t = mb[a];
        uint16_t size = *(const uint16_t*)(mb + a + 3);
        uint16_t nsize = (uint16_t)(size - 0x101);
        uint32_t ns = best + 1 + nsize, k;
        uint8_t mcb[16] = {0};
        mcb[0] = t; mcb[1] = 8; mcb[2] = 0; mcb[3] = 0x00; mcb[4] = 0x01;
        memcpy(mcb + 8, "HFJP", 4);
        for (k = 0; k < 16; k++) wb(ns * 16 + k, mcb[k]);
        wb(a, 'M');
        ww(a + 3, nsize);
        dbg("reserved DOS memory at %x\n", (ns + 1) * 16);
        return (ns + 1) * 16u;
    }
}

static void try_patch(void)
{
    const uint8_t *mb = ((uint8_t**)A_TLB_READ)[0x20];
    uint32_t d, s, b, b2;
    if (!mb) return;
    if (g_patched && mb[g_dec] == 0xfe && mb[g_dec + 1] == 0x38) return;   /* 入れたまま */
    g_patched = 0;
    d = find(mb, SIG_DEC, sizeof SIG_DEC, MEM_LO);
    if (!d || memcmp(mb + d + 0x6f, SIG_DEC2, sizeof SIG_DEC2)) return;
    s = find(mb, SIG_STR, sizeof SIG_STR, MEM_LO);
    b2 = find(mb, SIG_BLIT2, sizeof SIG_BLIT2, MEM_LO);
    if (!s || !b2) return;
    b = b2 - 0x39;
    if (memcmp(mb + b, SIG_BLIT, sizeof SIG_BLIT)) return;
    if (!install_callbacks()) return;
    if (memcmp(mb + s - 0x56, SIG_DEC, 3) || mb[s - 0x56 + 3] != 0xb8 || mb[s - 0x56 + 4] != 0x0e) return;
    g_dec = d; g_dec_imm = d + 0x72; g_str_imm = s + 4; g_blit = b; g_str = s - 0x56;
    g_font = ((uint32_t)*(const uint16_t*)(mb + b + 7) << 4) + *(const uint16_t*)(mb + b + 0x42);
    if (!g_buf) g_buf = reserve_dos(mb);
    else {   /* 前のゲームで切り出した領域が残っているか */
        g_buf = reserve_dos(mb);
    }
    /* 書き換え */
    wb(d + 0, 0xfe); wb(d + 1, 0x38); wb(d + 2, (uint8_t)g_cb_base); wb(d + 3, (uint8_t)(g_cb_base >> 8));
    wb(d + 4, 0x90); wb(d + 5, 0x90);
    wb(g_str_imm, 0xff);
    wb(g_str + 0, 0xfe); wb(g_str + 1, 0x38); wb(g_str + 2, (uint8_t)(g_cb_base + 2)); wb(g_str + 3, (uint8_t)((g_cb_base + 2) >> 8));
    wb(g_str + 4, 0x90); wb(g_str + 5, 0x90);
    wb(b + 0, 0xfe); wb(b + 1, 0x38); wb(b + 2, (uint8_t)(g_cb_base + 1)); wb(b + 3, (uint8_t)((g_cb_base + 1) >> 8));
    g_patched = 1;
    dbg("patched: dec %x str %x blit %x font %x buf %x\n", d, s, b, g_font, g_buf);
}

/* ----------------------------------------------------- fopen フック */
typedef FILE *(__cdecl *fopen_t)(const char *, const char *);
static fopen_t real_fopen;
static CRITICAL_SECTION g_cs;

static FILE *__cdecl hook_fopen(const char *name, const char *mode)
{
    if (g_enabled) {
        EnterCriticalSection(&g_cs);
        try_patch();
        LeaveCriticalSection(&g_cs);
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
    wsprintfA(g_dir, "%s\\%s", la, gp_dirname);
    CreateDirectoryA(g_dir, NULL);
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("Hillsfar JP patch active\n");
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
