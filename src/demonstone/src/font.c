/* 日本語の字をフォント（.XFT）に足す
   XFT（ファイルの先頭からの相対位置で並ぶ。読み込み時にゲームが絶対番地に直す）:
     +0 'XF', u16 1, u32 ファイルの大きさ, u16 区画の数, u16 行の高さ H, u32 区画表の位置, u32 字形画像の位置
     区画表: {u32 字数, u32 最初の字, u32 字の表の位置}（字の検索は区画を順に見る: 最初の字 <= c < 最初の字 + 字数）
     字の表: 1 字 20 バイト {u32 x0, y0, x1, y1（右下を含む）, u16 送り幅（= 字形の幅）, u16 高さ}
     字形画像: Xbox 形式の見出し 0x80 バイト（+4 幅, +8 高さ, +0x3c Size 欄, +0x40 データの大きさ）+ 線形 A8（幅 x 高さ）
   作り直したフォント: 元の区画・字の表をそのまま写し、日本語の字の区画を足す。字形画像は幅 2048 にして、
   元の画像を左上に写し、その下に日本語の字（バンドルした Noto Serif JP を GDI で描く）を並べる */
#include "dst.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

static HANDLE g_fontres[1];
static const WCHAR *g_face[1] = { L"DSTJP Serif" };
static const int g_weight[1] = { 600 };

static void load_fonts(void)
{
    int i;
    for (i = 0; i < 1; i++) {
        DWORD n, cnt = 0;
        const void *p;
        if (g_fontres[i]) continue;
        p = res_get(3 + i, &n);
        if (p) g_fontres[i] = AddFontMemResourceEx((void *)p, n, NULL, &cnt);
        dlog("font res %d: %p (%u)", i, g_fontres[i], cnt);
    }
}

#pragma pack(push, 1)
typedef struct { uint32_t x0, y0, x1, y1; uint16_t adv, h; } Rec;
typedef struct { uint32_t count, first, recs; } Sec;
#pragma pack(pop)

/* 元の字の 'H' の一番下の行 = ベースライン */
static int find_baseline(const uint8_t *tex, int tw, int th, const uint8_t *f, int nsec, const Sec *sec, int H)
{
    int s, x, y, last = -1;
    for (s = 0; s < nsec; s++) {
        if ('H' >= (int)sec[s].first && 'H' < (int)(sec[s].first + sec[s].count)) {
            const Rec *r = (const Rec *)(f + sec[s].recs) + ('H' - sec[s].first);
            for (y = (int)r->y0; y <= (int)r->y1 && y < th; y++)
                for (x = (int)r->x0; x <= (int)r->x1 && x < tw; x++)
                    if (tex[y * tw + x] > 0x80) last = y - (int)r->y0;
            break;
        }
    }
    return last >= 0 ? last + 1 : H * 4 / 5;
}

