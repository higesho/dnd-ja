/*
 * Eye of the Beholder 日本語化パッチ（実行時の部分）
 * DOSBox 0.74-3（Forgotten Realms: The Archives 同梱版）に winmm.dll の代理として読み込まれる。
 *  - EOB.EXE / MCGA.OVL を開く瞬間に、コールバック命令を入れたコピーへ差し替える
 *  - メッセージ表示関数（1f89:060a）: 原文を訳文に置き換え、和文の折り返しをして1行ずつ返す
 *  - 文字列描画（MCGA.OVL）: 原文なら訳文を引き、和文（美咲ゴシック）も自前で描く
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "resource.h"

void proxy_init(void);
void dev_init(const char *gamedir);

/* ---------------------------------------------------------------- 記録 */
static char g_dir[MAX_PATH], g_gamedir[MAX_PATH];
static FILE *g_log;
static int g_logon, g_dev;
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

/* ------------------------------------------------- DOSBox 0.74-3 の内部 */
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
static __inline void ww(uint32_t a, uint16_t v) { wb(a, (uint8_t)v); wb(a + 1, (uint8_t)(v >> 8)); }
static __inline int16_t rws(uint32_t a) { return (int16_t)rw(a); }
#define REG32(i) (((uint32_t*)A_REGS)[i])
#define SET_AX(v) (*(uint16_t*)&REG32(0) = (uint16_t)(v))
#define SET_DX(v) (*(uint16_t*)&REG32(2) = (uint16_t)(v))
#define R_SP ((uint16_t)REG32(4))
#define R_BP ((uint16_t)REG32(5))
#define SEGV(i) (((uint16_t*)A_SEGVAL)[i])
#define PHYS_CS (((uint32_t*)A_SEGPHYS)[1])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
static __inline uint32_t farptr(uint32_t a) { return ((uint32_t)rw(a + 2) << 4) + rw(a); }

/* ゲーム側の変数（DS 相対） */
#define V_CW     0x8100   /* byte 文字幅 */
#define V_CH     0x8101   /* byte 行の高さ */
#define V_COL    0xfe52   /* 桁（文字単位） */
#define V_ROW    0xfe54   /* 行 */
#define V_WH     0xfe60   /* 窓の高さ px */
#define V_WW     0xfe62   /* 窓の幅（8px 単位） */
#define V_WX     0xfe64   /* 窓の x（8px 単位） */
#define V_WY     0xfe66   /* 窓の y px */
#define V_BG     0x811b
#define V_FG     0x811d
#define JP_PITCH 9        /* 和文を出すときの行の高さ */

/* ------------------------------------------------------- 埋め込みデータ */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)

static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;
static const uint8_t *g_glyphs, *g_gflags;
static uint32_t g_nglyph;
static int g_cb_base = 0x70;
static int g_enabled;

/* 訳文表（完全一致: 開番地法のハッシュ表） */
typedef struct { const uint8_t *key; uint16_t klen, vlen; const uint16_t *val; } tent;
static tent *g_tab;
static uint32_t g_tabsz;
static tent *g_pat;
static uint32_t g_npat;

static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xffffffffu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}
static uint32_t fnv(const uint8_t *p, uint32_t n)
{
    uint32_t h = 2166136261u;
    while (n--) h = (h ^ *p++) * 16777619u;
    return h;
}

static const uint8_t *read_entries(const uint8_t *d, tent *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        out[i].klen = *(const uint16_t*)d; d += 2;
        out[i].key = d; d += out[i].klen;
        out[i].vlen = *(const uint16_t*)d; d += 2;
        out[i].val = (const uint16_t*)d; d += out[i].vlen * 2;
    }
    return d;
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
    if (memcmp(h->magic, "EJP1", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len);
    p = g_blob + sizeof(blob_hdr);
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == 'F') {
            g_nglyph = *(const uint32_t*)d;
            g_glyphs = d + 4;
            g_gflags = g_glyphs + g_nglyph * 8;
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0];
            g_logon = (int)v[1];
        } else if (s->type == 'T') {
            uint32_t n = *(const uint32_t*)d, k;
            tent *tmp = (tent*)calloc(n ? n : 1, sizeof(tent));
            read_entries(d + 4, tmp, n);
            g_tabsz = 16;
            while (g_tabsz < n * 2) g_tabsz <<= 1;
            g_tab = (tent*)calloc(g_tabsz, sizeof(tent));
            for (k = 0; k < n; k++) {
                uint32_t hh = fnv(tmp[k].key, tmp[k].klen) & (g_tabsz - 1);
                while (g_tab[hh].key) hh = (hh + 1) & (g_tabsz - 1);
                g_tab[hh] = tmp[k];
            }
            free(tmp);
        } else if (s->type == 'Q') {
            g_npat = *(const uint32_t*)d;
            g_pat = (tent*)calloc(g_npat ? g_npat : 1, sizeof(tent));
            read_entries(d + 4, g_pat, g_npat);
        }
        p = d + s->len;
    }
    return g_tab != NULL;
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

