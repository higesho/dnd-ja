/*
 * D&D Stronghold 日本語化パッチ（実行時部分）
 * DOSBox に winmm.dll の代理として読み込まれる。ゲームのファイルは書き換えない。
 * DOSBox が STRONG.EXE を開くとき（fopen）に、手を入れたコピー（%LOCALAPPDATA%\SG_JP）を渡す。
 *  - DGROUP の英語の文字列を、同じ場所に日本語（独自の符号）で書き込む
 *  - 文字のオーバーレイ（Borland VROOMM、スタブ段 0x12c4）の 1 字描画の入口・字幅の読み出し・折り返しの判定に
 *    DOSBox のコールバック（FE 38 nn 00）を置く。日本語の字は美咲フォントで DLL が描く
 * 日本語の符号: 1 バイト（0x80-0xFF、よく使う 128 字）、2 バイト（1 バイト目 0x01-0x1F の一部 + 2 バイト目 0x80-0xFF）
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "resource.h"

void proxy_init(void);
#ifdef SG_TEST
void testio_init(void);
void testio_mouse(int x, int y);
#endif

static char g_dir[MAX_PATH];
#ifndef SG_TEST
/* 配布版: ログは書かない（ログ・画面の撮影・入力の差し込みなどの試験用の仕組みは、build.bat test の試験版にだけ入る） */
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

/* ------------------------------------------------------- 実モード（16 ビット）の補助 */
#define A_SEGVAL (0x74d100u + g_dbx)
#define PHYS_ES (((uint32_t*)A_SEGPHYS)[0])
#define R_AX ((uint16_t)REG32(0))
#define R_BX ((uint16_t)REG32(3))
#define R_SP ((uint16_t)REG32(4))
#define R_BP ((uint16_t)REG32(5))
#define R_SI ((uint16_t)REG32(6))
#define R_DI ((uint16_t)REG32(7))
#define R_IP ((uint16_t)REG32(8))
#define SET16(i, v) (*(uint16_t*)&REG32(i) = (uint16_t)(v))
#define SET_AL(v) (*(uint8_t*)&REG32(0) = (uint8_t)(v))
static __inline uint16_t rw(uint32_t a) { return (uint16_t)(rb(a) | (rb(a + 1) << 8)); }
static __inline void ww(uint32_t a, uint16_t v) { wb(a, (uint8_t)v); wb(a + 1, (uint8_t)(v >> 8)); }
static void push16(uint16_t v) { SET16(4, R_SP - 2); ww(PHYS_SS + R_SP, v); }
static uint16_t arg16(int k) { return rw(PHYS_SS + (uint16_t)(R_SP + 2 * k)); }   /* [sp + 2k] */

/* ------------------------------------------------------- パッチデータ
 * 'SGJ1', 節の数, 節 {u8 種類, u32 長さ, 中身}
 *  'G' 字形: u32 件数, {u16 符号（0x00XX = 1 バイト 0x80-0xFF, LLTT = 2 バイト）, u8 禁則, u8 0, u8 字形[8]}
 *  'L' 2 バイトの字の 1 バイト目になる値の表（u8[32] のビット）
 *  'E' EXE の書き換え: u32 大きさ, u32 件数, {u32 位置, u16 長さ, 元[長さ], 新[長さ]}
 *  'A' コピープロテクトの答えの前後（ゲームの符号、0 区切り）
 */