int font_build(const uint8_t *src, uint32_t n, uint8_t **out, uint32_t *outn)
{
    int nsec, H, ncs, nadd = 0, i, k, em, cellw, base, ebase, ty, kind;
    uint32_t secoff, texoff, tw, th, W, TH, x, y, nnew = 0, nrec_old = 0, recs_at, texat, total, rows;
    const Sec *sec;
    const uint8_t *tex;
    const uint16_t *cs;
    uint16_t *add;
    Sec *nsecs;
    Rec *nrecs;
    uint8_t *f, *pix;
    HDC dc;
    HFONT hf, old;
    TEXTMETRICW tm;
    MAT2 m2 = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    uint8_t *gbuf = NULL;
    DWORD gcap = 0;
    int gstart[256], gcount[256], ng = 0;
    Rec blank;

    if (n < 0x20 || src[0] != 'X' || src[1] != 'F') return 0;
    nsec = *(const uint16_t *)(src + 8);
    H = *(const uint16_t *)(src + 0xa);
    secoff = *(const uint32_t *)(src + 0xc);
    texoff = *(const uint32_t *)(src + 0x10);
    if (nsec <= 0 || nsec > 64 || secoff + nsec * 12 > n || texoff + 0x80 > n || H <= 0 || H > 128) return 0;
    sec = (const Sec *)(src + secoff);
    tw = *(const uint32_t *)(src + texoff + 4);
    th = *(const uint32_t *)(src + texoff + 8);
    if (texoff + 0x80 + tw * th > n || tw > 2048) return 0;
    tex = src + texoff + 0x80;
    for (i = 0; i < nsec; i++) {
        if (sec[i].recs + sec[i].count * sizeof(Rec) > n) return 0;
        nrec_old += sec[i].count;
    }

    cs = data_charset(&ncs);
    if (!cs || !ncs) return 0;
    add = (uint16_t *)malloc(ncs * 2);
    for (i = 0; i < ncs; i++) {
        int have = 0, s;
        for (s = 0; s < nsec; s++) if (cs[i] >= sec[s].first && cs[i] < sec[s].first + sec[s].count) { have = 1; break; }
        if (!have) add[nadd++] = cs[i];
    }
    { char e[16]; if (g_test && GetEnvironmentVariableA("DSTJP_FONTMAX", e, sizeof e) && atoi(e) < nadd) nadd = atoi(e); }
    /* 足す字を区画に分ける（字の番号が 32 以内で続くものは 1 つの区画にまとめ、すき間は空の字にする） */
    for (i = 0; i < nadd; i++) {
        if (ng > 0 && add[i] - (gstart[ng - 1] + gcount[ng - 1]) <= 32) { gcount[ng - 1] = add[i] - gstart[ng - 1] + 1; continue; }
        if (ng == 256) { nadd = i; break; }
        gstart[ng] = add[i]; gcount[ng] = 1; ng++;
    }
    for (i = 0; i < ng; i++) nnew += gcount[i];

    load_fonts();
    kind = 0;
    em = (int)floor(H * 0.80 + 0.5);
    cellw = em + 1;
    W = 2048;
    { char e[16]; if (g_test && GetEnvironmentVariableA("DSTJP_FONTW", e, sizeof e)) W = (uint32_t)atoi(e); }
    if (W < tw) W = tw;
    rows = (nadd + (W / cellw) - 1) / (W / cellw);
    TH = th + rows * H + 2;
    if (TH > 4096) { dlog("font: too many glyphs (%d)", nadd); free(add); return 0; }

    /* 出力の並び */
    recs_at = 0x14 + (nsec + ng) * 12;
    texat = recs_at + (nrec_old + nnew) * sizeof(Rec) + 0x80;
    texat = ((texat + 0xfff) & ~0xfffu) - 0x80;          /* 元と同じく画素が 4KB 境界に来るように */
    total = texat + 0x80 + W * TH;
    total = (total + 0xfff) & ~0xfffu;
    f = (uint8_t *)calloc(1, total);
    memcpy(f, src, 0x14);
    *(uint32_t *)(f + 4) = total;
    *(uint16_t *)(f + 8) = (uint16_t)(nsec + ng);
    *(uint32_t *)(f + 0xc) = 0x14;
    *(uint32_t *)(f + 0x10) = texat;
    nsecs = (Sec *)(f + 0x14);
    nrecs = (Rec *)(f + recs_at);
    k = 0;
    for (i = 0; i < nsec; i++) {
        nsecs[i] = sec[i];
        nsecs[i].recs = recs_at + k * sizeof(Rec);
        memcpy(nrecs + k, src + sec[i].recs, sec[i].count * sizeof(Rec));
        k += sec[i].count;
    }
    for (i = 0; i < ng; i++) {
        nsecs[nsec + i].count = gcount[i];
        nsecs[nsec + i].first = gstart[i];
        nsecs[nsec + i].recs = recs_at + k * sizeof(Rec);
        k += gcount[i];
    }
    /* 字形画像の見出し: 元を写して大きさを直す */
    memcpy(f + texat, src + texoff, 0x80);
    *(uint32_t *)(f + texat + 4) = W;
    *(uint32_t *)(f + texat + 8) = TH;
    *(uint32_t *)(f + texat + 0x3c) = ((W / 64 - 1) << 24) | ((TH - 1) << 12) | (W - 1);
    *(uint32_t *)(f + texat + 0x40) = W * TH;
    pix = f + texat + 0x80;
    for (y = 0; y < th; y++) memcpy(pix + y * W, tex + y * tw, tw);

    base = find_baseline(tex, tw, th, src, nsec, sec, H);
    dc = CreateCompatibleDC(NULL);
    hf = CreateFontW(-em, 0, 0, 0, g_weight[kind], 0, 0, 0, SHIFTJIS_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
                     ANTIALIASED_QUALITY, DEFAULT_PITCH, g_face[kind]);
    old = (HFONT)SelectObject(dc, hf);
    GetTextMetricsW(dc, &tm);
    ty = (H - em) / 2;
    ebase = ty + tm.tmAscent - tm.tmInternalLeading / 2;
    if (ebase < base) ebase = base;
    if (ebase > H - 1) ebase = H - 1;

    {   /* 空白（' '）の字の表。無ければ最初の字 */
        int s2;
        blank = *(const Rec *)(src + sec[0].recs);
        for (s2 = 0; s2 < nsec; s2++)
            if (' ' >= (int)sec[s2].first && ' ' < (int)(sec[s2].first + sec[s2].count)) { blank = *((const Rec *)(src + sec[s2].recs) + (' ' - sec[s2].first)); break; }
    }
    x = 0; y = th + 2;
    for (i = 0; i < ng; i++) {
        int c;
        Rec *r0 = (Rec *)(f + nsecs[nsec + i].recs);
        for (c = 0; c < gcount[i]; c++) {
            WCHAR ch = (WCHAR)(gstart[i] + c);
            int used = 0, j;
            Rec *r = r0 + c;
            GLYPHMETRICS gm;
            DWORD sz;
            for (j = 0; j < nadd; j++) if (add[j] == ch) { used = 1; break; }
            r->h = (uint16_t)H;
            if (!used) { *r = blank; continue; }   /* 幅 0 の字はゲームが扱えないので空白の字を写す */
            if (x + cellw > W) { x = 0; y += H; }
            sz = GetGlyphOutlineW(dc, ch, GGO_GRAY8_BITMAP, &gm, 0, NULL, &m2);
            if (sz != GDI_ERROR && sz) {
                int gx, gy, ox, oy, pitch;
                if (sz > gcap) { gcap = sz + 4096; gbuf = (uint8_t *)realloc(gbuf, gcap); }
                GetGlyphOutlineW(dc, ch, GGO_GRAY8_BITMAP, &gm, sz, gbuf, &m2);
                pitch = (gm.gmBlackBoxX + 3) & ~3;
                ox = gm.gmptGlyphOrigin.x;
                oy = ebase - gm.gmptGlyphOrigin.y;
                for (gy = 0; gy < (int)gm.gmBlackBoxY; gy++)
                    for (gx = 0; gx < (int)gm.gmBlackBoxX; gx++) {
                        int px = ox + gx, py = oy + gy, a;
                        if (px < 0 || px >= cellw || py < 0 || py >= H) continue;
                        a = gbuf[gy * pitch + gx] * 255 / 64;
                        if (a > 255) a = 255;
                        if (a) pix[(y + py) * W + x + px] = (uint8_t)a;
                    }
            }
            r->x0 = x; r->y0 = y; r->x1 = x + cellw - 1; r->y1 = y + H - 1; r->adv = (uint16_t)cellw;
            x += cellw;
        }
    }
    dlog("font: H %d em %d base %d/%d, %d glyphs in %d sections, tex %ux%u -> %ux%u", H, em, base, ebase, nadd, ng, tw, th, W, TH);
    SelectObject(dc, old);
    DeleteObject(hf);
    DeleteDC(dc);
    free(gbuf);
    free(add);
    *out = f;
    *outn = total;
    return 1;
}

