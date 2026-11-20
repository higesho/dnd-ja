/*
 * Al-Qadim: The Genie's Curse 日本語化パッチ（実行時部分）
 * DOSBox 0.74-3 に winmm.dll の代理として読み込まれる。ゲームのファイルは書き換えない。
 * DOSBox がファイルを開くとき（fopen）に、次のファイルは手を入れたコピー（%LOCALAPPDATA%\AQ_JP）を渡す:
 *  - GENIER.EXE（Watcom C, DOS/4G）: 次の場所に DOSBox のコールバック（FE 38 nn nn）を置く
 *      GPL の文字列を取り出した後     … 訳の表にあれば日本語に差し替える
 *      1 字描画の入口 / フォントの位置 … 日本語の字は、その字形をフォントの描画用の枠（0xFF）に書いてから描かせる
 *      折り返しの判定                 … 日本語は 1 字ごとに区切れる（行頭・行末の禁則あり）
 *  - SYSFILE.LIB: 2 つのフォント（IBM.Genie, Round。Mac の NFNT 形式）を作り直す
 *      0x80-0xBF = 日本語の 1 バイト目（幅 = 全角 1 字）, 2 バイト目（幅 0）, 0xD2/0xD3 =「」, 0xFF = 描画用の枠
 * 日本語の文字は独自の 2 バイト符号（1 バイト目 0x80-0xBF, 2 バイト目 0xC0-0xFE、0xD2/0xD3 を除く）で表す。
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "resource.h"

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

/* ------------------------------------------------ DOSBox 0.74-3 の中身 */
static uint32_t g_dbx, g_dbx_code;   /* DOSBox の版: Steam 0.74-3 = 0、GOG 0.74-2.1 = +0xe000 / -0x10 */
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
        {0x416a26u, 0x41965fu, 0x431da2u, 0x3b000u, 0xbc0u},       /* SVN 版（Ravenloft に同梱のもの） */
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
#define LOC(off) (PHYS_SS + R_EBP - (off))          /* ローカル変数 [ebp - off] */
static __inline void push32(uint32_t v) { R_ESP -= 4; wd(PHYS_SS + R_ESP, v); }
static __inline uint32_t pop32(void) { uint32_t v = rd(PHYS_SS + R_ESP); R_ESP += 4; return v; }

/* ------------------------------------------------------- パッチデータ */
#pragma pack(push, 1)
typedef struct { char magic[4]; uint32_t version; uint32_t nsect; } blob_hdr;
typedef struct { uint32_t type; uint32_t len; } sect_hdr;
#pragma pack(pop)
static const uint8_t *g_blob;
static uint32_t g_blob_len, g_blob_crc;
#define IS_LEAD(c) ((c) >= 0x80 && (c) <= 0xbf)
#define IS_TRAIL(c) ((c) >= 0xc0 && (c) <= 0xfe && (c) != 0xd2 && (c) != 0xd3 && (c) != 0xfa)
#define SLOT 0xff
static uint16_t *g_glyph_index;
static const uint8_t *g_g8, *g_g12, *g_gflags, *g_brackets, *g_cpstr, *g_sysrep;
static int g_cb_base = 0x70;
static int g_enabled;
static const uint8_t *g_epatch; static uint32_t g_epatch_len;
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
    if (memcmp(h->magic, "AQJ1", 4)) return 0;
    g_blob_crc = crc32(g_blob, g_blob_len);
    p = g_blob + sizeof(blob_hdr);
    for (i = 0; i < h->nsect; i++) {
        const sect_hdr *s = (const sect_hdr*)p;
        const uint8_t *d = p + sizeof(sect_hdr);
        if (s->type == 'F') {
            uint32_t n = *(const uint32_t*)d, k;
            const uint16_t *codes = (const uint16_t*)(d + 4);
            g_g8 = d + 4 + n * 2;
            g_g12 = g_g8 + n * 8;
            g_gflags = g_g12 + n * 24;
            g_glyph_index = (uint16_t*)calloc(65536, 2);
            for (k = 0; k < n; k++) g_glyph_index[codes[k]] = (uint16_t)(k + 1);
        } else if (s->type == 'T') {
            load_table(d);
        } else if (s->type == 'E') {
            g_epatch = d; g_epatch_len = s->len;
        } else if (s->type == 'B') {
            g_brackets = d;
        } else if (s->type == 'C') {
            g_cpstr = d;
        } else if (s->type == 'S') {
            g_sysrep = d;
        } else if (s->type == 'P') {
            const uint32_t *v = (const uint32_t*)d;
            g_cb_base = (int)v[0];
            g_logon = (int)v[1];
        }
        p = d + s->len;
    }
    return g_glyph_index && g_epatch && g_brackets;
}

