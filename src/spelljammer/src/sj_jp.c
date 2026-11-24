/*
 * Spelljammer: Pirates of Realmspace 日本語化パッチ（実行時部分）
 * DOSBox 0.74-3 に winmm.dll の代理として読み込まれる。ゲームのファイルは書き換えない。
 * DOSBox がファイルを開くとき（fopen）に、次のファイルは手を入れたコピー（%LOCALAPPDATA%\SJ_JP）を渡す:
 *  - SJ.EXE / SF3.EXE / OTC.EXE（Borland C++ の実モードの EXE。どれも同じ描画ライブラリを持つ）:
 *      1 字描画の入口 … 日本語の字は自前で描き、元の処理には「描かない字（0x80）」を渡す
 *      字幅の入口     … 日本語の字は 1 バイト目 0・2 バイト目 8 を返す
 *      データ領域の文字列 … 訳（元と同じ長さ以内）で上書き（'D' 節。元の文字列はハッシュで確かめる）
 *  - ART\*.BTN（ボタン）: 文字を訳に替えて作り直す（'B' 節）
 *  - 任務の説明・噂・物語・アイテムなどの名前: 'D' 節のとおりに書き換え・置き換え
 * 日本語の文字は独自の 2 バイト符号（1 バイト目 0x80-0xBF, 2 バイト目 0xC0-0xFE）で表す。
 * 日本語の直後の空白は幅 0 で描く（長い文章では字の間に空白を入れて、ゲームの折り返しで区切れるようにしてある）。
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "resource.h"

void proxy_init(void);
#ifdef SJ_TEST
void testio_init(void);
#endif

static char g_dir[MAX_PATH];
#ifndef SJ_TEST
/* 配布版: ログは書かない（ログと画面の撮影などの試験用の仕組みは、build.bat test の試験版にだけ入る） */
#define dbg(...) ((void)0)
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
#define R_BP ((uint16_t)REG32(5))
#define R_IP ((uint16_t)REG32(8))
#define SET_AX(v) (*(uint16_t*)&REG32(0) = (uint16_t)(v))
#define SET_SP(v) (*(uint16_t*)&REG32(4) = (uint16_t)(v))
#define SET_BP(v) (*(uint16_t*)&REG32(5) = (uint16_t)(v))
#define SET_IP(v) (REG32(8) = (uint16_t)(v))
#define PHYS_CS (((uint32_t*)A_SEGPHYS)[1])
#define PHYS_SS (((uint32_t*)A_SEGPHYS)[2])
#define PHYS_DS (((uint32_t*)A_SEGPHYS)[3])
static void push16(uint16_t v) { SET_SP(R_SP - 2); ww(PHYS_SS + R_SP, v); }

/* ------------------------------------------------------- パッチデータ */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)
static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;
#define IS_LEAD(c) ((c) >= 0x80 && (c) <= 0xbf)
#define IS_TRAIL(c) ((c) >= 0xc0 && (c) <= 0xfe)
static uint16_t *g_glyph_index;
static const uint8_t *g_g8;
static int g_cb_base = 0x70;
static int g_enabled;
static const uint8_t *g_dsect; static uint32_t g_dsect_len;     /* 'D': ファイルの書き換え */
static const uint8_t *g_bsect;                                  /* 'B': ボタンの文字 */
static const uint8_t *g_csect;                                  /* 'C': コピープロテクトの答えの前後（「（答え：」0「）」0） */
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
    if (memcmp(h->magic, "SJJ1", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len);
    p = g_blob + sizeof(blob_hdr);
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == 'F') {
            uint32_t n = *(const uint32_t*)d, k;
            const uint16_t *codes = (const uint16_t*)(d + 4);
            g_g8 = d + 4 + n * 2;
            g_glyph_index = (uint16_t*)calloc(65536, 2);
            for (k = 0; k < n; k++) g_glyph_index[codes[k]] = (uint16_t)(k + 1);
        } else if (s->type == 'B') g_bsect = d;
        else if (s->type == 'C') g_csect = d;
        else if (s->type == 'D') { g_dsect = d; g_dsect_len = s->len; }
        else if (s->type == 'P') g_cb_base = (int)((const uint32_t*)d)[0];
        p = d + s->len;
    }
    return g_glyph_index != NULL;
}
static int glyph_no(uint8_t lead, uint8_t trail) { return g_glyph_index ? g_glyph_index[(lead << 8) | trail] : 0; }