static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;
static const uint8_t *g_epatch, *g_ans, *g_dpatch;
static int g_cb_base = 0x70;
static int g_enabled;
static const uint8_t *g_g1[256];          /* 1 バイトの字の字形 */
static uint8_t g_f1[256];
static const uint8_t **g_g2;              /* 2 バイトの字 [lead*256+trail] */
static uint8_t *g_f2;
static uint8_t g_leadbits[32];
#define IS_LEAD(c) ((g_leadbits[(c) >> 3] >> ((c) & 7)) & 1)
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
    const uint8_t *p, *end;
    uint32_t ns, i;
    if (!r) return 0;
    g = LoadResource(self, r);
    g_blob = (const uint8_t*)LockResource(g);
    g_blob_len = SizeofResource(self, r);
    if (!g_blob || g_blob_len < 8 || memcmp(g_blob, "SGJ1", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len) ^ 0x1u;    /* 末尾の数 = DLL のコードの版 */
    g_g2 = (const uint8_t**)calloc(65536, sizeof(void*));
    g_f2 = (uint8_t*)calloc(65536, 1);
    ns = *(const uint32_t*)(g_blob + 4);
    p = g_blob + 8; end = g_blob + g_blob_len;
    for (i = 0; i < ns && p + 5 <= end; i++) {
        uint8_t t = p[0];
        uint32_t len = *(const uint32_t*)(p + 1), k;
        const uint8_t *d = p + 5;
        if (t == 'G') {
            uint32_t n = *(const uint32_t*)d;
            const uint8_t *q = d + 4;
            for (k = 0; k < n; k++, q += 12) {
                uint16_t c = *(const uint16_t*)q;
                if (c < 0x100) { g_g1[c] = q + 4; g_f1[c] = q[2]; }
                else { g_g2[c] = q + 4; g_f2[c] = q[2]; }
            }
        } else if (t == 'L') memcpy(g_leadbits, d, 32);
        else if (t == 'E') g_epatch = d;
        else if (t == 'A') g_ans = d;
        else if (t == 'D') g_dpatch = d;
        p = d + len;
    }
    return g_epatch != NULL;
}

/* ------------------------------------------------------------ コールバック（番地はオーバーレイの中の位置） */
#define DG 0x14100u                 /* 実行時の DGROUP は (PSP+0x10+0x1410)*16。DS から取る */
/* 字の符号: 1 バイト（0x80-0xFF）か、1 バイト目（IS_LEAD）+ 2 バイト目（0x80-0xFF） */
static uint8_t g_lead;

/* 0: 1 字描く（オーバーレイ 0x12c4 の +0x28c。push bp; mov bp,sp; sub sp,0ch の代わり）
 *    [sp+4] = 字, [sp+6] = 字体（far）, [sp+10] = 色。描く位置は DGROUP:[42ch] x, [42eh] y, 描き先の段 [430h] */
static uint32_t __cdecl cb_putc(void)
{
    uint8_t c = (uint8_t)arg16(2), col = (uint8_t)arg16(5);
    uint32_t ds = PHYS_DS;
    const uint8_t *g = NULL;
    uint16_t fseg = arg16(4), foff = arg16(3);
    int x, y, h, r, k;
    uint32_t dst;
    if (IS_LEAD(c)) { g_lead = c; goto skip; }
    if (c < 0x80) { g_lead = 0; goto orig; }
    if (g_lead) { g = g_g2[(g_lead << 8) | c]; g_lead = 0; }
    else g = g_g1[c];
    if (!g) goto skip;
    x = (int16_t)rw(ds + 0x42c); y = (int16_t)rw(ds + 0x42e);
    dst = (uint32_t)rw(ds + 0x430) << 4;
    h = rb(((uint32_t)fseg << 4) + foff);           /* 字体の高さ */
    y += h > 8 ? (h - 8) / 2 : 0;
    for (r = 0; r < 8; r++)
        for (k = 0; k < 8; k++) {
            if (!(g[r] & (0x80 >> k))) continue;
            if (x + k >= 0 && x + k < 320 && y + r >= 0 && y + r < 200) wb(dst + (y + r) * 320 + x + k, col);
            /* 影（右下）: 字のない所だけ */
            if (k < 7 && !(g[r] & (0x80 >> (k + 1))) && x + k + 1 < 320 && y + r < 200 && (r == 7 || !(g[r + 1] & (0x80 >> (k + 1)))))
                if (y + r + 1 < 200) wb(dst + (y + r + 1) * 320 + x + k + 1, 0xec);
        }
    /* 関数の出口（x に di を足して戻る）へ: push bp; mov bp,sp; sub sp,0ch; push si; push di の後で di = 8 */
    push16(R_BP); SET16(5, R_SP); SET16(4, R_SP - 0xc);
    push16(R_SI); push16(R_DI);
    SET16(7, 8);
    SET16(8, R_IP - 4 - 0x28c + 0x4dd);
    return 0;
skip:                                   /* 描かず、進めずに戻る（1 バイト目）: 出口の 0x4e7（pop di ...）へ */
    push16(R_BP); SET16(5, R_SP); SET16(4, R_SP - 0xc);
    push16(R_SI); push16(R_DI);
    SET16(8, R_IP - 4 - 0x28c + 0x4e7);
    return 0;
orig:
    push16(R_BP); SET16(5, R_SP); SET16(4, R_SP - 0xc);
    return 0;
}

