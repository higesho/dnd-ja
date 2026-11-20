/*
 * Pool of Radiance 日本語化パッチ（実行時部分）
 * DOSBox 0.74-3（Forgotten Realms The Archives 同梱版）に winmm.dll の代理として読み込まれる。
 *  - DOSBox が START.EXE を開くとき、EXEPACK を展開してフックを入れたコピーへ差し替える
 *    （GAME.OVR も、必要なら同じ大きさのまま書き換えたコピーへ）
 *  - 1 行表示（709:0626）とワードラップ表示（709:0768）で英文を訳す。
 *    日本語の字は 1 字 1 マス（8x8）。文字列の中では 0x80-0xff の 1 バイト（動的に割り当てる番号）で表す
 *  - 1 文字描画（709:01df）の入口で、その番号の字形をスタックに置き、フォントの参照先をそこへ向けてゲームに描かせる
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
#define SET_AL(v) (*(uint8_t*)&REG32(0) = (uint8_t)(v))
#define R_SP ((uint16_t)REG32(4))
#define R_BP ((uint16_t)REG32(5))
#define SET_SP(v) (*(uint16_t*)&REG32(4) = (uint16_t)(v))
#define SET_BP(v) (*(uint16_t*)&REG32(5) = (uint16_t)(v))
#define SEGVAL(i) (((uint16_t*)A_SEGVAL)[i])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])

/* ------------------------------------------------------- パッチデータ */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)

static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;

/* 訳文の中の日本語の字: 1バイト目 0x81-0xfe, 2バイト目 0x80-0xfe（独自の符号） */
#define IS_LEAD(c) ((c) >= 0x81 && (c) <= 0xfe)
#define IS_TRAIL(c) ((c) >= 0x80 && (c) <= 0xfe)
static uint16_t *g_glyph_index;  /* 65536 -> グリフ番号+1 */
static const uint8_t *g_glyphs;  /* 8バイト/字（行ごと, bit7=左） */
static const uint8_t *g_gflags;  /* 1=行頭禁則 2=行末禁則 */
static int g_cb_base = 0x70;
/* 作品ごとの値（'P' 節）。既定値は Pool of Radiance */
static uint32_t gp_font = 0x5e20;     /* フォントの far ポインタ（DS 内） */
static uint32_t gp_cursor = 0x5e24;   /* ワードラップ表示のカーソル列（DS 内） */
static uint32_t gp_wx1 = 0x14, gp_wx2 = 0x10;   /* ワードラップ表示の左端・右端の引数（bp+） */
static uint32_t gp_px = 0x0e;         /* 1 行表示の x の引数（bp+） */
static uint32_t gp_menuvar = 0x6b2c;  /* メニューのフックで置き換えた mov byte [xxxx],0 */
static uint32_t gp_cp = 1;            /* コードホイールの答えを出す */
static char gp_dirname[32] = "PoolRad_JP";
static uint32_t gp_m2[4][4];          /* 型 2 のメニュー: {見出しの bp-, 項目の bp-, 置き換えた命令(0=lea di,[bp-項目] 1=mov byte [bp-d],0), d} */
static uint32_t gp_drawmode = 1;      /* 1: mov bp,sp/sub sp,12 を置き換え  2: mov bp,sp の後で sp を 12 下げる */
static char gp_exe[16] = "START.EXE";       /* 書き換える実行ファイルの名前 */
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
    if (memcmp(h->magic, "PRJ1", 4)) return 0;
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
        } else if (s->type == 'T') {
            load_table(d);
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0];
            g_logon = (int)v[1];
            if (s->len >= 9 * 4 + 32) {
                gp_font = v[2]; gp_cursor = v[3]; gp_wx1 = v[4]; gp_wx2 = v[5]; gp_px = v[6]; gp_menuvar = v[7]; gp_cp = v[8];
                memcpy(gp_dirname, d + 36, 31);
            }
            if (s->len >= 9 * 4 + 32 + 16) memcpy(gp_exe, d + 36 + 32, 15);
            if (s->len >= 9 * 4 + 32 + 16 + 64) memcpy(gp_m2, d + 36 + 32 + 16, 64);
            if (s->len >= 9 * 4 + 32 + 16 + 64 + 4) gp_drawmode = *(const uint32_t*)(d + 36 + 32 + 16 + 64);
        }
        p = d + s->len;
    }
    return g_glyph_index != NULL;
}

#include "trtable.inc"

/* 'E' / 'X' 節: 先頭16バイトがファイル名 */
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

/* ------------------------------------------------ 日本語の字の動的な番号 */
/* ゲームに渡す文字列では、日本語の字を 0x80-0xff の 1 バイトで表す。
 * 番号と字の対応は使うたびに更新し、いちばん長く使われていない番号から使い回す
 * （画面に描いた字は絵として残るので、番号を使い回しても既に描いた字は変わらない） */
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