/* ------------------------------------------------------------ 訳文さがし */
#define MAXS 4096
/* 原文を照合キーへ: 空白・制御文字の並びは1つの空白、前後は詰める、小文字 */
static int make_key(const uint8_t *s, int n, uint8_t *key)
{
    int i, k = 0, sp = 0;
    for (i = 0; i < n; i++) {
        uint8_t c = s[i] & 0x7f;
        if (c <= 0x20) { sp = 1; continue; }
        if (sp && k) key[k++] = ' ';
        sp = 0;
        if (c >= 'A' && c <= 'Z') c += 32;
        key[k++] = c;
        if (k >= MAXS - 2) break;
    }
    key[k] = 0;
    return k;
}

static const tent *lookup_key(const uint8_t *key, int klen)
{
    uint32_t hh;
    if (!g_tab || !klen) return NULL;
    hh = fnv(key, klen) & (g_tabsz - 1);
    while (g_tab[hh].key) {
        if (g_tab[hh].klen == klen && !memcmp(g_tab[hh].key, key, klen)) return &g_tab[hh];
        hh = (hh + 1) & (g_tabsz - 1);
    }
    return NULL;
}

/* 文字列を訳す。out へ u16 列（ASCII / 0x0d / 0x8000|字形）。訳がなければ -1 */
/* パターン照合: 0x01=文字列 0x02=数 0x03=16進。最短一致＋後戻り。as/al に引数の位置と長さ */
static int pmatch(const uint8_t *p, int pn, const uint8_t *s, int sn, int base, int *as, int *al, int na)
{
    int i;
    if (!pn) return sn == 0;
    if (*p >= 1 && *p <= 3) {
        if (na >= 10) return 0;
        for (i = 1; i <= sn; i++) {
            uint8_t c = s[i - 1];
            if (*p == 2 && !((c >= '0' && c <= '9') || (i == 1 && (c == '-' || c == '+')))) break;
            if (*p == 3 && !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) break;
            as[na] = base; al[na] = i;
            if (pmatch(p + 1, pn - 1, s + i, sn - i, base + i, as, al, na + 1)) return 1;
        }
        return 0;
    }
    if (!sn || *p != *s) return 0;
    return pmatch(p + 1, pn - 1, s + 1, sn - 1, base + 1, as, al, na);
}

static int translate_key(const uint8_t *key, int klen, uint16_t *out, int cap, int depth)
{
    const tent *t = lookup_key(key, klen);
    uint32_t i;
    if (t) {
        int n = t->vlen < cap ? t->vlen : cap;
        memcpy(out, t->val, n * 2);
        return n;
    }
    if (depth > 2) return -1;
    for (i = 0; i < g_npat; i++) {
        const tent *q = &g_pat[i];
        int as[10], al[10], k, n = 0, nargs = 0;
        for (k = 0; k < q->klen; k++) if (q->key[k] >= 1 && q->key[k] <= 3) nargs++;
        if (!pmatch(q->key, q->klen, key, klen, 0, as, al, 0)) continue;
        {
            /* 訳文の先頭が 0xF0FF なら「文字列の引数がすべて訳せるときだけ」使う */
            int strict = q->vlen && q->val[0] == 0xF0FF, ok = 1;
            for (k = strict; k < q->vlen && n < cap; k++) {
                uint16_t v = q->val[k];
                if (v >= 0xF000 && v < 0xF00A) {
                    int a = v - 0xF000, r, j, s0, sl;
                    if (a >= nargs) continue;
                    s0 = as[a]; sl = al[a];
                    /* 引数の前後の空白は訳さない */
                    while (sl > 0 && key[s0] == ' ') s0++, sl--;
                    while (sl > 0 && key[s0 + sl - 1] == ' ') sl--;
                    r = sl ? translate_key(key + s0, sl, out + n, cap - n, depth + 1) : 0;
                    if (r < 0) {      /* 訳のない引数（人名・数）は原文のまま */
                        int digits = 1;
                        for (j = 0; j < sl; j++) if (key[s0 + j] < '0' || key[s0 + j] > '9') digits = 0;
                        if (strict && !digits) { ok = 0; break; }
                        for (j = 0; j < sl && n < cap; j++) out[n++] = key[s0 + j];
                    } else n += r;
                } else out[n++] = v;
            }
            if (!ok) continue;
        }
        return n;
    }
    return -1;
}