/* 1: 字の幅（mov al, es:[bx+0a1h] の代わり）。ax = 字（符号拡張のこともある）、es:bx = 字体 + ax */
static uint32_t __cdecl cb_width(void)
{
    uint8_t c = (uint8_t)R_AX;
    if (IS_LEAD(c)) SET_AL(0);
    else if (c >= 0x80) SET_AL(8);
    else SET_AL(rb(PHYS_ES + (uint16_t)(R_BX + 0xa1)));
    return 0;
}

/* 2: 折り返しの判定（0x225da の cmp byte es:[bx+si],20h; jne +5 の代わり）
 *    空白のほか、日本語の字の後ろでも切れる（行頭・行末の禁則あり）。切れるなら続きの mov di,si+1 へ、切れないなら飛ばす */
static int kin_head(uint32_t p)          /* p の字が行頭に来られないか */
{
    uint8_t c = rb(p);
    if (IS_LEAD(c)) return g_f2[(c << 8) | rb(p + 1)] & 1;
    if (c >= 0x80) return g_f1[c] & 1;
    return c == '.' || c == ',' || c == '!' || c == '?' || c == ')' || c == ':';
}
static uint32_t __cdecl cb_wrap(void)
{
    uint32_t base = PHYS_ES + R_BX, p = base + R_SI;
    uint8_t c = rb(p);
    int brk = 0;
    if (c == 0x20) brk = 1;
    else if (c >= 0x80) {
        int fl;
        if (R_SI > 0 && IS_LEAD(rb(p - 1))) fl = g_f2[(rb(p - 1) << 8) | c];
        else fl = g_f1[c];
        brk = !(fl & 2) && !kin_head(p + 1) && rb(p + 1) != 0;
    }
    if (!brk) SET16(8, R_IP + 7);
    return 0;
}

/* 3: コピープロテクト（push ds; push 712h =「最初の単語を入力。」の代わり）。その文に（答え：語）を足して、
 *    この関数の作業領域 ss:[bp-5ah]（問いの sprintf に使い終わったもの）に置き、その far ポインタを積む。
 *    答え = DS:[54ch + 番号*4] の far ポインタ、番号 = ss:[bp-1] */