/* ---------------------------------------------------------------- 差し替え
   フォントは .wad の決まった大きさの読み込み領域にほかの資源と一緒に読まれるので、ファイルを大きくすると
   あふれて読み込みが止まる。そこでファイルはそのままにし、ゲームがフォントを組み立てる関数（0x44a6a0:
   this, データ, 0。表の位置を絶対番地に直し、字形画像を作る）に入るところで、データを作り直したものに替える。
   組み立てるとゲームが中身（表の位置、字形画像の見出しの画素の位置や D3D の字形画像）を書き換えるので、
   読み込み元の番地ごとに別の写しを渡す。同じ番地に同じフォントが読まれたら、前のものはもう捨てられているので
   その写しを作りたての状態に戻して使い回す */
typedef struct { uint64_t h; uint8_t *p; uint32_t n, head; } Master;   /* 作り直したフォント（head = 表と見出しの大きさ） */
typedef struct { const uint8_t *d; int m; uint8_t *p; } Inst;
static Master g_master[16];
static int g_nmaster;
static Inst g_inst[64];
static int g_ninst;
static CRITICAL_SECTION g_fcs;
static uint32_t g_fx_back;

static uint64_t fnv64b(const uint8_t *b, uint32_t n)
{
    uint64_t h = 14695981039346656037ull;
    while (n--) h = (h ^ *b++) * 1099511628211ull;
    return h;
}

