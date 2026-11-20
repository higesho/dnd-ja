/*
 * DragonStrike 日本語化パッチ（実行時部分）
 * DOSBox 0.74-3 に winmm.dll の代理として読み込まれる。ゲームのファイルは書き換えない。
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "resource.h"
#define PATCH_CODE_VER 2

void proxy_init(void);
void testio_init(void);

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

/* ------------------------------------------------ DOSBox の中身 */
static uint32_t g_dbx, g_dbx_code;   /* DOSBox の版: Steam 0.74-3 = 0、GOG 0.74-2.1 = +0xe000 / -0x10、SVN 版 = +0x3b000 / +0xbc0 */
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
        {0x415e66u, 0x41897fu, 0x4351ccu, 0u, 0u},
        {0x415e56u, 0x41896fu, 0x4351bcu, 0xe000u, 0xfffffff0u},
        {0x416a26u, 0x41965fu, 0x431da2u, 0x3b000u, 0xbc0u},
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
#define R_AX ((uint16_t)REG32(0))
#define R_SP ((uint16_t)REG32(4))
#define R_IP ((uint16_t)REG32(8))
#define SET_AX(v) (*(uint16_t*)&REG32(0) = (uint16_t)(v))
#define SET_SP(v) (*(uint16_t*)&REG32(4) = (uint16_t)(v))
#define SET_IP(v) (REG32(8) = (uint16_t)(v))
#define PHYS_ES (((uint32_t*)A_SEGPHYS)[0])
#define PHYS_CS (((uint32_t*)A_SEGPHYS)[1])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
#define R_DX ((uint16_t)REG32(2))
#define R_BP ((uint16_t)REG32(5))
static void push16(uint16_t v) { SET_SP(R_SP - 2); ww(PHYS_SS + R_SP, v); }

/* ------------------------------------------------------- パッチデータ（リソース IDR_PATCH、tools/make_patch.py が作る）
 * 'DSJ1', 節の数, 節 {u32 種類, u32 長さ, 中身}
 *  'G' 字形: u32 件数, u16 符号[件数]（0x00XX = 1 バイトの字 0x80-0xFE、LLTT = 2 バイトの字）, u8 字形[件数][8]
 *  'D' ファイルの書き換え（Spelljammer と同じ形。種類 1 = その場で書き換え、2 = 置き換え）
 *  'P' u32 コールバックの番号の始め
 *  'A' コピープロテクトの答えの前後の文字列（「（答え：」0「）」0、ゲームの符号で）
 */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)
static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;
static uint16_t *g_gidx;          /* 符号 -> 字形の番号 + 1 */
static const uint8_t *g_g8;
static int g_cb_base = 0x70;
static int g_enabled;
static const uint8_t *g_dsect;
static const uint8_t *g_asect;
#define IS_LEAD(c) ((c) >= 0x01 && (c) <= 0x1f && (c) != 0x0a && (c) != 0x0d)
static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xffffffffu, k;
    while (n--) { c ^= *p++; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1))); }
    return ~c;
}
static uint64_t fnv64(const uint8_t *p, int n)
{
    uint64_t h = 14695981039346656037ull;
    while (n--) h = (h ^ *p++) * 1099511628211ull;
    return h;
}
static int load_blob(HMODULE self)
{
    HRSRC r = FindResourceA(self, MAKEINTRESOURCEA(IDR_PATCH), (LPCSTR)RT_RCDATA);
    const blob_hdr *h;
    const uint8_t *p;
    uint32_t i;
    if (!r) return 0;
    g_blob = (const uint8_t*)LockResource(LoadResource(self, r));
    g_blob_len = SizeofResource(self, r);
    if (!g_blob || g_blob_len < sizeof(blob_hdr)) return 0;
    h = (const blob_hdr*)g_blob;
    if (memcmp(h->magic, "DSJ1", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len) ^ (PATCH_CODE_VER * 0x9e3779b9u);   /* 作ったファイルの名前に使う（DLL の作りが変わったら作り直す） */
    p = g_blob + sizeof(blob_hdr);
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == 'G') {
            uint32_t n = *(const uint32_t*)d, k;
            const uint16_t *codes = (const uint16_t*)(d + 4);
            g_g8 = d + 4 + n * 2;
            g_gidx = (uint16_t*)calloc(65536, 2);
            for (k = 0; k < n; k++) g_gidx[codes[k]] = (uint16_t)(k + 1);
        } else if (s->type == 'D') g_dsect = d;
        else if (s->type == 'A') g_asect = d;
        else if (s->type == 'P') g_cb_base = (int)((const uint32_t*)d)[0];
        p = d + s->len;
    }
    return g_gidx != NULL;
}