/* 原文 -> 訳。まず大文字小文字を区別する表（キー先頭 0x01）、なければ小文字のキーで */
static int translate_str(const uint8_t *s, int n, uint16_t *out, int cap)
{
    static uint8_t key[MAXS], cs[MAXS + 1];
    int k, i, sp = 0, m = 1;
    const tent *t;
    cs[0] = 1;
    for (i = 0; i < n && m < MAXS - 2; i++) {
        uint8_t c = s[i] & 0x7f;
        if (c <= 0x20) { sp = 1; continue; }
        if (sp && m > 1) cs[m++] = ' ';
        sp = 0;
        cs[m++] = c;
    }
    t = lookup_key(cs, m);
    if (t) {
        int r = t->vlen < cap ? t->vlen : cap;
        memcpy(out, t->val, r * 2);
        return r;
    }
    k = make_key(s, n, key);
    return translate_key(key, k, out, cap, 0);
}

/* ------------------------------------------------------------- 未訳の記録 */
static void log_miss(const char *where, const uint8_t *s, int n)
{
    static uint32_t seen[8192];
    static int nseen;
    static FILE *mf;
    uint8_t key[512];
    int k, i, letters = 0;
    uint32_t h;
    if (!g_dev) return;
    k = make_key(s, n > 500 ? 500 : n, key);
    for (i = 0; i < k; i++) if (key[i] >= 'a' && key[i] <= 'z') letters++;
    if (letters < 2) return;
    h = fnv(key, k);
    for (i = 0; i < nseen; i++) if (seen[i] == h) return;
    if (nseen < 8192) seen[nseen++] = h;
    if (!mf) {
        char p[MAX_PATH];
        wsprintfA(p, "%s\\_jp\\test\\miss.txt", g_gamedir);
        mf = fopen(p, "a");
        if (!mf) return;
    }
    fprintf(mf, "%s\t", where);
    for (i = 0; i < n && i < 500; i++) {
        uint8_t c = s[i];
        if (c == '\r') fputs("\\r", mf);
        else if (c < 0x20 || c >= 0x7f) fprintf(mf, "\\x%02x", c);
        else fputc(c, mf);
    }
    fputc('\n', mf);
    fflush(mf);
}

/* ------------------------------------------------------------ 描画 */
static int is_jp_unit(uint16_t u) { return u >= 0x8000 && u < 0xC000; }
static int gflag(uint16_t u) { return is_jp_unit(u) && (u & 0x7fff) < g_nglyph ? g_gflags[u & 0x7fff] : 0; }

/* ドライバの現在の描画先・字形 */
typedef struct { uint32_t page, font; int fh, fw; } drvctx;

static void drv_ctx(drvctx *c)
{
    uint32_t cs = PHYS_CS;
    uint32_t pidx = farptr(cs + 0xb), ptab = farptr(cs + 7), pf = farptr(cs + 3);
    uint16_t seg = rw(ptab + (rw(pidx) & 0x1f) * 2);
    c->page = (uint32_t)seg << 4;
    c->font = farptr(pf);
    c->fh = rb(c->font + 0x102);
    c->fw = rb(c->font + 0x103);
}

static void draw_ascii(const drvctx *c, uint8_t ch, int x, int y, uint8_t fg, uint8_t bg)
{
    uint32_t g = c->font + rw(c->font + 2 * ((ch & 0x7f) + 1));
    int r, k, bpr = (c->fw + 7) / 8;
    for (r = 0; r < c->fh; r++) {
        if (y + r >= 200) break;
        for (k = 0; k < c->fw; k++) {
            uint8_t b = rb(g + r * bpr + k / 8);
            uint32_t a = c->page + (uint32_t)(y + r) * 320 + x + k;
            if (x + k >= 320) break;
            if (b & (0x80 >> (k & 7))) wb(a, fg);
            else if (bg) wb(a, bg);
        }
    }
}

static void draw_jp(const drvctx *c, uint16_t u, int x, int y, uint8_t fg, uint8_t bg)
{
    static const uint8_t tofu[8] = {0xfe,0x82,0x82,0x82,0x82,0x82,0xfe,0};
    uint32_t gi = u & 0x7fff;
    const uint8_t *bits = gi < g_nglyph ? g_glyphs + gi * 8 : tofu;
    int r, k, h = c->fh > 8 ? c->fh : 8;
    for (r = 0; r < h; r++) {
        uint8_t row = r < 8 ? bits[r] : 0;
        if (y + r >= 200) break;
        for (k = 0; k < 8; k++) {
            uint32_t a = c->page + (uint32_t)(y + r) * 320 + x + k;
            if (x + k >= 320) break;
            if (row & (0x80 >> k)) wb(a, fg);
            else if (bg) wb(a, bg);
        }
    }
}