static uint8_t *read_w(const WCHAR *path, uint32_t *n)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD sz, got = 0;
    uint8_t *b;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(f, NULL);
    b = (uint8_t *)malloc(sz ? sz : 1);
    if (!ReadFile(f, b, sz, &got, NULL) || got != sz) { CloseHandle(f); free(b); return NULL; }
    CloseHandle(f);
    *n = sz;
    return b;
}

/* 元のデータ d に対応する作り直したフォントの番号（無ければ作る）。日本語の要らないもの・作れないものは -1 */
static int get_master(const uint8_t *d, uint32_t n)
{
    uint64_t h = fnv64b(d, n);
    uint32_t outn = 0;
    uint8_t *out;
    WCHAR path[MAX_PATH];
    int i;
    for (i = 0; i < g_nmaster; i++) if (g_master[i].h == h) return g_master[i].p ? i : -1;
    if (g_nmaster == 16) return -1;
    g_master[g_nmaster].h = h;
    g_master[g_nmaster].p = NULL;
    i = g_nmaster++;
    {   /* 数字だけのフォント（fontNumbers）には足さない */
        int s, ns = *(const uint16_t *)(d + 8), cnt = 0;
        uint32_t so = *(const uint32_t *)(d + 0xc);
        for (s = 0; s < ns && so + s * 12 + 12 <= n; s++) cnt += (int)*(const uint32_t *)(d + so + s * 12);
        if (cnt < 64) return -1;
    }
    swprintf(path, MAX_PATH, L"%s%08X_font_%016llX.xft", work_dir(), files_stamp(), (unsigned long long)h);
    out = read_w(path, &outn);
    if (out && (outn < 0x20 || out[0] != 'X' || *(uint32_t *)(out + 4) != outn)) { free(out); out = NULL; }
    if (!out) {
        if (!font_build(d, n, &out, &outn)) return -1;
        files_write(path, out, outn);
    }
    g_master[i].p = out;
    g_master[i].n = outn;
    g_master[i].head = *(const uint32_t *)(out + 0x10) + 0x80;
    dlog("font %016llX: %u -> %u bytes", (unsigned long long)h, n, outn);
    return i;
}