/* ------------------------------------------------------------ 訳の表 */
/* 'T' 節: u32 個数, 項目 {u8 種類, u8 旗, u16 訳長, u16 英語長, u64 英語のハッシュ, 訳} */
typedef struct { uint64_t h; const uint8_t *ja; uint16_t enlen, jalen; } tentry;
static tentry *g_tr;
static uint32_t g_ntr;
static int32_t *g_hash;
static uint32_t g_hmask;
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
    while (sz < n * 2 + 2) sz <<= 1;
    g_hash = (int32_t*)calloc(sz, 4);
    g_hmask = sz - 1;
    for (i = 0; i < n; i++) {
        tentry *e = &g_tr[i];
        uint32_t h;
        e->jalen = *(const uint16_t*)(p + 2);
        e->enlen = *(const uint16_t*)(p + 4);
        memcpy(&e->h, p + 6, 8);
        p += 14;
        e->ja = p;
        p += e->jalen;
        h = (uint32_t)e->h & g_hmask;
        while (g_hash[h]) h = (h + 1) & g_hmask;
        g_hash[h] = (int32_t)i + 1;
    }
    g_ntr = n;
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
static int glyph_no(uint8_t lead, uint8_t trail) { return g_glyph_index ? g_glyph_index[(lead << 8) | trail] : 0; }

static void log_miss(const char *tag, const uint8_t *s, int n)
{
    char buf[400];
    int i;
    if (!g_logon) return;
    for (i = 0; i < n && i < 390; i++) buf[i] = (char)((s[i] >= 0x20 && s[i] < 0x7f) ? s[i] : '.');
    buf[i] = 0;
    dbg("%s [%s]\n", tag, buf);
}

/* ------------------------------------------------------------ コールバック */
#define JPBUF 2047          /* GPL の作業領域（EXE の書き換えで 2048 バイトにしてある） */

/* 0: GPL の文字列を取り出した後（mov [ebp-4],eax; mov eax,[ebp-4] の代わり）。eax = 文字列 */
/* 2 つの文字列を取る命令（代入 0x0A・一致 0x5A・部分一致 0x5B。0x2434d で取り出す）の文字列か。
 * これらは訳さない（変数の枠は 42 バイトで、訳すとあふれる。比べる相手と食い違う）。
 * 呼び出しの流れ: 0x2434d → (0x242ca →) 0x23e75 → 0x23c19（ここ）。リンク時の番地で戻り先を見る */
static int g_code_delta_ok;
static uint32_t g_code_delta;
static int in_two_operand_op(void)
{
    uint32_t f0 = R_EBP, r0, f1, r1, f2, r2;
    if (!g_code_delta_ok) { g_code_delta = R_EIP - 0x23c99; g_code_delta_ok = 1; }
    r0 = rd(PHYS_SS + f0 + 0x18) - g_code_delta;          /* 0x23c19 の戻り先 */
    f1 = rd(PHYS_SS + f0);
    r1 = rd(PHYS_SS + f1 + 0x18) - g_code_delta;          /* 0x23e75 の戻り先 */
    if (r0 != 0x23e9f) return 0;                          /* 0x23e75 から */
    if (r1 == 0x24365) return 1;                          /* 0x2434d の 2 つ目 */
    if (r1 != 0x242dd) return 0;
    f2 = rd(PHYS_SS + f1);
    r2 = rd(PHYS_SS + f2 + 0x18) - g_code_delta;          /* 0x242ca の戻り先 */
    return r2 == 0x24360;                                 /* 0x2434d の 1 つ目（0x216e5 = 台詞の表示は訳す） */
}