/* 独自符号の訳文 → ゲーム用（日本語 1 字 = 1 バイト）。最大 255 バイト */
static int to_game(const uint8_t *s, int n, uint8_t *out, uint16_t *codes)
{
    int i = 0, o = 0;
    uint32_t start = g_clock;
    while (i < n && o < 255) {
        if (IS_LEAD(s[i]) && i + 1 < n && IS_TRAIL(s[i + 1])) {
            uint16_t c = (uint16_t)((s[i] << 8) | s[i + 1]);
            if (codes) codes[o] = c;
            out[o++] = (uint8_t)dyn_get(c);
            i += 2;
        } else {
            if (codes) codes[o] = 0;
            out[o++] = s[i++];
        }
    }
    if (g_clock - start > 120) dbg("warning: %u kanji codes in one string\n", g_clock - start);
    return o;
}

/* ---------------------------------------------------------- 文字列の訳 */
#define NCACHE 128
typedef struct { uint32_t h; int n; uint8_t *en; sbuf out; int hit, flags, ctx; } centry;
static centry g_cache[NCACHE];
static int g_cache_next;

/* 表にない文: ゲームがつなげた文（台本の断片＋名前など）とみて、語の切れ目で表にある断片を長い順に探して訳す。
 * 表にない部分（人の名前・数など）はそのまま。日本語に接する空白は捨てる */
static int is_brk(uint8_t c) { return c == ' ' || (c && strchr(",.!?:;'\"-()", c) != NULL); }
static int g_wrapctx;              /* 1 = ワードラップ表示の文（桁そろえの空白埋めをしない） */
static int tr_segments(sbuf *out, const uint8_t *s, int n)
{
    /* 部品の列: 訳した部品（jp=1）と、原文のまま写す字（jp=0）。原文の空白は、隣が訳した日本語なら捨てる */
    sbuf raw = {0};
    static uint8_t kind[1024];       /* raw の各バイト: 0=原文, 1=訳 */
    int i = 0, any = 0, k;
    while (i < n) {
        int matched = 0;
        if (s[i] != ' ' && (i == 0 || is_brk(s[i - 1]) || is_brk(s[i]))) {
            int j;
            for (j = n; j > i + 1; j--) {
                if (j < n && !is_brk(s[j]) && !is_brk(s[j - 1])) continue;
                {
                    sbuf t = {0};
                    int a = i, b = j;
                    while (b > a && s[b - 1] == ' ') b--;
                    if (b - a >= 2 && tr_str(&t, s + a, b - a, 1)) {
                        int q;
                        if (!g_wrapctx && b + 1 < n && s[b] == ' ' && s[b + 1] == ' ') {
                            /* 後ろに空白が 2 つ以上: 桁をそろえた表なので、訳を英語と同じ幅まで空白で埋める */
                            int cells = 0, r;
                            for (r = 0; r < t.n; r++) { if (IS_LEAD(t.p[r]) && r + 1 < t.n) r++; cells++; }
                            while (cells < b - a) { sb_c(&t, ' '); cells++; }
                            for (q = 0; q < t.n && raw.n + q < 1024; q++) kind[raw.n + q] = 2;
                            sb_put(&raw, t.p, t.n);
                            free(t.p);
                            i = b;
                            matched = 1;
                            any = 1;
                            /* 続く原文の空白は残す */
                            while (i < n && s[i] == ' ') { if (raw.n < 1024) kind[raw.n] = 2; sb_c(&raw, s[i++]); }
                            break;
                        }
                        for (q = 0; q < t.n && raw.n + q < 1024; q++) kind[raw.n + q] = 1;
                        sb_put(&raw, t.p, t.n);
                        free(t.p);
                        i = b;
                        matched = 1;
                        any = 1;
                        break;
                    }
                    free(t.p);
                }
            }
        }
        if (!matched) { if (raw.n < 1024) kind[raw.n] = 0; sb_c(&raw, s[i++]); }
    }
    if (!any) { free(raw.p); return 0; }
    for (k = 0; k < raw.n; k++) {
        if (raw.p[k] == ' ' && k > 0 && k + 1 < raw.n && k < 1024 && kind[k] == 0 && !(k + 1 < raw.n && raw.p[k + 1] == ' ') && !(k > 0 && raw.p[k - 1] == ' ')) {
            int prev_jp = k >= 1 && raw.p[k - 1] >= 0x80;
            int next_jp = k + 1 < raw.n && raw.p[k + 1] >= 0x80;
            if (prev_jp || next_jp) continue;
        }
        sb_c(out, raw.p[k]);
    }
    free(raw.p);
    return 1;
}