/* d にまだ元のフォント（ハッシュ h）が残っているか */
static int same_font(const uint8_t *d, uint64_t h)
{
    MEMORY_BASIC_INFORMATION mi;
    uint32_t n;
    if (!VirtualQuery(d, &mi, sizeof mi) || mi.State != MEM_COMMIT || (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    if ((const uint8_t *)mi.BaseAddress + mi.RegionSize < d + 8) return 0;
    if (d[0] != 'X' || d[1] != 'F') return 0;
    n = *(const uint32_t *)(d + 4);
    if (n < 0x20 || (const uint8_t *)mi.BaseAddress + mi.RegionSize < d + n) return 0;
    return fnv64b(d, n) == h;
}

static const uint8_t *__cdecl font_subst(const uint8_t *d)
{
    uint32_t n;
    int m, i;
    uint8_t *p;
    char e[8];
    if (!d || d[0] != 'X' || d[1] != 'F' || *(const uint16_t *)(d + 2) != 1) return d;
    if (g_test && GetEnvironmentVariableA("DSTJP_NOFONT", e, sizeof e)) return d;
    n = *(const uint32_t *)(d + 4);
    if (n < 0x20 || n > 0x4000000) return d;
    EnterCriticalSection(&g_fcs);
    m = get_master(d, n);
    if (m < 0) { LeaveCriticalSection(&g_fcs); return d; }
    for (i = 0; i < g_ninst; i++)
        if (g_inst[i].d == d && g_inst[i].m == m) {
            memcpy(g_inst[i].p, g_master[m].p, g_master[m].head);   /* 表と見出しを作りたてに戻す */
            LeaveCriticalSection(&g_fcs);
            return g_inst[i].p;
        }
    /* 前に渡した写しのうち、読み込み元がもう別のもので上書きされているもの（= そのフォントは捨てられた）を使い回す */
    for (i = 0; i < g_ninst; i++)
        if (g_inst[i].m == m && !same_font(g_inst[i].d, g_master[m].h)) {
            if (g_test) dlog("font copy %d reused for %p (was %p)", i + 1, d, g_inst[i].d);
            g_inst[i].d = d;
            memcpy(g_inst[i].p, g_master[m].p, g_master[m].head);
            LeaveCriticalSection(&g_fcs);
            return g_inst[i].p;
        }
    if (g_ninst == 64) { dlog("font: too many copies"); LeaveCriticalSection(&g_fcs); return d; }
    /* 字形画像の位置が元と同じく 4KB 境界に来るよう、ページ単位で取る */
    p = (uint8_t *)VirtualAlloc(NULL, g_master[m].n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) { LeaveCriticalSection(&g_fcs); return d; }
    memcpy(p, g_master[m].p, g_master[m].n);
    g_inst[g_ninst].d = d;
    g_inst[g_ninst].m = m;
    g_inst[g_ninst].p = p;
    g_ninst++;
    if (g_test) dlog("font copy %d: master %d for %p", g_ninst, m, d);
    LeaveCriticalSection(&g_fcs);
    return p;
}

static __declspec(naked) void fx_stub(void)
{
    __asm {
        push ecx
        push dword ptr [esp + 8]
        call font_subst
        add esp, 4
        pop ecx
        mov dword ptr [esp + 4], eax
        mov eax, dword ptr [esp + 4]     /* 元の最初の 2 命令 */
        push ebx
        jmp dword ptr [g_fx_back]
    }
}

void font_init(void)
{
    /* 0x44a6a0: mov eax,[esp+4]; push ebx; push esi; mov esi,ecx; mov [esi],eax; push edi; add [eax+0Ch],eax; mov ecx,[esi]; add [ecx+10h],eax */
    static const uint8_t sig[] = { 0x8b, 0x44, 0x24, 0x04, 0x53, 0x56, 0x8b, 0xf1, 0x89, 0x06, 0x57, 0x01, 0x40, 0x0c,
                                   0x8b, 0x0e, 0x01, 0x41, 0x10, 0x8b, 0x0e, 0x8b, 0x79, 0x0c };
    uint32_t at = find_code(0x44a6a0, sig, "xxxxxxxxxxxxxxxxxxxxxxxx");
    InitializeCriticalSection(&g_fcs);
    if (!at) { dlog("font: unexpected code"); return; }
    g_fx_back = at + 5;
    patch_jmp(at, (void *)fx_stub);
}