/* ------------------------------------------------------------ 字を描く（VGA.EXE の 1 字描画の代わり） */
/* VGA.EXE の変数（ゲームと同じデータ領域）: [0x116] 桁, [0x117] 行, [0x118] 地の色（0 = 透明・影付き）, [0x128] 字の色,
 * [0x14b] 描く頁（3 以上は 0）, [0x149e + 頁*2] 頁のセグメント, [0x1dcc] 1 行の桁数。画面は 320 x 200 の 1 バイト 1 画素 */
static uint8_t g_lead;
static uint8_t g_pix_lead[3];          /* 1 字描画（桁・Y）の頁ごとの 1 バイト目 */
static const uint8_t *glyph_of(uint16_t code)
{
    uint16_t k = g_gidx ? g_gidx[code] : 0;
    return k ? g_g8 + (k - 1) * 8 : NULL;
}
static void put_glyph(uint32_t ds, const uint8_t *g, int x, int y)
{
    uint8_t page = rb(ds + 0x14b), fg = rb(ds + 0x128), bg = rb(ds + 0x118);
    uint32_t base;
    int r, k;
    if (page >= 3) page = 0;
    base = (uint32_t)rw(ds + 0x149e + page * 2) << 4;
    if (bg == 0) {                     /* 透明: 1 画素下に影（色 0x0c）を付ける */
        for (r = 0; r < 8; r++)
            if (y + 1 + r < 200)
                for (k = 0; k < 8; k++) if (g[r] & (0x80 >> k)) wb(base + (y + 1 + r) * 320 + x + k, 0x0c);
        for (r = 0; r < 8; r++)
            if (y + r < 200)
                for (k = 0; k < 8; k++) if (g[r] & (0x80 >> k)) wb(base + (y + r) * 320 + x + k, fg);
    } else {
        for (r = 0; r < 8; r++)
            if (y + r < 200)
                for (k = 0; k < 8; k++) wb(base + (y + r) * 320 + x + k, (g[r] & (0x80 >> k)) ? fg : bg);
    }
}
/* 日本語の字なら描いて 1 を返す（lead だけのときも 1）。描いた字の幅は 1 桁 */
static int jp_char(uint32_t ds, uint8_t c, int x, int y, int *advance)
{
    const uint8_t *g = NULL;
    *advance = 0;
    if (g_lead) {
        uint8_t l = g_lead;
        g_lead = 0;
        g = glyph_of((uint16_t)((l << 8) | c));
        if (!g) return 0;
    } else if (IS_LEAD(c) && g_gidx) {
        g_lead = c;
        return 1;
    } else if (c >= 0x80) {
        g = glyph_of(c);
        if (!g) return 0;
    } else return 0;
    put_glyph(ds, g, x, y);
    *advance = 1;
    return 1;
}
/* 0x155c（1 字、桁・行で）: push di/si/es/ax の代わり。ax = 字 */
static uint32_t __cdecl cb_cell(void)
{
    uint32_t ds = PHYS_DS;
    uint16_t fn = (uint16_t)(R_IP - 4);
    uint8_t col = rb(ds + 0x116), row = rb(ds + 0x117);
    int adv;
    if (jp_char(ds, (uint8_t)R_AX, col * 8, row * 8, &adv)) {
        if (adv) {
            col++;
            if (col >= rb(ds + 0x1dcc)) { col = 0; wb(ds + 0x117, (uint8_t)(row + 1)); }
            wb(ds + 0x116, col);
        }
        SET_IP(fn + 0x6c);             /* retf */
        return 0;
    }
    push16((uint16_t)REG32(7)); push16((uint16_t)REG32(6)); push16((uint16_t)(PHYS_ES >> 4)); push16(R_AX);
    return 0;
}
/* メッセージ欄（MAIN の 20 字の欄）: 呼んだところ（戻り先の IP）で見分ける。欄の表の位置も MAIN.EXE を作るときに読む */
static uint16_t g_bar_ret1, g_bar_ret2, g_bar_tab;
/* ゲームの符号の並び（n バイト）を、桁 col0 から幅 w の真ん中に詰めて描く（空白は前後を除く） */
static void draw_centered(uint32_t ds, const uint8_t *s, int n, int col0, int w, int y)
{
    const uint8_t *gl[40];
    uint8_t asc[40][8];
    int k = 0, i, a = 0, b = n, x;
    while (a < b && s[a] == ' ') a++;
    while (b > a && s[b - 1] == ' ') b--;
    for (i = a; i < b && k < 40; i++) {
        uint8_t c = s[i];
        if (IS_LEAD(c) && i + 1 < b) { gl[k++] = glyph_of((uint16_t)((c << 8) | s[i + 1])); i++; }
        else if (c >= 0x80) gl[k++] = glyph_of(c);
        else {
            uint32_t f = ((uint32_t)rw(ds + 0x138) << 4) + c * 8;
            int r;
            for (r = 0; r < 8; r++) asc[k][r] = rb(f + r);
            gl[k] = asc[k]; k++;
        }
    }
    x = (col0 + (w - k) / 2) * 8;
    for (i = 0; i < k; i++, x += 8) if (gl[i]) put_glyph(ds, gl[i], x, y);
}