static const sbuf *tr_cached(const uint8_t *s, int n, int *flags)
{
    uint32_t h = fnv(s, n);
    int i;
    centry *c;
    for (i = 0; i < NCACHE; i++) {
        c = &g_cache[i];
        if (c->en && c->h == h && c->n == n && c->ctx == g_wrapctx && !memcmp(c->en, s, n)) { *flags = c->flags; return c->hit ? &c->out : NULL; }
    }
    c = &g_cache[g_cache_next];
    g_cache_next = (g_cache_next + 1) % NCACHE;
    free(c->en); free(c->out.p);
    memset(c, 0, sizeof *c);
    c->h = h; c->n = n; c->ctx = g_wrapctx;
    c->en = (uint8_t*)malloc(n + 1);
    memcpy(c->en, s, n);
    c->hit = tr_str(&c->out, s, n, 0);
    if (!c->hit && n >= 3) {
        c->out.n = 0;
        c->hit = tr_segments(&c->out, s, n);
    }
    {
        const tentry *e = tr_exact(s, n);
        c->flags = e ? e->flags : 0;
    }
    *flags = c->flags;
    dbg("%s: %.*s\n", c->hit ? "tr" : "--", n, s);
    return c->hit ? &c->out : NULL;
}

/* ゲームの Pascal 文字列（長さ 1 バイト + 本文）を読む */
static int read_pstr(uint32_t a, uint8_t *buf)
{
    int n = rb(a), i;
    for (i = 0; i < n; i++) buf[i] = rb(a + 1 + i);
    buf[n] = 0;
    return n;
}
static void write_pstr(uint32_t a, const uint8_t *s, int n)
{
    int i;
    if (n > 255) n = 255;
    wb(a, (uint8_t)n);
    for (i = 0; i < n; i++) wb(a + 1 + i, s[i]);
}

/* フォントの参照先（DS:5e20）を一時的に差し替えた場合の元の値 */
static uint32_t g_font_saved, g_font_temp;
static int g_font_swapped;
static void font_restore(void)
{
    uint32_t ds = PHYS_DS;
    if (!g_font_swapped) return;
    if ((rw(ds + gp_font) | ((uint32_t)rw(ds + gp_font + 2) << 16)) == g_font_temp) {   /* ゲームが別のフォントを読み込んでいたら触らない */
        ww(ds + gp_font, (uint16_t)g_font_saved);
        ww(ds + gp_font + 2, (uint16_t)(g_font_saved >> 16));
    }
    g_font_swapped = 0;
}

/* コピープロテクト（コードホイール）: 道筋の点線を表示するとき、答えの語を後ろに付ける。
 * 出題の番号は呼び出し元（GAME.OVR のコピープロテクトの関数）の [bp-2a]。
 * 表は DS:0000 から 21 バイトずつ 13 件: +2 答え（string[6]）, +9..+c ルーン, +d 道筋（string[9]） */
static int cp_answer(const uint8_t *s, int n, sbuf *out)
{
    uint32_t ss = PHYS_SS, ds = PHYS_DS, rec;
    uint16_t pbp = rw(ss + R_BP);
    int idx, a = 0, b = n, k, alen;
    const tentry *e;
    while (a < b && s[a] == ' ') a++;
    while (b > a && s[b - 1] == ' ') b--;
    if (b - a != 9) return 0;
    for (k = a; k < b; k++) if (s[k] != '.' && s[k] != '-' && s[k] != ' ') return 0;
    idx = rb(ss + pbp - 0x2a);
    if (idx >= 13) return 0;
    rec = ds + idx * 0x15;
    if (rb(rec + 0x0d) != 9) return 0;
    for (k = 0; k < 9; k++) if (rb(rec + 0x0e + k) != s[a + k]) return 0;
    alen = rb(rec + 2);
    if (alen < 1 || alen > 8) return 0;
    {
        uint8_t ans[9];
        span sp;
        for (k = 0; k < alen; k++) ans[k] = rb(rec + 3 + k);
        sp.p = ans; sp.n = alen;
        sb_put(out, s, n);
        e = tr_exact((const uint8_t*)"@ANSWER", 7);
        if (e) render(out, e->ja, e->jalen, &sp, 1);
        else { sb_put(out, (const uint8_t*)" (", 2); sb_put(out, ans, alen); sb_c(out, ')'); }
    }
    dbg("copy protection idx %d\n", idx);
    return 1;
}

/* Curse 型: 「type character and press return: 」を出すとき、出題の関数のフレームの [bp-3] にある答えの字を付ける。
 * Print の bp から呼び出し元のフレームを数段たどり、[bp-14]（道筋 0..2）・[bp-15]（升目 0..5）がもっともらしいものを使う */