/* u16 列を描く（ドライバの printText と同じ決まり: 0x0d で改行、右端で折り返し） */
static void draw_units(const drvctx *c, const uint16_t *u, int n, int x, int y, uint8_t fg, uint8_t bg)
{
    int x0 = x, i, lh = c->fh;
    for (i = 0; i < n; i++) if (is_jp_unit(u[i])) { if (lh < 8) lh = 8; break; }   /* 和文の行は 8 ドット以上あける */
    for (i = 0; i < n; i++) {
        uint16_t v = u[i];
        int w;
        if (y >= 200) break;
        if (v == 0x0d) { x = x0; y += lh; continue; }
        if ((v & 0xff00) == 0xE300) { x = x0 + (v & 0xff) * c->fw; continue; }   /* 桁へ移動 */
        if (v >= 0xC000) continue;
        w = is_jp_unit(v) ? 8 : c->fw;
        if (x + w > 320) { x = x0; y += lh; if (y >= 200) break; }
        if (is_jp_unit(v)) draw_jp(c, v, x, y, fg, bg);
        else draw_ascii(c, (uint8_t)v, x, y, fg, bg);
        x += w;
    }
}

/* ゲスト文字列（和文は 2 バイト: 0x80|上位7bit, 0x80|下位7bit）を u16 列へ */
static int decode_guest(uint32_t s, uint16_t *out, int cap, int *has_jp)
{
    int n = 0, i;
    *has_jp = 0;
    for (i = 0; i < MAXS && n < cap; i++) {
        uint8_t c = rb(s + i);
        if (!c) break;
        if (c >= 0x80) {
            uint8_t d = rb(s + i + 1);
            if (d >= 0x80) {
                out[n++] = (uint16_t)(0x8000 | ((c & 0x7f) << 7) | (d & 0x7f));
                *has_jp = 1;
                i++;
                continue;
            }
        }
        out[n++] = c;
    }
    return n;
}

static int g_in_disp;
static int g_shift_next;   /* 改行なしの行の続きを、桁の端数だけ左へ寄せる */

/* 原文（ゲスト）を訳して u16 列にする。訳がなければ原文のまま。has_jp に和文の有無 */
static int prepare_text(uint32_t s, uint16_t *v, int cap, int *has_jp)
{
    static uint16_t t[MAXS];
    static uint8_t raw[MAXS];
    int n, i, m;
    n = decode_guest(s, v, cap, has_jp);
    if (*has_jp || g_in_disp || n <= 0) return n;
    for (i = 0; i < n; i++) raw[i] = (uint8_t)v[i];
    m = translate_str(raw, n, t, MAXS);
    if (m < 0) {
        log_miss("print", raw, n);
        return n;
    }
    {   /* 原文の先頭の改行は残す */
        int lead = 0, k = 0, j;
        while (lead < n && raw[lead] == 0x0d) v[k++] = raw[lead++];
        for (j = 0; j < m && k < cap; j++) v[k++] = t[j];
        for (j = 0; j < k; j++) if (is_jp_unit(v[j])) *has_jp = 1;
        return k;
    }
}

/* コールバック: ドライバの文字列描画 printText(char far *s, x, y, fg, bg) */
static uint32_t __cdecl cb_print(void)
{
    static uint16_t v[MAXS];
    uint32_t sp = PHYS_SS + R_SP;
    uint32_t s = farptr(sp + 4);
    int x = rws(sp + 8), y = rws(sp + 10), n, jp;
    uint8_t fg = rb(sp + 12), bg = rb(sp + 14);
    drvctx c;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= 320 || y >= 200) return 0;
    drv_ctx(&c);
    n = prepare_text(s, v, MAXS, &jp);
    if (g_in_disp && g_shift_next) { x += g_shift_next; if (x < 0) x = 0; g_shift_next = 0; }
    if (g_dev && jp) dbg("print x=%d y=%d fg=%d bg=%d ret=%04x:%04x\n", x, y, fg, bg, rw(sp + 2), rw(sp));
    draw_units(&c, v, n, x, y, fg, bg);
    return 0;
}

/* コピープロテクトの質問（"On the page with this symbol..."）なら、答えの単語を書き足す。
 * 呼び出し元の SI = 問題の番号（DS:65f1 の 4 バイトの表: 記号・行・語）、答えは DS:6669 からの単語の並び */