/* 0x14e2（1 字、桁と Y の画素で。dx = Y）: push di/si/es/ax の代わり */
static uint32_t __cdecl cb_pix(void)
{
    uint32_t ds = PHYS_DS;
    uint16_t fn = (uint16_t)(R_IP - 4);
    uint8_t col = rb(ds + 0x116), c = (uint8_t)R_AX, page = rb(ds + 0x14b);
    const uint8_t *g = NULL;
    int x = col * 8;
    uint16_t ret = rw(PHYS_SS + R_SP);
    if (g_bar_tab && (ret == g_bar_ret1 || ret == g_bar_ret2) && col >= 10 && col < 30) {
        /* メッセージ欄の字: 最後の桁で 20 バイトを読んで詰めて描く。それまでは桁を進めるだけ */
        if (col == 29) {
            uint8_t s[20];
            uint32_t cs = (uint32_t)rw(PHYS_SS + R_SP + 2) << 4, base = cs + g_bar_tab + (uint16_t)((uint16_t)REG32(3) - 19);
            int i;
            for (i = 0; i < 20; i++) s[i] = rb(base + i);
            draw_centered(ds, s, 20, 10, 20, R_DX);
        }
        wb(ds + 0x116, (uint8_t)(col + 1));
        SET_IP(fn + 0x32);
        return 0;
    }
    /* メッセージ欄は 1 バイトごとに「空白（裏・表の頁）→ 字（裏・表の頁）」と描き、1 バイト 1 桁で進むものとして桁を戻す。
       2 バイトの字は頁ごとに 1 バイト目を覚え（間の空白では忘れない）、1 バイト目は何も描かずに 1 桁進め、
       2 バイト目で 2 桁の真ん中に描く */
    if (page >= 3) page = 0;
    if (c == 0x20) goto orig;
    if (g_pix_lead[page]) {
        uint8_t l = g_pix_lead[page];
        g_pix_lead[page] = 0;
        g = glyph_of((uint16_t)((l << 8) | c));
        x = col * 8 - 4;
        if (!g) goto orig;
    } else if (IS_LEAD(c) && g_gidx) {
        g_pix_lead[page] = c;
        wb(ds + 0x116, (uint8_t)(col + 1));
        SET_IP(fn + 0x32);
        return 0;
    } else if (c >= 0x80) {
        g = glyph_of(c);
        if (!g) goto orig;
    } else goto orig;
    put_glyph(ds, g, x < 0 ? 0 : x, R_DX);
    wb(ds + 0x116, (uint8_t)(col + 1));
    SET_IP(fn + 0x32);                 /* retf */
    return 0;
orig:
    push16((uint16_t)REG32(7)); push16((uint16_t)REG32(6)); push16((uint16_t)(PHYS_ES >> 4)); push16(R_AX);
    return 0;
}