static uint32_t __cdecl cb_copy(void)
{
    uint32_t ds = PHYS_DS, ss = PHYS_SS;
    uint16_t buf = (uint16_t)(R_BP - 0x5a);
    uint32_t w = ss + buf, a;
    int n = 0, i;
    uint8_t idx = rb(ss + (uint16_t)(R_BP - 1));
    a = ((uint32_t)rw(ds + 0x54c + idx * 4 + 2) << 4) + rw(ds + 0x54c + idx * 4);
    dbg("copy: ss=%x bp=%x sp=%x idx=%d ans=%x ds=%x\n", ss, R_BP, R_SP, idx, a, ds);
    for (i = 0; n < 0x40; i++) { uint8_t c = rb(ds + 0x712 + i); if (!c) break; wb(w + n++, c); }
    if (g_ans) {
        const uint8_t *p = g_ans;
        while (*p && n < 0x48) wb(w + n++, *p++);
        for (i = 0; i < 16 && n < 0x52; i++) { uint8_t c = rb(a + i); if (c < 0x20 || c > 0x7e) break; wb(w + n++, c); }
        p++;
        while (*p && n < 0x58) wb(w + n++, *p++);
    }
    wb(w + n, 0);
    push16((uint16_t)(ss >> 4)); push16(buf);
#ifdef SG_TEST
    { char aw[20]; int q; for (q = 0; q < 16 && rb(a + q) >= 0x20 && rb(a + q) < 0x7f; q++) aw[q] = (char)rb(a + q); aw[q] = 0; dbg("copy answer [%s]\n", aw); }
#endif
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    static const void *fn[] = {cb_putc, cb_width, cb_wrap, cb_copy};
    if (g_cb_installed) return 1;
    for (i = 0; i < 4; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy\n", b + i); return 0; }
    for (i = 0; i < 4; i++) tab[b + i] = (uint32_t)(uintptr_t)fn[i];
    g_cb_installed = 1;
    dbg("callbacks installed at %x\n", b);
    return 1;
}

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
        uint8_t tmp[2048];
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
    int kind;
    if (!_stricmp(bn, "STRONG.EXE")) kind = 1;
    else if (!_stricmp(bn, "STRONG.DAT") && g_dpatch) kind = 2;
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
    if (kind == 2) {      /* STRONG.DAT: 訳を末尾に足して、目次（位置・大きさ）を書き換える */
        uint32_t cnt = ((const uint32_t*)g_dpatch)[1], i, add = 0, pos;
        const uint8_t *p = g_dpatch + 8;
        uint8_t *nb;
        if (((const uint32_t*)g_dpatch)[0] != n) { dbg("dat size mismatch\n"); free(b); return NULL; }
        for (i = 0; i < cnt; i++) { add += *(const uint32_t*)(p + 4); p += 8 + *(const uint32_t*)(p + 4); }
        nb = (uint8_t*)realloc(b, n + add);
        if (!nb) { free(b); return NULL; }
        b = nb; pos = n; p = g_dpatch + 8;
        for (i = 0; i < cnt; i++) {
            uint32_t fld = *(const uint32_t*)p, len = *(const uint32_t*)(p + 4);
            memcpy(b + pos, p + 8, len);
            memcpy(b + fld, &pos, 4); memcpy(b + fld + 4, &len, 4);
            pos += len; p += 8 + len;
        }
        n += add;
        if (!write_all(buf, b, n)) { free(b); return NULL; }
    } else if (!build_exe(b, n) || !write_all(buf, b, n)) { free(b); return NULL; }
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

static void init(HMODULE self)
{
    char la[MAX_PATH];
#ifdef SG_TEST
    char v[8];
#endif
    HMODULE crt;
    InitializeCriticalSection(&g_cs);
    if (!GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) return;
    wsprintfA(g_dir, "%s\\SG_JP", la);
    CreateDirectoryA(g_dir, NULL);
#ifdef SG_TEST
    g_logon = GetEnvironmentVariableA("SG_LOG", v, sizeof v) > 0;
    if (GetEnvironmentVariableA("SG_NOJP", v, sizeof v) > 0) return;      /* テスト用: 英語のまま */
#endif
    if (!verify_dosbox()) { dbg("unknown DOSBox build\n"); return; }
    if (!load_blob(self)) { dbg("no patch data\n"); return; }
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) { dbg("fopen import not found\n"); return; }
    g_enabled = 1;
    dbg("Stronghold JP patch active (dosbox %x), blob %08X\n", g_dbx, g_blob_crc);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        init(h);
#ifdef SG_TEST
        testio_init();
#endif
    }
    return TRUE;
}