static int cp_answer2(const uint8_t *s, int n, sbuf *out)
{
    static const char q[] = "type character and press return: ";
    uint32_t ss = PHYS_SS;
    uint16_t f = R_BP;
    int lv;
    const tentry *e;
    if (n != (int)sizeof q - 1 || memcmp(s, q, n)) return 0;
    for (lv = 0; lv < 5; lv++) {
        uint8_t ans, path, box;
        f = rw(ss + f);
        if (!f) break;
        ans = rb(ss + f - 3); path = rb(ss + f - 0x14); box = rb(ss + f - 0x15);
        if (path <= 2 && box <= 5 && ((ans >= 'A' && ans <= 'Z') || (ans >= '0' && ans <= '9'))) {
            span sp;
            sbuf t = {0};
            int fl;
            const sbuf *tr = tr_cached(s, n, &fl);
            if (tr) sb_put(out, tr->p, tr->n); else sb_put(out, s, n);
            sp.p = &ans; sp.n = 1;
            e = tr_exact((const uint8_t*)"@ANSWER", 7);
            if (e) render(&t, e->ja, e->jalen, &sp, 1);
            else { sb_put(&t, (const uint8_t*)"(", 1); sb_c(&t, ans); sb_c(&t, ')'); }
            sb_put(out, t.p, t.n);
            free(t.p);
            dbg("copy protection answer %c (level %d)\n", ans, lv);
            return 1;
        }
    }
    return 0;
}

/* --- 1 行表示 709:0626 の、文字列を局所変数 [bp-100] に写した直後（mov al,[bp-100] の代わり） */
static uint32_t __cdecl cb_print(void)
{
    uint32_t a = PHYS_SS + R_BP - 0x100;
    static uint8_t buf[260], out[260];
    int n, fl = 0;
    const sbuf *t;
    font_restore();
    n = read_pstr(a, buf);
    if (gp_cp == 2 && n > 20) {
        sbuf q = {0};
        if (cp_answer2(buf, n, &q)) {
            int m = to_game(q.p, q.n, out, NULL);
            free(q.p);
            write_pstr(a, out, m);
            SET_AL(m);
            return 0;
        }
        free(q.p);
    }
    if (gp_cp == 1 && n >= 9 && n < 20) {
        sbuf q = {0};
        if (cp_answer(buf, n, &q)) {
            int m = to_game(q.p, q.n, out, NULL);
            free(q.p);
            write_pstr(a, out, m);
            SET_AL(m);
            return 0;
        }
        free(q.p);
    }
    t = n ? tr_cached(buf, n, &fl) : NULL;
    if (t) {
        int m = to_game(t->p ? t->p : (const uint8_t*)"", t->n, out, NULL);
        if (fl & 1) {   /* 中央寄せし直す: x を (英語の長さ - 訳の長さ)/2 だけ右へ */
            uint32_t xa = PHYS_SS + R_BP + gp_px;
            int x = rb(xa) + (n - m) / 2;
            if (x >= 0 && x < 40) wb(xa, (uint8_t)x);
        }
        write_pstr(a, out, m);
        n = m;
    }
    SET_AL(n);
    return 0;
}

/* --- ワードラップ表示 709:0768（x1=[bp+14], y1=[bp+12], x2=[bp+10], y2=[bp+e]）の、
 * 文字列を [bp-100] に写して位置を整えた直後（07f5: mov al,[bp-100] の代わり）。
 * ゲームの折り返しは空白と記号でしか切らないので、日本語を含む訳は DLL が行に分け、
 * 行の終わりに空白を 1 つ置く（行をちょうど x2-1 までで埋め、空白が x2 に来るようにする） */
static int is_ascii_word(uint8_t c) { return c > 0x20 && c < 0x80; }
static int ascii_no_start(uint8_t c) { return c && strchr(",.!?):;'\"", c) != NULL; }

static int prewrap(const uint8_t *in, const uint16_t *codes, int n, int cur, int x1, int x2, uint8_t *out)
{
    int o = 0, i = 0, col = cur, width;
    int has_jp = 0;
    for (i = 0; i < n; i++) if (in[i] >= 0x80) { has_jp = 1; break; }
    if (!has_jp || x2 <= x1) { memcpy(out, in, n); return n; }
    i = 0;
    while (i < n && o < 255) {
        /* 次に置く単位: 日本語 1 字（直後の行頭禁則の字・句読点も付ける）、または英数字の語 */
        int j = i, len;
        if (in[i] == ' ') {
            if (col > x2 - 1) { i++; continue; }      /* 行末の空白は捨てる（折り返しの空白がそこに来る） */
            out[o++] = ' '; col++; i++;
            continue;
        }
        if (in[j] >= 0x80) {
            j++;
            while (j < n && ((in[j] >= 0x80 && (gflags(codes[j]) & 1)) || (in[j] < 0x80 && ascii_no_start(in[j])))) j++;
        } else {
            while (j < n && is_ascii_word(in[j])) j++;
        }
        len = j - i;
        width = x2 - col;                              /* 空白を x2 に置くための残り */
        if (len > width && col > x1) {
            /* この行には入らない: 行を空白で x2-1 まで埋めてから区切りの空白を置く */
            while (col < x2 && o < 255) { out[o++] = ' '; col++; }
            if (o < 255) out[o++] = ' ';
            col = x1;
        }
        while (i < j && o < 255) { out[o++] = in[i++]; col++; }
    }
    return o;
}