/* ------------------------------------------------------------ コールバック */
/* 1 字描画（push bp / mov bp,sp / sub sp,2 の代わり。[bp+6] = 字）。
 * 関数の中の変数の番地（データ領域の中）は、EXE ごとに違うので関数の命令から読む:
 *   +0x0a フォント（far ptr）, +0x6a 画面の 1 行のバイト数, +0x71 色, +0x75 Y, +0x93/+0x97 描く先（seg/off）, +0x9b X */
static uint8_t g_lead;
static int g_dprev, g_wprev;           /* 直前に描いた字・測った字が日本語か */
static uint32_t __cdecl cb_draw(void)
{
    uint32_t ss = PHYS_SS, cs = PHYS_CS, ds = PHYS_DS;
    uint16_t fn = (uint16_t)(R_IP - 4), bp;
    uint8_t c;
    push16(R_BP);
    SET_BP(R_SP);
    SET_SP(R_SP - 2);
    bp = R_BP;
    c = rb(ss + bp + 6);
    if (IS_LEAD(c)) {
        g_lead = c;
        wb(ss + bp + 6, 0x80);
    } else if (IS_TRAIL(c)) {
        int gn = g_lead ? glyph_no(g_lead, c) : 0;
        g_lead = 0;
        if (gn) {
            uint16_t vP = rw(cs + fn + 0x6a), vC = rw(cs + fn + 0x71), vY = rw(cs + fn + 0x75);
            uint16_t vHi = rw(cs + fn + 0x93), vLo = rw(cs + fn + 0x97), vX = rw(cs + fn + 0x9b);
            uint16_t pitch = rw(ds + vP), y = rw(ds + vY), x = rw(ds + vX);
            uint8_t col = rb(ds + vC);
            uint32_t prod = (uint32_t)y * pitch;
            uint16_t ax = (uint16_t)prod, dx = (uint16_t)(prod >> 16);
            const uint8_t *g = g_g8 + (gn - 1) * 8;
            int r, k;
            dx = (uint16_t)((dx & 0xf) << 12);
            if (ax & 0x8000) { ax -= 0x8000; dx += 0x800; }
            dx = (uint16_t)(dx + rw(ds + vHi));
            ax = (uint16_t)(ax + rw(ds + vLo) + x);
            for (r = 0; r < 8; r++) {
                uint32_t base = ((uint32_t)dx << 4) + ax;
                for (k = 0; k < 8; k++) if (g[r] & (0x80 >> k)) wb(base + k, col);
                ax = (uint16_t)(ax + pitch);
                if (ax & 0x8000) { dx += 0x800; ax -= 0x8000; }
            }
            ww(ds + vX, (uint16_t)(x + 8));
        }
        wb(ss + bp + 6, 0x80);
        g_dprev = 1;
    } else {
        g_lead = 0;
        if (c == 0x20 && g_dprev) wb(ss + bp + 6, 0x80);     /* 日本語のあとの空白は幅 0 */
        else g_dprev = 0;
    }
    return 0;
}

/* 字幅（push bp / mov bp,sp / push si の代わり。[bp+6] = 字）。日本語は +0x22（pop si / pop bp / retf）へ飛ぶ */
static uint32_t __cdecl cb_width(void)
{
    uint16_t fn = (uint16_t)(R_IP - 4), bp;
    uint8_t c;
    int w = -1;
    push16(R_BP);
    SET_BP(R_SP);
    push16((uint16_t)REG32(6));
    bp = R_BP;
    c = rb(PHYS_SS + bp + 6);
    if (IS_LEAD(c)) w = 0;
    else if (IS_TRAIL(c)) { w = 8; g_wprev = 1; }
    else if (c == 0x20 && g_wprev) w = 0;
    else g_wprev = 0;
    if (w >= 0) { SET_AX(w); SET_IP(fn + 0x22); }
    return 0;
}

/* コピープロテクトの問い（SJ.EXE）: sprintf(buf, "ページ%d、行%d、語%dを入力", …) の直後（add sp,0Eh / xor si,si の代わり）。
 * 答え（STR.DAT から読んだ語）は [bp-12Eh] にある。問いの文の後ろに「（答え：…）」を足す。buf の位置は 7 バイト前の push の値 */