/* ------------------------------------------------------------ コピープロテクトの答え */
/* 1 文字（ゲームの符号）を桁・行の位置に描く。ASCII はゲームのフォント（[0x138] のセグメント、1 字 8 バイト） */
static void draw_text(uint32_t ds, int col, int row, const uint8_t *s, int n)
{
    int i, adv;
    for (i = 0; i < n; i++) {
        uint8_t c = s[i];
        if (jp_char(ds, c, col * 8, row * 8, &adv)) { col += adv; continue; }
        if (c >= 0x20 && c < 0x80) {
            uint8_t g[8];
            uint32_t f = ((uint32_t)rw(ds + 0x138) << 4) + c * 8;
            int r;
            for (r = 0; r < 8; r++) g[r] = rb(f + r);
            put_glyph(ds, g, col * 8, row * 8);
            col++;
        }
    }
}
/* MAIN の問いを出したところ（mov ax,cs:[bp+2] の代わり）。bp = 問いの記録（序数, 段落, u16 記号, u16 答え） */
static uint32_t __cdecl cb_copy(void)
{
    uint32_t cs = PHYS_CS, ds = PHYS_DS;
    uint16_t bp = R_BP, ans = rw(cs + (uint16_t)(bp + 4));
    uint8_t buf[80];
    int n = 0, k;
    char dbgans[24];
    SET_AX(rw(cs + (uint16_t)(bp + 2)));
    if (!g_asect) return 0;
    {
        const uint8_t *pre = g_asect, *post = pre + strlen((const char*)pre) + 1;
        uint8_t fg = rb(ds + 0x128);
        for (k = 0; pre[k] && n < 40; k++) buf[n++] = pre[k];
        for (k = 0; k < 20; k++) {
            uint8_t c = (uint8_t)(5 - rb(cs + (uint16_t)(ans + k)));
            if (c < 'a' || c > 'z') break;
            buf[n++] = c; dbgans[k] = (char)c;
        }
        dbgans[k] = 0;
        for (k = 0; post[k] && n < 78; k++) buf[n++] = post[k];
        wb(ds + 0x128, 0x0f);
        draw_text(ds, 11, 15, buf, n);
        wb(ds + 0x128, fg);
        dbg("copy protection answer: %s\n", dbgans);
    }
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    static const void *fn[] = {cb_cell, cb_pix, cb_copy};
    if (g_cb_installed) return 1;
    for (i = 0; i < 3; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy\n", b + i); return 0; }
    for (i = 0; i < 3; i++) tab[b + i] = (uint32_t)(uintptr_t)fn[i];
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

/* ------------------------------------------------------------ ファイルを作る */
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
/* 型（? = どれでもよい）で探す。ちょうど 1 か所のときだけ返す */
static int find_pat(const uint8_t *b, uint32_t n, const uint8_t *pat, const char *mask, int len)
{
    uint32_t i;
    int hit = -1, cnt = 0, k;
    for (i = 0; i + len <= n; i++) {
        for (k = 0; k < len; k++) if (mask[k] != '?' && b[i + k] != pat[k]) break;
        if (k == len) { hit = (int)i; cnt++; }
    }
    return cnt == 1 ? hit : -1;
}
/* VGA.EXE: 1 字描画（桁・行）と（桁・Y） */
static const uint8_t PAT_CELL[] = {0x57,0x56,0x06,0x50,0x53,0x51,0x52,0x1e,0x80,0x3e,0x18,0x01,0x00,0x75,0x1f,0x8b,0xd8,0x32,0xe4,
                                   0xa0,0x17,0x01,0xd1,0xe0,0xd1,0xe0,0xd1,0xe0,0x40,0x8b,0xd0};
static const uint8_t PAT_PIX[] = {0x57,0x56,0x06,0x50,0x53,0x51,0x52,0x1e,0x80,0x3e,0x18,0x01,0x00,0x75,0x14,0x50,0x53,0x52,0x42,0x8b,0xd8};
/* MAIN.EXE: 問いの記録から記号の番号を取るところ（mov ax,cs:[bp+2] / mov cs:[..],ax / mov ax,cs:[bp+4] / mov cs:[..],ax） */
static const uint8_t PAT_COPY[] = {0x2e,0x8b,0x46,0x02,0x2e,0xa3,0,0,0x2e,0x8b,0x46,0x04,0x2e,0xa3,0,0};

/* 'D' 節: u32 ファイル数, {char 名前[16], u8 種類, u8 0[3], u32 元の大きさ, u64 元のハッシュ（0 = 確かめない）, u32 長さ, 中身}
 *   種類 1: その場で書き換え（u32 件数, {u32 位置, u16 元の長さ, u16 訳の長さ, u64 元のハッシュ, 訳}。余りは 0 で埋める）
 *   種類 2: ファイルごと置き換え */
static const uint8_t *find_dfile(const char *bn)
{
    const uint8_t *p;
    uint32_t n, i;
    if (!g_dsect) return NULL;
    n = *(const uint32_t*)g_dsect;
    p = g_dsect + 4;
    for (i = 0; i < n; i++) {
        uint32_t plen = *(const uint32_t*)(p + 32);
        if (!_strnicmp((const char*)p, bn, 16)) return p;
        p += 36 + plen;
    }
    return NULL;
}
static int apply_inplace(const char *bn, uint8_t *b, uint32_t n, const uint8_t *pl)
{
    uint32_t cnt = *(const uint32_t*)pl, k, ok = 0, bad = 0;
    const uint8_t *p = pl + 4;
    for (k = 0; k < cnt; k++) {
        uint32_t off = *(const uint32_t*)p;
        uint16_t ol = *(const uint16_t*)(p + 4), jl = *(const uint16_t*)(p + 6);
        uint64_t h; memcpy(&h, p + 8, 8);
        if (off + ol <= n && jl <= ol && fnv64(b + off, ol) == h) {
            memcpy(b + off, p + 16, jl);
            memset(b + off + jl, 0, ol - jl);
            ok++;
        } else bad++;
        p += 16 + jl;
    }
    dbg("%s: %u patched, %u mismatched\n", bn, ok, bad);
    return ok > 0;
}
/* MAIN.EXE のメッセージ欄の場所を読む（作ったコピーを使うときも） */
static void scan_main(const uint8_t *b, uint32_t n)
{
    /* mov al,cs:[bx+表] / ... / lcall [6Ch] / dec [116h] / mov [14Bh],1 / lcall [6Ch] */
    static const uint8_t PB[] = {0x2e,0x8a,0x87,0,0,0xc6,0x06,0x18,0x01,0x00,0xc6,0x06,0x4b,0x01,0x02,0x2e,0x8a,0x26,0,0,
                                 0x88,0x26,0x28,0x01,0xff,0x1e,0x6c,0x00,0xfe,0x0e,0x16,0x01,0xc6,0x06,0x4b,0x01,0x01,0xff,0x1e,0x6c,0x00};
    int pb = find_pat(b, n, PB, "xxx??xxxxxxxxxxxxx??xxxxxxxxxxxxxxxxxxxxx", (int)sizeof PB);
    uint32_t hdr = n > 10 ? (uint32_t)(b[8] | (b[9] << 8)) * 16 : 0;
    if (pb < 0 || (uint32_t)pb < hdr) { dbg("MAIN.EXE: message bar not found\n"); return; }
    g_bar_tab = (uint16_t)(b[pb + 3] | (b[pb + 4] << 8));
    g_bar_ret1 = (uint16_t)(pb + 0x1c - hdr);
    g_bar_ret2 = (uint16_t)(pb + 0x29 - hdr);
    dbg("message bar: table %04x ret %04x %04x\n", g_bar_tab, g_bar_ret1, g_bar_ret2);
}
static uint8_t *build_file(const char *bn, uint8_t *b, uint32_t n, uint32_t *outn)
{
    const uint8_t *e = find_dfile(bn);
    if (!_stricmp(bn, "VGA.EXE")) {
        int pc = find_pat(b, n, PAT_CELL, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", (int)sizeof PAT_CELL);
        int pp = find_pat(b, n, PAT_PIX, "xxxxxxxxxxxxxxxxxxxxx", (int)sizeof PAT_PIX);
        if (pc < 0 || pp < 0) { dbg("VGA.EXE: cell %d pix %d not found\n", pc, pp); return NULL; }
        b[pc] = 0xfe; b[pc + 1] = 0x38; b[pc + 2] = (uint8_t)g_cb_base; b[pc + 3] = 0;
        b[pp] = 0xfe; b[pp + 1] = 0x38; b[pp + 2] = (uint8_t)(g_cb_base + 1); b[pp + 3] = 0;
        *outn = n;
        return b;
    }
    if (!e) return NULL;
    {
        uint8_t type = e[16];
        uint32_t osz = *(const uint32_t*)(e + 20), plen = *(const uint32_t*)(e + 32);
        uint64_t oh; memcpy(&oh, e + 24, 8);
        if (osz != n || (oh && fnv64(b, n) != oh)) { dbg("%s: not the expected file\n", bn); return NULL; }
        if (type == 2) {
            uint8_t *o = (uint8_t*)malloc(plen ? plen : 1);
            memcpy(o, e + 36, plen);
            free(b);
            *outn = plen;
            return o;
        }
        apply_inplace(bn, b, n, e + 36);
        if (!_stricmp(bn, "MAIN.EXE")) scan_main(b, n);
        if (!_stricmp(bn, "MAIN.EXE")) {
            /* 問いは 2 か所（始めとゲームの途中）。どちらにも置く */
            uint32_t i;
            int cnt = 0, k;
            for (i = 0; i + sizeof PAT_COPY <= n; i++) {
                for (k = 0; k < (int)sizeof PAT_COPY; k++) if (k != 6 && k != 7 && k != 14 && k != 15 && b[i + k] != PAT_COPY[k]) break;
                if (k < (int)sizeof PAT_COPY) continue;
                b[i] = 0xfe; b[i + 1] = 0x38; b[i + 2] = (uint8_t)(g_cb_base + 2); b[i + 3] = 0;
                cnt++;
            }
            dbg("MAIN.EXE: copy question callbacks %d\n", cnt);
        }
        *outn = n;
        return b;
    }
}

/* ------------------------------------------------------------ fopen の差し替え */
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
    uint32_t n, nn;
    uint8_t *b, *o;
    if (_stricmp(bn, "VGA.EXE") && !find_dfile(bn)) return NULL;
    wsprintfA(buf, "%s\\%08X_%s", g_dir, g_blob_crc, bn);
    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) {
        if (!_stricmp(bn, "MAIN.EXE") && !g_bar_tab && (b = read_all(buf, &n)) != NULL) { scan_main(b, n); free(b); }
        return buf;
    }
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
    o = build_file(bn, b, n, &nn);
    if (!o) { free(b); return NULL; }
    if (!write_all(buf, o, nn)) { free(o); return NULL; }
    free(o);
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

/* ------------------------------------------------------------ 調べもの（DS_TEST=1） */
/* 共有メモリ "DS_DUMP" の [0] に 1 を書くと、DOS のメモリ 1MB を %LOCALAPPDATA%\DS_JP\mem.bin に書き出す（[1] CS, [2] DS の物理番地） */
static DWORD WINAPI dump_thread(LPVOID p)
{
    HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 16, "DS_DUMP");
    volatile int32_t *f;
    (void)p;
    if (!h) return 0;
    f = (volatile int32_t*)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 16);
    if (!f) return 0;
    for (;;) {
        Sleep(100);
        if (f[0] == 1) {
            char path[MAX_PATH];
            FILE *o;
            uint32_t a;
            wsprintfA(path, "%s\\mem.bin", g_dir);
            o = fopen(path, "wb");
            if (o) { for (a = 0; a < 0x100000; a++) fputc(rb(a), o); fclose(o); }
            f[1] = (int32_t)PHYS_CS; f[2] = (int32_t)PHYS_DS;
            f[0] = 2;
        }
    }
}

static void init(HMODULE self)
{
    char la[MAX_PATH], v[8];
    (void)self;
    if (!GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) return;
    wsprintfA(g_dir, "%s\\DS_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (GetEnvironmentVariableA("DS_LOG", v, sizeof v) && v[0] == '1') g_logon = 1;
    InitializeCriticalSection(&g_cs);
    if (!verify_dosbox()) { dbg("unknown DOSBox\n"); return; }
    if (GetEnvironmentVariableA("DS_TEST", v, sizeof v) && v[0] == '1') CreateThread(NULL, 0, dump_thread, NULL, 0, NULL);
    if (GetEnvironmentVariableA("DS_NOJP", v, sizeof v) && v[0] == '1') return;
    if (!load_blob(self)) { dbg("no blob\n"); return; }
    {
        HMODULE crt = GetModuleHandleA("msvcrt.dll");
        if (!crt) return;
        real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
        if (!real_fopen || !hook_iat()) return;
    }
    g_enabled = 1;
    dbg("DragonStrike JP patch active, blob %08X\n", g_blob_crc);
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