static uint32_t __cdecl cb_wprint(void)
{
    uint32_t bp = PHYS_SS + R_BP, a = bp - 0x100, ds = PHYS_DS;
    static uint8_t buf[260], g[260], out[260];
    static uint16_t codes[260];
    int n, fl = 0;
    const sbuf *t;
    font_restore();
    n = read_pstr(a, buf);
    g_wrapctx = 1;
    t = n ? tr_cached(buf, n, &fl) : NULL;
    g_wrapctx = 0;
    if (t) {
        int m = to_game(t->p ? t->p : (const uint8_t*)"", t->n, g, codes);
        int x1 = rb(bp + gp_wx1), x2 = rb(bp + gp_wx2), cur = rb(ds + gp_cursor);
        {   /* 日本語に接する空白を捨てる（前の表示に続けて出す断片の頭の空白など） */
            int r, w = 0;
            for (r = 0; r < m; r++) {
                if (g[r] == ' ' && ((r > 0 && g[r - 1] >= 0x80) || (r + 1 < m && g[r + 1] >= 0x80) || (w == 0 && cur > x1 && r + 1 < m && g[r + 1] == ' '))) continue;
                codes[w] = codes[r];
                g[w++] = g[r];
            }
            m = w;
        }
        m = prewrap(g, codes, m, cur, x1, x2, out);
        write_pstr(a, out, m);
        n = m;
    }
    SET_AL(n);
    return 0;
}

/* --- 1 文字描画 709:01df の入口（mov bp,sp / sub sp,12 の代わり）
 * 引数: [bp+10]=x, [bp+e]=y, [bp+c]=色, [bp+a]=回数, [bp+8]=字, [bp+6]=1 なら字を大文字にして 64 で割った余り */
static uint32_t __cdecl cb_draw(void)
{
    uint32_t ss = PHYS_SS, ds = PHYS_DS, bp;
    uint8_t ch, flag;
    font_restore();
    if (gp_drawmode == 1) SET_BP(R_SP);
    SET_SP(R_SP - 0x12);
    bp = ss + R_BP;
    ch = rb(bp + 8);
    flag = rb(bp + 6);
    if (flag == 1 && ch >= 0x80) {
        uint16_t code = g_dyn_code[ch - 0x80];
        int g = code ? glyph_of(code) : 0, k;
        static const uint8_t tofu[8] = {0x00,0x7e,0x42,0x42,0x42,0x42,0x7e,0x00};
        const uint8_t *bits = g ? g_glyphs + (g - 1) * 8 : tofu;
        uint16_t sp;
        if (code) g_dyn_stamp[ch - 0x80] = g_clock++;
        SET_SP(R_SP - 8);
        sp = R_SP;
        for (k = 0; k < 8; k++) wb(ss + sp + k, bits[k]);
        g_font_saved = rw(ds + gp_font) | ((uint32_t)rw(ds + gp_font + 2) << 16);
        g_font_temp = sp | ((uint32_t)SEGVAL(2) << 16);
        ww(ds + gp_font, sp);
        ww(ds + gp_font + 2, SEGVAL(2));
        g_font_swapped = 1;
        wb(bp + 8, 0);
        wb(bp + 6, 0);
    }
    return 0;
}

/* 全体訳にホットキー（英大文字）を残す。
 * 大文字で始まる語が 2 つ以上ある（項目の並びの）ときは、訳の語数が原文と同じなら語ごとに、その語の最初の大文字を
 * 頭に付ける（訳の語に英大文字がないとき）。語数が違えば -1（呼び出し側は語ごとの訳に回す）。
 * そうでない（説明文の）ときは、訳に英大文字がなければ原文の最初の大文字を頭に付ける */
static int kh_words(const uint8_t *s, int n, int *st, int *ln, int max)
{
    int i = 0, c = 0;
    while (i < n) {
        int j;
        while (i < n && s[i] == ' ') i++;
        if (i >= n) break;
        j = i;
        while (j < n && s[j] != ' ') j++;
        if (c < max) { st[c] = i; ln[c] = j - i; }
        c++;
        i = j;
    }
    return c;
}
static int keep_hotkey(const uint8_t *en, int n, const sbuf *t, uint8_t *out, int maxlen)
{
    int k, hk = 0, has = 0, m, caps = 0;
    int es[24], el[24], js[24], jl[24], ew, jw;
    sbuf q = {0};
    ew = kh_words(en, n, es, el, 24);
    for (k = 0; k < ew && k < 24; k++) if (en[es[k]] >= 'A' && en[es[k]] <= 'Z') caps++;
    if (caps >= 2) {
        int w, i = 0;
        jw = kh_words(t->p, t->n, js, jl, 24);
        if (jw != ew || ew > 24) return -1;
        for (w = 0; w < jw; w++) {
            int h = 0, up = 0;
            for (k = 0; k < el[w]; k++) if (en[es[w] + k] >= 'A' && en[es[w] + k] <= 'Z') { h = en[es[w] + k]; break; }
            for (k = 0; k < jl[w]; k++) if (t->p[js[w] + k] >= 'A' && t->p[js[w] + k] <= 'Z') { up = 1; break; }
            sb_put(&q, t->p + i, js[w] - i);              /* 語の前の空白 */
            if (h && !up) sb_c(&q, (uint8_t)h);
            sb_put(&q, t->p + js[w], jl[w]);
            i = js[w] + jl[w];
        }
        sb_put(&q, t->p + i, t->n - i);
    } else {
        for (k = 0; k < n; k++) if (en[k] >= 'A' && en[k] <= 'Z') { hk = en[k]; break; }
        for (k = 0; k < t->n; k++) if (t->p[k] >= 'A' && t->p[k] <= 'Z') { has = 1; break; }
        if (hk && !has) sb_c(&q, (uint8_t)hk);
        sb_put(&q, t->p, t->n);
    }
    m = to_game(q.p, q.n, out, NULL);
    free(q.p);
    return m > maxlen ? maxlen : m;
}