static uint32_t __cdecl cb_text(void)
{
    uint8_t s[0x130];
    uint32_t a = PHYS_DS + R_EAX;
    int n = 0;
    const tentry *e;
    while (n < 0x12f && (s[n] = rb(a + n)) != 0) n++;
    if (n > 0 && s[0] < 0x80 && in_two_operand_op()) { log_miss("keep (2-operand op)", s, n); n = 0; }
    if (n > 0 && s[0] < 0x80) {
        e = lookup(s, n);
        if (e) {
            int k, m = e->jalen > JPBUF ? JPBUF : e->jalen;
            for (k = 0; k < m; k++) wb(a + k, e->ja[k]);
            wb(a + m, 0);
        } else log_miss("miss", s, n);
    }
    wd(LOC(4), R_EAX);
    return 0;
}

/* 1: 1 字描画の入口（push ebx; push ecx; push edx; push esi の代わり）。eax = 字 */
static uint8_t g_lead;
static int g_pend;
static uint32_t __cdecl cb_draw(void)
{
    uint32_t c = R_EAX & 0xffff;
    if (c < 0x100 && IS_LEAD(c)) {             /* 1 バイト目: 描かずに戻る（幅は 2 バイト目で進める） */
        g_lead = (uint8_t)c;
        R_EIP = pop32();
        return 0;
    }
    if (c < 0x100 && IS_TRAIL(c)) {
        int gn = g_lead ? glyph_no(g_lead, (uint8_t)c) : 0;
        g_lead = 0;
        if (!gn) { R_EIP = pop32(); return 0; }
        g_pend = gn;
        R_EAX = SLOT;
    } else g_lead = 0;
    push32(R_EBX); push32(R_ECX); push32(R_EDX); push32(R_ESI);
    return 0;
}

/* 描画用の枠（0xFF）に字形を書く。fp = フォントの見出し（NFNT）の位置 */
static void put_glyph(uint32_t fp, int gn)
{
    uint32_t h = PHYS_DS + fp;
    int first = rw(h + 2), last = rw(h + 4), fh = rw(h + 0xe), owt = rw(h + 0x10), rowb = rw(h + 0x18) * 2;
    int n = last - first + 1, owoff = 0x10 + owt * 2, locoff = owoff - (n + 2) * 2;
    int x0, x1, w, r, x, big;
    uint32_t bits = h + 0x1a;
    if (SLOT < first || SLOT > last || fh <= 0 || fh > 20) return;
    x0 = rw(h + locoff + (SLOT - first) * 2);
    x1 = rw(h + locoff + (SLOT - first + 1) * 2);
    w = x1 - x0;
    if (w <= 0 || w > 16) return;
    big = w >= 12;
    for (r = 0; r < fh; r++) {
        uint32_t v = 0;
        if (big) { if (r < 12) v = (g_g12[(gn - 1) * 24 + r * 2] << 8) | g_g12[(gn - 1) * 24 + r * 2 + 1]; v <<= 16; }
        else if (r < 8) v = (uint32_t)g_g8[(gn - 1) * 8 + r] << 24;
        for (x = 0; x < w; x++) {
            uint32_t a = bits + r * rowb + ((x0 + x) >> 3);
            uint8_t m = (uint8_t)(0x80 >> ((x0 + x) & 7)), o = rb(a);
            wb(a, (v & (0x80000000u >> x)) ? (uint8_t)(o | m) : (uint8_t)(o & ~m));
        }
    }
}

/* 2: 1 字描画の中、フォントの位置が分かった所（mov edx,[ebp-64h]; add edx,eax の代わり） */
static uint32_t __cdecl cb_font(void)
{
    uint32_t fp = rd(LOC(0x64));
    R_EDX = fp + R_EAX;
    if (g_pend) {
        int first = rw(PHYS_DS + fp + 2);
        if (rw(LOC(0x40)) == SLOT - first) put_glyph(fp, g_pend);
        g_pend = 0;
    }
    return 0;
}