static uint32_t __cdecl cb_copyq(void)
{
    uint32_t ss = PHYS_SS, ds = PHYS_DS;
    uint16_t at = (uint16_t)(R_IP - 4), buf = rw(PHYS_CS + at - 7), bp = R_BP;
    uint32_t a = ds + buf, q = ss + (uint16_t)(bp - 0x12e);
    int n = 0, k;
    SET_SP(R_SP + 0x0e);
    *(uint16_t*)&REG32(6) = 0;
    if (g_csect) {
        const uint8_t *pre = g_csect, *post = pre + strlen((const char*)pre) + 1;
        while (n < 200 && rb(a + n)) n++;
        for (k = 0; pre[k]; k++) wb(a + n++, pre[k]);
        {
            char ans[24];
            for (k = 0; k < 20 && rb(q + k); k++) { ans[k] = (char)rb(q + k); wb(a + n++, rb(q + k)); }
            ans[k] = 0;
            dbg("copy protection answer: %s\n", ans);
        }
        for (k = 0; post[k]; k++) wb(a + n++, post[k]);
        wb(a + n, 0);
    }
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    static const void *fn[] = {cb_draw, cb_width, cb_copyq};
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
static const uint8_t PAT_DRAW[] = {0x55,0x8b,0xec,0x83,0xec,0x02,0x56,0x57,0xc4,0x3e,0x00,0x00,0x80,0x7e,0x06,0x0d,0x75,0x09,0xa1,0x00,0x00,0xa3,0x00,0x00,0xe9,0xbf,0x00,0x80,0x7e,0x06,0x0a,0x75,0x10,0x33,0xc0,0x26,0x8a,0x05,0x01,0x06,0x00,0x00,0xff,0x06,0x00,0x00,0xe9,0xa9,0x00,0x80,0x6e,0x06,0x20,0x80,0x7e,0x06,0x5f,0x7e,0x03,0xe9,0x9c,0x00,0x33,0xc0,0x33,0xdb,0x8a,0x5e,0x06,0x26,0x8a,0x41,0x02,0x50,0x33,0xc0,0x33,0xdb,0x33,0xc9,0x33,0xd2,0x26,0x8a,0x05,0x8a,0xc8,0x26,0x8a,0x5d,0x01,0x26,0xf6,0x65,0x01,0x8a,0x56,0x06,0xf7,0xe2,0x83,0xc7,0x62,0x03,0xf8,0xa1,0x00,0x00,0x89,0x46,0xfe,0x1e,0xa1,0x00,0x00,0x50,0xa1,0x00,0x00,0xf7,0x26,0x00,0x00,0x83,0xe2,0x0f,0xd1,0xca,0xd1,0xca,0xd1,0xca,0xd1,0xca,0x0b,0xc0,0x79,0x07,0x2d,0x00,0x80,0x81,0xc2,0x00,0x08,0x03,0x16,0x00,0x00,0x03,0x06,0x00,0x00,0x03,0x06,0x00,0x00,0x8b,0xf0,0x8e};
static const char MASK_DRAW[] = "xxxxxxxxxx??xxxxxxx??x??xxxxxxxxxxxxxxxx??xx??xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx??xxxxx??xx??xx??xxxxxxxxxxxxxxxxxxxxxxxx??xx??xx??xxx";
static const uint8_t PAT_WIDTH[] = {0x55,0x8b,0xec,0x56,0xc4,0x36,0x00,0x00,0x46,0x46,0x33,0xc0,0x33,0xdb,0x8a,0x5e,0x06,0x80,0xeb,0x20,0x26,0x8a,0x44,0xff,0x80,0xfb,0x5f,0x7f,0x03,0x26,0x8a,0x00,0xfe,0xc0,0x5e,0x5d,0xcb};
static const char MASK_WIDTH[] = "xxxxxx??xxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

/* ---- 'D' 節: ファイルごとの書き換え
 *   u32 ファイル数, {char 名前[16], u8 種類, u8 0[3], u32 元の大きさ, u64 元のハッシュ（0 = 確かめない）, u32 長さ, 中身}
 *   種類 1: その場で書き換え（u32 件数, {u32 位置, u16 元の長さ, u16 訳の長さ, u64 元のハッシュ, 訳}。余りは 0 で埋める）
 *   種類 2: ファイルごと置き換え（中身 = 新しいファイル）
 *   種類 4: 種類 1 と同じだが、ファイル全体を XOR で解いてから書き換え、また XOR する（噂）。余りは空白で埋める
 * 'B' 節: ボタンの文字 u32 件数, {u64 英語のハッシュ, u16 英語の長さ, u16 訳の長さ, 訳} */
static const uint8_t KEY[8] = {0xa1, 0x09, 0x1a, 0xfb, 0x6b, 0x55, 0x1c, 0x9a};
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
static int apply_inplace(const char *bn, uint8_t *b, uint32_t n, const uint8_t *pl, int pad)
{
    uint32_t cnt = *(const uint32_t*)pl, k, ok = 0, bad = 0;
    const uint8_t *p = pl + 4;
    for (k = 0; k < cnt; k++) {
        uint32_t off = *(const uint32_t*)p;
        uint16_t ol = *(const uint16_t*)(p + 4), jl = *(const uint16_t*)(p + 6);
        uint64_t h; memcpy(&h, p + 8, 8);
        if (off + ol <= n && jl <= ol && fnv64(b + off, ol) == h) {
            memcpy(b + off, p + 16, jl);
            memset(b + off + jl, pad, ol - jl);
            ok++;
        } else bad++;
        p += 16 + jl;
    }
    dbg("%s: %u patched, %u mismatched\n", bn, ok, bad);
    return ok > 0;
}
/* 書き換えたファイルを作る（作らないときは NULL）。b は解放してよい */
static uint8_t *build_dfile(const char *bn, const uint8_t *e, uint8_t *b, uint32_t n, uint32_t *outn)
{
    uint8_t type = e[16];
    uint32_t osz = *(const uint32_t*)(e + 20), plen = *(const uint32_t*)(e + 32), i;
    uint64_t oh; memcpy(&oh, e + 24, 8);
    if (osz != n || (oh && fnv64(b, n) != oh)) { dbg("%s: not the expected file\n", bn); return NULL; }
    if (type == 2) {
        uint8_t *o = (uint8_t*)malloc(plen ? plen : 1);
        memcpy(o, e + 36, plen);
        free(b);
        *outn = plen;
        return o;
    }
    if (type == 4) for (i = 0; i < n; i++) b[i] ^= KEY[i & 7];
    apply_inplace(bn, b, n, e + 36, type == 4 ? ' ' : 0);
    if (type == 4) for (i = 0; i < n; i++) b[i] ^= KEY[i & 7];
    *outn = n;
    return b;
}

/* EXE: 描画の入口にコールバックを置き、'D' 節の文字列を書き込む */
static uint8_t *build_exe(const char *bn, uint8_t *b, uint32_t n, uint32_t *outn)
{
    int pd = find_pat(b, n, PAT_DRAW, MASK_DRAW, (int)sizeof PAT_DRAW);
    int pw = find_pat(b, n, PAT_WIDTH, MASK_WIDTH, (int)sizeof PAT_WIDTH);
    const uint8_t *e = find_dfile(bn);
    if (pd < 0 || pw < 0) { dbg("%s: draw %d width %d not found\n", bn, pd, pw); return NULL; }
    b[pd] = 0xfe; b[pd + 1] = 0x38; b[pd + 2] = (uint8_t)g_cb_base; b[pd + 3] = 0; b[pd + 4] = 0x90; b[pd + 5] = 0x90;
    b[pw] = 0xfe; b[pw + 1] = 0x38; b[pw + 2] = (uint8_t)(g_cb_base + 1); b[pw + 3] = 0;
    {   /* コピープロテクトの問い（SJ.EXE だけにある） */
        static const uint8_t PQ[] = {0xff,0x76,0xfa,0xff,0x76,0xfc,0xff,0x76,0xfe,0x1e,0x68,0,0,0x1e,0x68,0,0,0x9a,0,0,0,0,
                                     0x83,0xc4,0x0e,0x33,0xf6,0xc6,0x86,0xec,0xfe,0x00};
        int pq = find_pat(b, n, PQ, "xxxxxxxxxxx??xx??x????xxxxxxxxxx", (int)sizeof PQ);
        if (pq >= 0) { pq += 22; b[pq] = 0xfe; b[pq + 1] = 0x38; b[pq + 2] = (uint8_t)(g_cb_base + 2); b[pq + 3] = 0; b[pq + 4] = 0x90; }
    }
    if (e && *(const uint32_t*)(e + 20) == n) apply_inplace(bn, b, n, e + 36, 0);
    *outn = n;
    return b;
}

/* ボタン（*.BTN）: レコード = 0x14 バイト + u16 w（0 でなければ +2 バイト）+ 0x26 バイト + u16 長さ + 文字列 */
static const uint8_t *btn_lookup(const uint8_t *s, int n, int *jl)
{
    const uint8_t *p;
    uint32_t cnt, k;
    uint64_t h;
    if (!g_bsect || n <= 0) return NULL;
    h = fnv64(s, n);
    cnt = *(const uint32_t*)g_bsect;
    p = g_bsect + 4;
    for (k = 0; k < cnt; k++) {
        uint64_t eh; memcpy(&eh, p, 8);
        if (eh == h && *(const uint16_t*)(p + 8) == n) { *jl = *(const uint16_t*)(p + 10); return p + 12; }
        p += 12 + *(const uint16_t*)(p + 10);
    }
    return NULL;
}
static uint8_t *build_btn(const char *bn, uint8_t *b, uint32_t n, uint32_t *outn)
{
    uint8_t *o = (uint8_t*)malloc(n * 3 + 64);
    uint32_t p = 0, q, m = 0, cnt = 0;
    while (p < n) {
        uint16_t w, len;
        const uint8_t *ja;
        int jl = 0;
        if (p + 0x16 > n) { free(o); return NULL; }
        w = *(const uint16_t*)(b + p + 0x14);
        q = p + 0x16 + (w ? 2 : 0) + 0x26;
        if (q + 2 > n) { free(o); return NULL; }
        len = *(const uint16_t*)(b + q);
        if (q + 2 + len > n) { free(o); return NULL; }
        memcpy(o + m, b + p, q - p); m += q - p;
        {   /* ファイルごとの訳（"PLAY.BTN|E^XIT" など）を先に探す */
            uint8_t key[96];
            int kl = 0, i;
            for (i = 0; bn[i] && kl < 30; i++) key[kl++] = (uint8_t)((bn[i] >= 'a' && bn[i] <= 'z') ? bn[i] - 32 : bn[i]);
            key[kl++] = '|';
            ja = NULL;
            if (kl + len <= (int)sizeof key) { memcpy(key + kl, b + q + 2, len); ja = btn_lookup(key, kl + len, &jl); }
        }
        if (!ja) ja = btn_lookup(b + q + 2, len, &jl);
        if (ja) { *(uint16_t*)(o + m) = (uint16_t)jl; memcpy(o + m + 2, ja, jl); m += 2 + jl; cnt++; }
        else { memcpy(o + m, b + q, 2 + len); m += 2 + len; }
        p = q + 2 + len;
    }
    free(b);
    dbg("%s: %u labels\n", bn, cnt);
    *outn = m;
    return o;
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
    size_t L = strlen(bn);
    int kind;
    const uint8_t *de = NULL;
    if (!_stricmp(bn, "SJ.EXE") || !_stricmp(bn, "SF3.EXE") || !_stricmp(bn, "OTC.EXE")) kind = 1;
    else if (L > 4 && !_stricmp(bn + L - 4, ".BTN") && g_bsect) kind = 2;
    else if ((de = find_dfile(bn)) != NULL) kind = 3;
    else return NULL;
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
    o = kind == 1 ? build_exe(bn, b, n, &nn) : kind == 2 ? build_btn(bn, b, n, &nn) : build_dfile(bn, de, b, n, &nn);
    if (!o) { if (kind == 1) free(b); else if (kind == 3) free(b); return NULL; }
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

static void init(HMODULE self)
{
    char la[MAX_PATH];
#ifdef SJ_TEST
    char v[8];
#endif
    HMODULE crt;
    InitializeCriticalSection(&g_cs);
    if (!GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) return;
    wsprintfA(g_dir, "%s\\SJ_JP", la);
    CreateDirectoryA(g_dir, NULL);
#ifdef SJ_TEST
    if (GetEnvironmentVariableA("SJ_LOG", v, sizeof v) && v[0] == '1') g_logon = 1;
#endif
    if (!verify_dosbox()) { dbg("unknown DOSBox\n"); return; }
    if (!load_blob(self)) { dbg("no blob\n"); return; }
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("Spelljammer JP patch active, blob %08X\n", g_blob_crc);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        init(h);
#ifdef SJ_TEST
        testio_init();
#endif
    }
    return TRUE;
}