/* 空白で区切った項目ごとに、メニュー用の表（頭に 0x01 を付けた英語）で訳す。訳にはホットキーの字が付いている。
 * 大文字で始まらない語を含む（項目の並びでなく説明文の）ときは、文字列全体の訳を使う */
static int menu_items(const uint8_t *buf, int n, uint8_t *out, int maxlen)
{
    static sbuf res;
    int i = 0, any = 0, k, sentence = 0, fl = 0;
    const sbuf *t;
    for (k = 0; k < n; k++)
        if ((k == 0 || buf[k - 1] == ' ') && buf[k] != ' ' && !(buf[k] >= 'A' && buf[k] <= 'Z')) sentence = 1;
    t = (sentence && n) ? tr_cached(buf, n, &fl) : NULL;
    if (t && t->n) { int m = keep_hotkey(buf, n, t, out, maxlen); if (m >= 0) return m; }
    res.n = 0;
    while (i < n) {
        int j = i;
        if (buf[i] == ' ') { sb_c(&res, ' '); i++; continue; }
        while (j < n && buf[j] != ' ') j++;
        {
            uint8_t key[64];
            const tentry *e = NULL;
            if (j - i < 60) {
                key[0] = 1;
                memcpy(key + 1, buf + i, j - i);
                e = tr_exact(key, j - i + 1);
            }
            if (e) { render(&res, e->ja, e->jalen, NULL, 0); any = 1; }
            else sb_put(&res, buf + i, j - i);
        }
        i = j;
    }
    if (!any) return -1;
    {
        int m = to_game(res.p, res.n, out, NULL);
        return m > maxlen ? maxlen : m;
    }
}

/* 型 2 のメニュー（Pools of Darkness など）: 見出しと項目を写した直後の lea di,[bp-q] の代わり */
static void menu2(int k)
{
    uint32_t pa = PHYS_SS + R_BP - gp_m2[k][0], ia = PHYS_SS + R_BP - (gp_m2[k][1] & 0xffff);
    int imax = (gp_m2[k][1] >> 16) ? (int)(gp_m2[k][1] >> 16) : 255;
    static uint8_t buf[260], out[260];
    int n, m, fl;
    const sbuf *t;
    font_restore();
    n = read_pstr(pa, buf);
    t = n ? tr_cached(buf, n, &fl) : NULL;
    if (t && t->n) { m = to_game(t->p, t->n, out, NULL); if (m > 40) m = 40; write_pstr(pa, out, m); }
    n = read_pstr(ia, buf);
    dbg("menu2: %.*s\n", n, buf);
    m = n ? menu_items(buf, n, out, imax) : -1;
    if (m >= 0) write_pstr(ia, out, m);
    if (gp_m2[k][2] == 1) wb(PHYS_SS + R_BP - gp_m2[k][3], 0);          /* mov byte [bp-d],0 */
    else if (gp_m2[k][2] == 2) {                                         /* mov al,[bp+s] / mov [addr],al */
        uint8_t al = rb(PHYS_SS + R_BP + (gp_m2[k][3] >> 16));
        wb(PHYS_DS + (gp_m2[k][3] & 0xffff), al);
        SET_AL(al);
    }
    else *(uint16_t*)&REG32(7) = (uint16_t)(R_BP - (gp_m2[k][1] & 0xffff));    /* lea di,[bp-q] */
}
static uint32_t __cdecl cb_menu2a(void) { menu2(0); return 0; }
static uint32_t __cdecl cb_menu2b(void) { menu2(1); return 0; }
static uint32_t __cdecl cb_menu2c(void) { menu2(2); return 0; }
static uint32_t __cdecl cb_menu2d(void) { menu2(3); return 0; }