/* 3: 折り返しの 1 字ごとの判定（inc [ebp-60h]; mov eax,[ebp-5ch]; mov [ebp-3ch],eax の代わり）
 * [ebp-8] = この字, [ebp-4] = 前の字, [ebp-24h] = この字の位置, [ebp-60h] = 字数, [ebp-5ch] = ここまでの幅
 * 空白のときと同じく [ebp-48h]（行に入れる字数）と [ebp-20h]（その幅）を記録すると、そこで折り返せる */
static int gflags_at(uint32_t a)       /* a = 1 バイト目の位置 */
{
    int gn = glyph_no(rb(a), rb(a + 1));
    return gn ? g_gflags[gn - 1] : 0;
}
/* 折り返しの判定は 2 か所: 3 = 文章の窓を描く関数（0x43818）, 4 = 1 行に入る字数を数える関数（0x43b33。窓の行数を決める）
 * どちらも「inc [字数]; mov eax,[幅]; mov [幅の写し],eax」の 9 バイトをコールバックに置き換えている */
typedef struct { int cnt, w, wcopy, c, prev, ptr, bcnt, bw; } wrapvars;
static void do_wrap(const wrapvars *v)
{
    uint32_t cnt = rd(LOC(v->cnt)) + 1, wsum = rd(LOC(v->w));
    uint8_t c = rb(LOC(v->c)), prev = rb(LOC(v->prev));
    wd(LOC(v->cnt), cnt);
    R_EAX = wsum;
    wd(LOC(v->wcopy), wsum);
    if ((IS_LEAD(c) || c == '{') && prev > 0x20) {
        uint32_t p = PHYS_DS + rd(LOC(v->ptr));
        int ok = 1;
        if (IS_LEAD(c) && (gflags_at(p) & 1)) ok = 0;                  /* 行頭禁則 */
        if (prev == '{') ok = 0;                                         /*「の直後 */
        else if (IS_TRAIL(prev)) { if (gflags_at(p - 2) & 2) ok = 0; }  /* 行末禁則 */
        else if (c == '{') ok = 0;                                       /* 英字のあとの「では切らない */
        if (ok) { wd(LOC(v->bcnt), cnt - 1); wd(LOC(v->bw), wsum); }
    }
}
static uint32_t __cdecl cb_wrap(void)
{
    static const wrapvars v = {0x60, 0x5c, 0x3c, 8, 4, 0x24, 0x48, 0x20};
    do_wrap(&v);
    return 0;
}
static uint32_t __cdecl cb_wrap2(void)
{
    static const wrapvars v = {0x1c, 0x2c, 0x34, 4, 8, 0x24, 0x28, 0x20};
    do_wrap(&v);
    return 0;
}