static int add_protect_answer(uint32_t s, uint16_t *v, int n)
{
    static const char probe[] = "on the page with this symbol";
    static const uint8_t label[] = "@cp-answer";
    char head[64];
    int i, k = 0, idx;
    uint32_t w;
    for (i = 0; i < 63; i++) {
        uint8_t c = rb(s + i);
        if (!c) break;
        if (c == 0x0d && !k) continue;
        head[k++] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    head[k] = 0;
    if (strncmp(head, probe, sizeof probe - 1)) return n;
    idx = (uint16_t)REG32(6);
    if (idx >= 30) return n;
    w = PHYS_DS + 0x6669;
    for (i = 0; i < idx; i++) {
        int g = 0;
        while (rb(w) && g++ < 32) w++;
        w++;
    }
    if (n < MAXS - 40) {
        int m;
        v[n++] = 0x0d;
        m = translate_str(label, sizeof label - 1, v + n, 16);
        if (m > 0) n += m;
        for (i = 0; i < 20 && n < MAXS; i++) {
            uint8_t c = rb(w + i);
            if (!c) break;
            v[n++] = c;
        }
    }
    dbg("protect: index %d\n", idx);
    return n;
}

/* コールバック: 影つき文字列 printShadedText(char far *s, x, y, col, bg)（EOB.EXE 側）。
 * 原作は 色12 で (x-1,y) (x,y+1) (x-1,y+1) に影、最後に (x,y) に本体。和文は影を右下に1回だけ */
static uint32_t __cdecl cb_shaded(void)
{
    static uint16_t v[MAXS];
    uint32_t sp = PHYS_SS + R_SP;
    uint32_t s = farptr(sp + 4);
    int x = rws(sp + 8), y = rws(sp + 10), n, jp;
    uint8_t col = rb(sp + 12), bg = rb(sp + 14);
    drvctx c;
    if (y >= 200) return 0;
    {
        /* ドライバの文脈（描画先・字形）はドライバの CS から取るので、DS の関数表から CS を得る */
        uint32_t fp = PHYS_DS + 0x80ca;
        uint32_t cs = (uint32_t)rw(fp + 2) << 4;
        uint32_t pidx = farptr(cs + 0xb), ptab = farptr(cs + 7), pf = farptr(cs + 3);
        c.page = (uint32_t)rw(ptab + (rw(pidx) & 0x1f) * 2) << 4;
        c.font = farptr(pf);
        c.fh = rb(c.font + 0x102);
        c.fw = rb(c.font + 0x103);
    }
    n = prepare_text(s, v, MAXS, &jp);
    n = add_protect_answer(s, v, n);
    if (g_dev) dbg("shaded x=%d y=%d col=%d bg=%d jp=%d ret=%04x:%04x\n", x, y, col, bg, jp, rw(sp + 2), rw(sp));
    if (!jp) {
        draw_units(&c, v, n, x - 1 < 0 ? 0 : x - 1, y, 12, bg);
        draw_units(&c, v, n, x, y + 1, 12, 0);
        draw_units(&c, v, n, x - 1 < 0 ? 0 : x - 1, y + 1, 12, 0);
        draw_units(&c, v, n, x, y, col, 0);
        return 0;
    }
    if (bg)     /* 背景の塗り（原作の1回目と同じ範囲） */
        draw_units(&c, v, n, x - 1 < 0 ? 0 : x - 1, y, bg, bg);
    draw_units(&c, v, n, x + 1, y + 1, 12, 0);   /* 和文の影は右下に1つ */
    draw_units(&c, v, n, x, y, col, 0);
    return 0;
}

/* ---------------------------------------------------------- メッセージ表示 */
/* 出力の予定: 1行ずつ、または変数の書き換え */
typedef struct { uint8_t kind, flag; uint16_t addr, val; int start, len, px; } outrec;
enum { O_LINE, O_SETB, O_SETW, O_COL };
static outrec g_out[512];
static int g_nout, g_iout;
static uint16_t g_units[MAXS];
static int g_save_ch, g_active, g_pend_col, g_pend_px, g_cw;

static int unit_w(uint16_t v) { return is_jp_unit(v) ? 8 : g_cw; }

static void add_line(int start, int len, int flag, int px)
{
    outrec *o;
    if (g_nout >= 512) return;
    o = &g_out[g_nout++];
    o->kind = O_LINE; o->flag = (uint8_t)flag; o->start = start; o->len = len; o->px = px;
}
static void add_set(int kind, uint16_t addr, uint16_t val)
{
    outrec *o;
    if (g_nout >= 512) return;
    o = &g_out[g_nout++];
    o->kind = (uint8_t)kind; o->addr = addr; o->val = val;
}

static int is_alnum(uint16_t c) { return c < 0x80 && ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')); }

/* units[0..n) を折り返して出力予定にする。制御: 0x0d 改行, 0xE000|x 前景色, 0xE100|x 背景色,
 * 0xE200|n 桁指定, 0xE300|n 桁まで空白 */
static void layout(int n, int col_px, int width_px)
{
    int i = 0, ls = 0, px = col_px;
    int brk = -1, brk_px = 0, brk_skip = 0;
    while (i < n) {
        uint16_t v = g_units[i];
        if (v == 0x0d) {
            add_line(ls, i - ls, 1, px - col_px);
            i++; ls = i; px = 0; col_px = 0; brk = -1;
            continue;
        }
        if (v >= 0xE000 && v < 0xF000) {
            int code = (v >> 8) & 0x0f, arg = v & 0xff;
            if (i > ls) add_line(ls, i - ls, 0, px - col_px);
            if (code == 0) add_set(O_SETW, V_BG, (uint16_t)arg);
            else if (code == 1) add_set(O_SETW, V_FG, (uint16_t)arg);
            else if (code == 2) { add_set(O_COL, V_COL, (uint16_t)arg); px = arg * g_cw; }
            else if (code == 3) { if (arg * g_cw > px) { add_set(O_COL, V_COL, (uint16_t)arg); px = arg * g_cw; } }
            i++; ls = i; col_px = px; brk = -1;
            continue;
        }
        /* 折り返し位置の候補（この文字の前で切れるか） */
        if (i > ls) {
            uint16_t p = g_units[i - 1];
            if (v == ' ') { brk = i; brk_px = px; brk_skip = 1; }
            else if ((is_jp_unit(v) || is_jp_unit(p)) && !(gflag(v) & 1) && !(gflag(p) & 2)
                     && !(is_alnum(p) && is_alnum(v)) && !(v < 0x80 && strchr(",.!?)]:;'\"", (char)v))) {
                brk = i; brk_px = px; brk_skip = 0;
            }
        }
        if (px + unit_w(v) > width_px && v != ' ') {
            if (brk > ls) {
                add_line(ls, brk - ls, 1, brk_px - col_px);
                i = brk + brk_skip;
            } else if (i > ls) {
                add_line(ls, i - ls, 1, px - col_px);
            } else {
                add_line(ls, 1, 1, unit_w(v));
                i++;
            }
            ls = i; px = 0; col_px = 0; brk = -1;
            /* 行頭の空白は捨てる */
            while (ls < n && g_units[ls] == ' ') ls++, i = ls;
            continue;
        }
        px += unit_w(v);
        i++;
    }
    if (i > ls) add_line(ls, i - ls, 0, px - col_px);
}

/* 原文（ゲスト）を u16 の制御付き列へ（訳がない時用） */
static int raw_units(uint32_t s, uint16_t *out, int cap)
{
    int n = 0, i;
    for (i = 0; i < MAXS && n < cap; i++) {
        uint8_t c = rb(s + i), last = 0;
        if (!c) break;
        if (c & 0x80) { c &= 0x7f; last = 1; }
        if (c == 0x02 || c == 0x06 || c == 0x09 || c == 0x13) {
            uint8_t a = rb(s + ++i);
            out[n++] = (uint16_t)((c == 0x02 ? 0xE000 : c == 0x06 ? 0xE100 : c == 0x09 ? 0xE200 : 0xE300) | a);
        } else if (c == 0x0d) out[n++] = 0x0d;
        else if (c >= 0x20) out[n++] = c;
        if (last) break;
    }
    return n;
}

/* コールバック: メッセージ表示の始め。[bp+6] = 文字列 */
static uint32_t __cdecl cb_disp_prep(void)
{
    static uint8_t raw[MAXS];
    static uint16_t t[MAXS];
    uint32_t s = farptr(PHYS_SS + R_BP + 6), ds = PHYS_DS;
    int n = 0, i, m, col, width;
    uint16_t lc[8], tc[8];
    int nlc = 0, ntc = 0;
    g_nout = g_iout = 0;
    g_active = 0;
    g_pend_col = -1;
    if (!rw(PHYS_SS + R_BP + 6) && !rw(PHYS_SS + R_BP + 8)) return 0;
    g_cw = rb(ds + V_CW);
    if (g_cw <= 0) g_cw = 6;
    /* 原文を読む（最後の文字は bit7 が立っていることがある） */
    for (i = 0; i < MAXS - 1; i++) {
        uint8_t c = rb(s + i);
        if (!c) break;
        raw[n++] = c;
        if (c & 0x80) break;
    }
    if (g_dev) {   /* 試験用: EOBJP_DISPTEST の文（\xNN 可）でメッセージを置き換える */
        static char tv[512];
        static int tl = -1;
        if (tl < 0) tl = (int)GetEnvironmentVariableA("EOBJP_DISPTEST", tv, sizeof tv - 1);
        if (tl > 0) {
            int j;
            n = 0;
            for (j = 0; j < tl && n < MAXS - 1; j++) {
                if (tv[j] == '\\' && tv[j + 1] == 'x' && j + 3 < tl + 1) {
                    char h[3] = {tv[j + 2], tv[j + 3], 0};
                    raw[n++] = (uint8_t)strtol(h, NULL, 16); j += 3;
                } else raw[n++] = (uint8_t)tv[j];
            }
        }
    }
    m = -1;
    {
        /* 制御コード（色替えなど）は、文の前と後ろにあるものだけなら外して訳し、あとで付け直す
         * （"\x06\x01%s has gained a level of experience.\x06\x0f\r" など）。文の途中にあれば訳さない */
        int ctl = 0, seen_text = 0, pend = 0;
        static uint8_t body[MAXS];
        int bn = 0;
        nlc = ntc = 0;
        for (i = 0; i < n; i++) {
            uint8_t c = raw[i] & 0x7f;
            if ((c == 0x02 || c == 0x06 || c == 0x09 || c == 0x13) && i + 1 < n) {
                uint16_t u = (uint16_t)((c == 0x02 ? 0xE000 : c == 0x06 ? 0xE100 : c == 0x09 ? 0xE200 : 0xE300) | (raw[i + 1] & 0x7f));
                if (c == 0x09 || c == 0x13) ctl = 1;                     /* 桁の指定は訳さない */
                if (!seen_text) { if (nlc < 8) lc[nlc++] = u; else ctl = 1; }
                else { if (ntc < 8) tc[ntc++] = u; else ctl = 1; pend = 1; }
                i++;
                continue;
            }
            if (c > 0x20) { seen_text = 1; if (pend) ctl = 1; }   /* 後ろの制御コードの後にまた文字: 途中にある */
            body[bn++] = raw[i];
        }
        if (nlc || ntc) {
            if (ctl) nlc = ntc = 0;
            else { memcpy(raw, body, bn); n = bn; }
        }
        if (!ctl) {
            /* 「%s has lost his 」＋品名＋「.」の3回に分けた文: 最後の「.」を「を失った。」に */
            static int lost_ctx;
            static const uint8_t tail[] = "@lost-tail";
            uint8_t k[64];
            int kl = make_key(raw, n < 60 ? n : 60, k);
            if (lost_ctx > 0 && kl == 1 && k[0] == '.') {
                m = translate_str(tail, sizeof tail - 1, t, MAXS - 64);
                lost_ctx = 0;
            } else {
                m = translate_str(raw, n, t, MAXS - 64);
                if (lost_ctx > 0) lost_ctx--;
            }
            if (kl > 14 && (!memcmp(k + kl - 12, "has lost his", 12) || !memcmp(k + kl - 12, "has lost her", 12))) lost_ctx = 2;
        }
    }
    if (m >= 0) {
        int k = 0, lead = 0, tail = 0, j;
        while (lead < n && (raw[lead] & 0x7f) == 0x0d) { g_units[k++] = 0x0d; lead++; }
        for (j = 0; j < nlc; j++) g_units[k++] = lc[j];
        for (j = 0; j < m; j++) g_units[k++] = t[j];
        for (j = 0; j < ntc; j++) g_units[k++] = tc[j];
        for (j = n - 1; j >= lead && ((raw[j] & 0x7f) == 0x0d || (raw[j] & 0x7f) == ' '); j--)
            if ((raw[j] & 0x7f) == 0x0d) tail++;
        while (tail-- > 0) g_units[k++] = 0x0d;
        n = k;
    } else {
        log_miss("disp", raw, n);
        n = raw_units(s, g_units, MAXS);
    }
    col = rws(ds + V_COL);
    width = rws(ds + V_WW) * 8;
    layout(n, col * g_cw, width);
    g_in_disp = 1;
    g_save_ch = rb(ds + V_CH);
    g_active = 1;
    wb(ds + V_CH, JP_PITCH);
    return 0;
}

/* コールバック: 次の1行を ss:[bp-0xac] へ。ax=1 で行あり（dx=改行フラグ）、ax=0 で終わり */
static uint32_t __cdecl cb_disp_next(void)
{
    uint32_t ds = PHYS_DS, buf = PHYS_SS + R_BP - 0xac;
    if (g_pend_col >= 0) {
        /* 改行なしで出した行のあと、桁を実際の幅に合わせる */
        int nc = g_pend_col + (g_pend_px + g_cw - 1) / g_cw;
        ww(ds + V_COL, (uint16_t)nc);
        g_shift_next = g_pend_col * g_cw + g_pend_px - nc * g_cw;
        g_pend_col = -1;
    }
    while (g_iout < g_nout) {
        outrec *o = &g_out[g_iout++];
        if (o->kind == O_LINE) {
            int k = 0, j;
            for (j = 0; j < o->len && k < 0xa8; j++) {
                uint16_t v = g_units[o->start + j];
                if (is_jp_unit(v)) {
                    wb(buf + k++, (uint8_t)(0x80 | ((v >> 7) & 0x7f)));
                    wb(buf + k++, (uint8_t)(0x80 | (v & 0x7f)));
                } else wb(buf + k++, (uint8_t)v);
            }
            wb(buf + k, 0);
            if (!o->flag) { g_pend_col = rws(ds + V_COL); g_pend_px = o->px; }
            SET_AX(1);
            SET_DX(o->flag);
            return 0;
        } else if (o->kind == O_SETW || o->kind == O_COL) {
            ww(ds + o->addr, o->val);
        } else if (o->kind == O_SETB) {
            wb(ds + o->addr, (uint8_t)o->val);
        }
    }
    if (g_active) {
        wb(ds + V_CH, (uint8_t)g_save_ch);
        g_active = 0;
    }
    g_in_disp = 0;
    SET_AX(0);
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    if (g_cb_installed) return 1;
    for (i = 0; i < 4; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy (%x)\n", b + i, tab[b + i]); return 0; }
    tab[b + 0] = (uint32_t)cb_disp_prep;
    tab[b + 1] = (uint32_t)cb_disp_next;
    tab[b + 2] = (uint32_t)cb_print;
    tab[b + 3] = (uint32_t)cb_shaded;
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

/* ----------------------------------------------------- 差し替えファイル */
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

/* 'X' 節: u32 元の大きさ, u32 元の crc, 以降 {u32 位置, u32 長さ, バイト列} */
static uint8_t *build_file(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1];
    const uint8_t *p = sec + 8, *e = sec + slen;
    uint8_t *out;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("file mismatch\n"); return NULL; }
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

/* 'K' 節: u32 元の大きさ, u32 元の crc, u32 件数, {char 名前[16], u32 長さ, データ}
 * PAK: {u32 位置, 名前\0}... 最後に u32 ファイルの大きさ、続いて中身 */
static uint8_t *build_pak(const uint8_t *orig, uint32_t olen, const uint8_t *sec, uint32_t slen, uint32_t *outlen)
{
    uint32_t osz = ((const uint32_t*)sec)[0], ocrc = ((const uint32_t*)sec)[1], nrep = ((const uint32_t*)sec)[2];
    uint32_t offs[256], n = 0, p = 0, first, total, hdr, i, k;
    const char *names[256];
    const uint8_t *rep[256];
    uint32_t replen[256];
    uint8_t *out;
    (void)slen;
    if (osz != olen || crc32(orig, olen) != ocrc) { dbg("pak mismatch\n"); return NULL; }
    first = *(const uint32_t*)orig;
    while (p + 4 < first && n < 255) {
        offs[n] = *(const uint32_t*)(orig + p);
        if (!offs[n] || offs[n] > olen) break;
        names[n] = (const char*)orig + p + 4;
        p += 4 + (uint32_t)strlen(names[n]) + 1;
        n++;
    }
    offs[n] = olen;
    for (i = 0; i < n; i++) {
        const uint8_t *q = sec + 12;
        rep[i] = NULL;
        for (k = 0; k < nrep; k++) {
            uint32_t L = *(const uint32_t*)(q + 16);
            if (!_stricmp((const char*)q, names[i])) { rep[i] = q + 20; replen[i] = L; }
            q += 20 + L;
        }
    }
    hdr = first;
    total = hdr;
    for (i = 0; i < n; i++) total += rep[i] ? replen[i] : offs[i + 1] - offs[i];
    out = (uint8_t*)malloc(total);
    memcpy(out, orig, hdr);
    p = 0; k = hdr;
    for (i = 0; i < n; i++) {
        uint32_t L = rep[i] ? replen[i] : offs[i + 1] - offs[i];
        *(uint32_t*)(out + p) = k;
        p += 4 + (uint32_t)strlen(names[i]) + 1;
        memcpy(out + k, rep[i] ? rep[i] : orig + offs[i], L);
        k += L;
    }
    *(uint32_t*)(out + p) = total;    /* 終わりの印 */
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

static const char *patched_path(const char *orig_path, char *buf)
{
    const char *bn = base_name(orig_path);
    uint32_t slen, olen, nlen;
    const uint8_t *sx;
    uint8_t *orig, *out;
    if (!g_enabled) return NULL;
    int kind = 'X';
    sx = find_sect('X', bn, &slen);
    if (!sx) { sx = find_sect('K', bn, &slen); kind = 'K'; }
    if (!sx) return NULL;
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) return buf;
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
    out = kind == 'X' ? build_file(orig, olen, sx, slen, &nlen) : build_pak(orig, olen, sx, slen, &nlen);
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
    char la[MAX_PATH], *s;
    HMODULE crt;
    InitializeCriticalSection(&g_cs);
    GetModuleFileNameA(NULL, g_gamedir, MAX_PATH);
    s = strrchr(g_gamedir, '\\'); if (s) *s = 0;      /* ...\DOSBOX */
    s = strrchr(g_gamedir, '\\'); if (s) *s = 0;      /* ゲームフォルダ */
    if (GetEnvironmentVariableA("EOBJP_DEV", la, MAX_PATH) && la[0] == '1') g_dev = 1;
    dev_init(g_gamedir);
    if (!GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) return;
    wsprintfA(g_dir, "%s\\EOB1_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    if (g_dev) g_logon = 1;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("EOB JP patch active, blob %08X\n", g_blob_crc);
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