/* --- 横並びメニュー（GAME.OVR, 区分 2c8cf の 03bd）の、項目の文字列を [bp-53]（string[40]）に写した直後
 * （03ea: mov byte [6b2c],0 の代わり）。項目の頭の大文字がホットキーとして強調される */
static uint32_t __cdecl cb_menu(void)
{
    uint32_t a = PHYS_SS + R_BP - 0x53;
    static uint8_t buf[260], out[260];
    int n, fl = 0;
    const sbuf *t;
    font_restore();
    wb(PHYS_DS + gp_menuvar, 0);
    {   /* 見出し（[bp-2a]）もここで訳す。項目はゲームがこの長さの位置から並べる */
        uint32_t pa = PHYS_SS + R_BP - 0x2a;
        int pn = read_pstr(pa, buf);
        const sbuf *pt = pn ? tr_cached(buf, pn, &fl) : NULL;
        if (pt && pt->n) {
            int m = to_game(pt->p, pt->n, out, NULL);
            if (m > 40) m = 40;
            write_pstr(pa, out, m);
        }
    }
    n = read_pstr(a, buf);
    dbg("menu: %.*s\n", n, buf);
    {   /* 大文字で始まらない語を含む（項目の並びでなく説明文の）ときは、文字列全体の訳を使う */
        int k, sentence = 0;
        for (k = 0; k < n; k++)
            if ((k == 0 || buf[k - 1] == ' ') && buf[k] != ' ' && !(buf[k] >= 'A' && buf[k] <= 'Z')) sentence = 1;
        t = (sentence && n) ? tr_cached(buf, n, &fl) : NULL;
    }
    if (t && t->n) {
        int m = keep_hotkey(buf, n, t, out, 40);
        if (m >= 0) { write_pstr(a, out, m); return 0; }
    }
    {   /* 空白で区切った項目ごとに、メニュー用の表（頭に 0x01 を付けた英語）で訳す。訳にはホットキーの字が付いている */
        static sbuf res;
        int i = 0, any = 0;
        res.n = 0;
        while (i < n) {
            int j = i;
            if (buf[i] == ' ') { sb_c(&res, ' '); i++; continue; }
            while (j < n && buf[j] != ' ') j++;
            {
                uint8_t key[64];
                const tentry *e = NULL;
                if (j - i < 60) {
                    key[0] = 1;
                    memcpy(key + 1, buf + i, j - i);
                    e = tr_exact(key, j - i + 1);
                }
                if (e) { render(&res, e->ja, e->jalen, NULL, 0); any = 1; }
                else sb_put(&res, buf + i, j - i);
            }
            i = j;
        }
        if (any) {
            int m = to_game(res.p, res.n, out, NULL);
            if (m > 40) m = 40;
            write_pstr(a, out, m);
        }
    }
    (void)t; (void)fl;
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    if (g_cb_installed) return 1;
    for (i = 0; i < 8; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy (%x)\n", b + i, tab[b + i]); return 0; }
    tab[b + 0] = (uint32_t)cb_print;
    tab[b + 1] = (uint32_t)cb_wprint;
    tab[b + 2] = (uint32_t)cb_draw;
    tab[b + 3] = (uint32_t)cb_menu;
    tab[b + 4] = (uint32_t)cb_menu2a;
    tab[b + 5] = (uint32_t)cb_menu2b;
    tab[b + 6] = (uint32_t)cb_menu2c;
    tab[b + 7] = (uint32_t)cb_menu2d;
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
static int apply_patches(uint8_t *img, uint32_t ilen, const uint8_t *p, const uint8_t *e)
{
    while (p < e) {
        uint32_t off = ((const uint32_t*)p)[0], n = ((const uint32_t*)p)[1];
        p += 8;
        if (off + n > ilen) return 0;
        memcpy(img + off, p, n);
        p += n;
    }
    return 1;
}

static uint8_t *build_x(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1];
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("X mismatch\n"); return NULL; }
    out = (uint8_t*)malloc(olen);
    memcpy(out, orig, olen);
    if (!apply_patches(out, olen, sec + 8, sec + slen)) { free(out); return NULL; }
    *outlen = olen;
    return out;
}

/* 'E' 節: EXEPACK の EXE を展開し、展開後のイメージに書き換えを当てて普通の MZ にする。
 * u32 元サイズ, u32 元CRC, 続いて 'X' と同じ {位置, 長さ, バイト列}（位置は展開後のイメージ内） */