static int g_cb_installed;
static int install_callbacks(void)
{
    uint32_t *tab = (uint32_t*)A_CB_HANDLERS;
    int b = g_cb_base, i;
    static const void *fn[] = {cb_text, cb_draw, cb_font, cb_wrap, cb_wrap2};
    if (g_cb_installed) return 1;
    for (i = 0; i < 5; i++)
        if (tab[b + i] != A_CB_ILLEGAL) { dbg("callback slot %x busy\n", b + i); return 0; }
    for (i = 0; i < 5; i++) tab[b + i] = (uint32_t)(uintptr_t)fn[i];
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

/* GENIER.EXE: 'E' 節の書き換え（元のバイトが合わなければ作らない） */
static uint8_t *build_exe(uint8_t *b, uint32_t n, uint32_t *outn)
{
    const uint8_t *p = g_epatch + 8;
    uint32_t cnt = ((const uint32_t*)g_epatch)[1], i;
    if (((const uint32_t*)g_epatch)[0] != n) { dbg("exe size mismatch\n"); return NULL; }
    for (i = 0; i < cnt; i++) {
        uint32_t off = *(const uint32_t*)p;
        uint8_t len = p[4], kind = p[5], k = p[6];
        const uint8_t *o = p + 7, *nw = o + len;
        if (off + len > n || memcmp(b + off, o, len)) { dbg("exe bytes mismatch at %x\n", off); return NULL; }
        if (kind == 1) {
            uint8_t cbb[4] = {0xfe, 0x38, (uint8_t)(g_cb_base + k), 0};
            memset(b + off, 0x90, len);
            memcpy(b + off, cbb, 4);
        } else memcpy(b + off, nw, len);
        p = nw + len;
    }
    *outn = n;
    return b;
}

/* NFNT（小さな位置は little endian）を作り直す。pre = 名前などの前置きの長さ */
static int font_w(int fh) { return fh >= 12 ? 12 : 8; }
static uint8_t *build_font(const uint8_t *b, uint32_t blen, uint32_t *outn)
{
    int nl = b[0], k = *(const uint16_t*)(b + 1 + nl), pre = 1 + nl + 2 * k;
    const uint16_t *H = (const uint16_t*)(b + pre);
    int first = H[1], last = H[2], fh = H[7], owt = H[8], rw0 = H[12];
    int n = last - first + 1, owoff = 0x10 + owt * 2, locoff = owoff - (n + 2) * 2;
    const uint16_t *loc = (const uint16_t*)(b + pre + locoff), *ow = (const uint16_t*)(b + pre + owoff);
    const uint8_t *bits = b + pre + 0x1a;
    int W = font_w(fh), c, x, r, total = 0, nrw, nn = 256;
    int iw[257], sx[257], src[257];
    uint16_t now[258], nloc[258];
    uint8_t *out, *nb;
    uint32_t sz;
    if (first != 0 || last < 0x7e || (uint32_t)(pre + owoff + (n + 2) * 2) > blen) return NULL;
    for (c = 0; c <= 256; c++) {
        int oc = c == 256 ? n : c;                    /* 256 = 字がないときの字形 */
        src[c] = 0; iw[c] = 0; sx[c] = 0;
        if (c == 256 || ((c < 0x80 || c == 0xfa) && c <= last)) {
            iw[c] = loc[oc + 1] - loc[oc]; sx[c] = loc[oc]; src[c] = 1; now[c] = ow[oc];
        } else if (c < 0x80) { now[c] = 0xffff; }
        else if (IS_LEAD(c)) now[c] = (uint16_t)W;
        else if (c == 0xd2 || c == 0xd3) { iw[c] = W; src[c] = 2 + (c - 0xd2); now[c] = (uint16_t)W; }
        else if (c == SLOT) { iw[c] = W; now[c] = (uint16_t)W; }
        else now[c] = 0;
        nloc[c] = (uint16_t)total;
        total += iw[c];
    }
    nloc[257] = (uint16_t)total;
    now[257] = 0xffff;
    nrw = (total + 15) / 16;
    sz = pre + 0x1a + nrw * 2 * fh + 258 * 2 * 2;
    out = (uint8_t*)calloc(1, sz);
    memcpy(out, b, pre + 0x1a);
    {
        uint16_t *NH = (uint16_t*)(out + pre);
        int nlocoff = 0x1a + nrw * 2 * fh, nowoff = nlocoff + 258 * 2;
        NH[2] = 0xff;
        if (NH[3] < W) NH[3] = (uint16_t)W;
        if (NH[6] < W) NH[6] = (uint16_t)W;
        NH[8] = (uint16_t)((nowoff - 0x10) / 2);
        NH[12] = (uint16_t)nrw;
        memcpy(out + pre + nlocoff, nloc, 258 * 2);
        memcpy(out + pre + nowoff, now, 258 * 2);
    }
    nb = out + pre + 0x1a;
    for (c = 0; c <= 256; c++) {
        for (r = 0; r < fh; r++) for (x = 0; x < iw[c]; x++) {
            int on = 0;
            if (src[c] == 1) { int sxx = sx[c] + x; on = (bits[r * rw0 * 2 + (sxx >> 3)] >> (7 - (sxx & 7))) & 1; }
            else if (src[c] >= 2) {
                int which = src[c] - 2;
                if (W == 8) on = r < 8 && ((g_brackets[which * 8 + r] >> (7 - x)) & 1);
                else if (r < 12) { const uint8_t *g = g_brackets + 16 + which * 24 + r * 2; on = ((g[0] << 8 | g[1]) >> (15 - x)) & 1; }
            }
            if (on) { int dx = nloc[c] + x; nb[r * nrw * 2 + (dx >> 3)] |= (uint8_t)(0x80 >> (dx & 7)); }
        }
    }
    (void)nn;
    *outn = sz;
    return out;
}

static int is_font(const uint8_t *b, uint32_t n)
{
    int nl, k, pre, i;
    if (n < 64) return 0;
    nl = b[0];
    if (nl < 1 || nl > 31) return 0;
    for (i = 1; i <= nl; i++) if (b[i] < 0x20 || b[i] >= 0x7f) return 0;
    k = *(const uint16_t*)(b + 1 + nl);
    pre = 1 + nl + 2 * k;
    if ((uint32_t)pre + 0x1a > n) return 0;
    return *(const uint16_t*)(b + pre) == 0x9000;
}

/* SYSFILE.LIB: u32 n, n x {u32 位置, u32 長さ}。フォントを作り直して末尾に足し、目次を書き換える */
static uint8_t *build_lib(uint8_t *b, uint32_t n, uint32_t *outn)
{
    uint32_t cnt = *(uint32_t*)b, i, total = n;
    uint8_t *newe[64] = {0};
    uint32_t newn[64] = {0};
    uint8_t *out;
    if (cnt > 64 || 4 + cnt * 8 > n) return NULL;
    for (i = 0; i < cnt; i++) {
        uint32_t o = ((uint32_t*)(b + 4))[i * 2], s = ((uint32_t*)(b + 4))[i * 2 + 1];
        if (o + s > n) return NULL;
        if (g_sysrep) {           /* 'S' 節: 差し替える項目（冒頭の物語など） */
            const uint8_t *q = g_sysrep + 4;
            uint32_t k, m = *(const uint32_t*)g_sysrep;
            for (k = 0; k < m; k++) {
                uint32_t idx = ((const uint32_t*)q)[0], l = ((const uint32_t*)q)[1];
                if (idx == i) {
                    newe[i] = (uint8_t*)malloc(l ? l : 1); memcpy(newe[i], q + 8, l); newn[i] = l;
                    total = (total + 3) & ~3u; total += l;
                }
                q += 8 + l;
            }
            if (newe[i]) continue;
        }
        if (is_font(b + o, s)) {
            newe[i] = build_font(b + o, s, &newn[i]);
            if (newe[i]) { total = (total + 3) & ~3u; total += newn[i]; dbg("font %d rebuilt %u -> %u\n", i, s, newn[i]); }
        }
    }
    out = (uint8_t*)calloc(1, total);
    memcpy(out, b, n);
    total = n;
    for (i = 0; i < cnt; i++) if (newe[i]) {
        total = (total + 3) & ~3u;
        memcpy(out + total, newe[i], newn[i]);
        ((uint32_t*)(out + 4))[i * 2] = total;
        ((uint32_t*)(out + 4))[i * 2 + 1] = newn[i];
        total += newn[i];
        free(newe[i]);
    }
    free(b);
    *outn = total;
    return out;
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
    int kind;
    uint32_t n, nn;
    uint8_t *b, *o;
    if (!_stricmp(bn, "GENIER.EXE")) kind = 1;
    else if (!_stricmp(bn, "SYSFILE.LIB")) kind = 2;
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
    o = kind == 1 ? build_exe(b, n, &nn) : build_lib(b, n, &nn);
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

static void init(HMODULE self)
{
    char la[MAX_PATH];
    HMODULE crt;
    InitializeCriticalSection(&g_cs);
    if (!GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) return;
    wsprintfA(g_dir, "%s\\AQ_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    crt = GetModuleHandleA("msvcrt.dll");
    if (!crt) return;
    real_fopen = (fopen_t)GetProcAddress(crt, "fopen");
    if (!real_fopen) return;
    if (!hook_iat()) return;
    g_enabled = 1;
    dbg("Al-Qadim JP patch active, blob %08X, %u entries\n", g_blob_crc, g_ntr);
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