static uint16_t U16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static uint8_t *build_e(const uint8_t *exe, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1];
    uint32_t hdr, cs, eh, plen, dlen, s, d, nrel = 0, k, seg, pos, hpar, total, minalloc, maxalloc;
    uint16_t rIP, rCS, rSP, rSS;
    uint8_t *img, *out;
    if (osz != olen || crc32(exe, olen) != ocrc) { dbg("E mismatch\n"); return NULL; }
    hdr = U16(exe + 8) * 16u;
    cs = U16(exe + 0x16);
    eh = hdr + cs * 16u;
    if (eh + 0x12 + 0x12d > olen || exe[eh + 16] != 'R' || exe[eh + 17] != 'B') { dbg("not EXEPACK\n"); return NULL; }
    rIP = U16(exe + eh); rCS = U16(exe + eh + 2); rSP = U16(exe + eh + 8); rSS = U16(exe + eh + 10);
    dlen = U16(exe + eh + 12) * 16u;
    plen = eh - hdr;
    img = (uint8_t*)calloc(dlen + 16, 1);
    memcpy(img, exe + hdr, plen);
    s = plen - 1;
    while (img[s] == 0xff && s > plen - 17) s--;
    d = dlen - 1;
    for (;;) {
        uint8_t cmd = img[s];
        uint32_t n = img[s - 2] | (img[s - 1] << 8);
        s -= 3;
        if ((cmd & 0xfe) == 0xb0) {
            uint8_t fb = img[s--];
            while (n--) img[d--] = fb;
        } else if ((cmd & 0xfe) == 0xb2) {
            while (n--) img[d--] = img[s--];
        } else { free(img); dbg("EXEPACK bad cmd\n"); return NULL; }
        if (cmd & 1) break;
    }
    if (!apply_patches(img, dlen, sec + 8, sec + slen)) { free(img); return NULL; }
    /* 再配置表を数える */
    pos = eh + 0x12d;
    for (seg = 0; seg < 16; seg++) { uint32_t c = U16(exe + pos); nrel += c; pos += 2 + c * 2; }
    hpar = (28 + 4 * nrel + 15) / 16;
    total = hpar * 16 + dlen;
    /* 必要な領域は元の EXE（詰めたイメージ + minalloc）と同じに */
    {
        uint32_t ppar = (U16(exe + 2) ? (U16(exe + 4) - 1) * 512u + U16(exe + 2) : U16(exe + 4) * 512u);
        uint32_t loadpar = (ppar - hdr + 15) / 16;
        uint32_t need = loadpar + U16(exe + 0x0a);
        uint32_t want = loadpar + U16(exe + 0x0c);
        minalloc = need > dlen / 16 ? need - dlen / 16 : 0;
        maxalloc = want > dlen / 16 ? want - dlen / 16 : 0xffff;
        if (maxalloc > 0xffff) maxalloc = 0xffff;
    }
    out = (uint8_t*)calloc(total, 1);
    {
        uint16_t h[14];
        h[0] = 0x5a4d; h[1] = (uint16_t)(total % 512); h[2] = (uint16_t)((total + 511) / 512); h[3] = (uint16_t)nrel;
        h[4] = (uint16_t)hpar; h[5] = (uint16_t)minalloc; h[6] = (uint16_t)maxalloc; h[7] = rSS; h[8] = rSP; h[9] = 0;
        h[10] = rIP; h[11] = rCS; h[12] = 28; h[13] = 0;
        memcpy(out, h, 28);
    }
    pos = eh + 0x12d;
    k = 0;
    for (seg = 0; seg < 16; seg++) {
        uint32_t c = U16(exe + pos), j;
        pos += 2;
        for (j = 0; j < c; j++, k++) {
            uint16_t off = U16(exe + pos), sg = (uint16_t)(seg * 0x1000);
            pos += 2;
            memcpy(out + 28 + k * 4, &off, 2);
            memcpy(out + 28 + k * 4 + 2, &sg, 2);
        }
    }
    memcpy(out + hpar * 16, img, dlen);
    free(img);
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
static int g_exe_ok;

static const char *patched_path(const char *orig_path, char *buf)
{
    const char *bn = base_name(orig_path);
    uint32_t slen, olen, nlen;
    const uint8_t *se, *sx;
    uint8_t *orig, *out = NULL;
    int is_exe = !_stricmp(bn, gp_exe);
    if (!g_enabled) return NULL;
    se = find_sect('E', bn, &slen);
    sx = se ? NULL : find_sect('X', bn, &slen);
    if (!se && !sx) return NULL;
    if (!is_exe && !g_exe_ok) return NULL;   /* 対応版の START.EXE のときだけ、ほかも差し替える */
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) {
        if (is_exe) {     /* 元のファイルが変わっていないか（大きさで）確かめる */
            WIN32_FILE_ATTRIBUTE_DATA fa;
            if (!(GetFileAttributesExA(orig_path, GetFileExInfoStandard, &fa) && fa.nFileSizeLow == ((const uint32_t*)(se ? se : sx))[0])) return NULL;
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
    out = se ? build_e(orig, olen, se, slen, &nlen) : build_x(orig, olen, sx, slen, &nlen);
    free(orig);
    if (!out) return NULL;
    if (!write_all(buf, out, nlen)) { free(out); return NULL; }
    if (is_exe) g_exe_ok = 1;
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
    dbg("Gold Box JP patch active (%s), blob %08X\n", gp_dirname, g_blob_crc);
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
