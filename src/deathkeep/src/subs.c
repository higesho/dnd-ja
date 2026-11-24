/*
 * ムービーの日本語字幕（語りのあるムービーだけ）
 * ゲームは Smacker の SmackToBuffer でゲームの画面バッファ（320x200 8 ビット）へ直接展開し、SmackDoFrame で 1 コマずつ描く。
 *  - SmackDoFrame の前: 前のコマで字幕の下にあった画素を戻す（展開は変わった所しか書き換えないため）
 *  - SmackDoFrame の後: 字幕の帯の画素を取っておき、コマの時刻に合う字幕を下の方に描く（白字に黒の縁）
 * ムービーは幅・高さ・コマ数で見分ける。色は SmackBuffer のパレット（ゲームの表示パレット）から一番明るい・暗い色を選ぶ。
 *
 * 画面バッファは 2 枚あって 1 コマごとに入れ替わり、ゲームは展開の前に「前のコマの絵（字幕入り）」を今のバッファへ写す。
 * だから戻すのは「直前のコマで控えた画素」。v1.0 は「同じバッファに 2 コマ前に控えた画素」を戻していたので、
 * 字幕の帯だけ絵が 2 コマ前に戻り、動きのある場面でちらついた。
 * 写さない作りの場合にも合うように、今のバッファの帯が「字幕を描いた直後の帯」と同じ控えを探して、それを戻す。
 */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "resource.h"

#ifdef DK_TEST
void dlog(const char *fmt, ...);
#else
#define dlog(...) ((void)0)   /* 配布版: ログは書かない */
#endif
void jp_draw_raw(int x, int y, int color, const char *utf8);
int jp_width_raw(const char *utf8);
uint8_t *jp_frame_buffer(int *pitch);
uint8_t *find_code(const uint16_t *pat, int n);

typedef struct { uint32_t s, e, off; } cue_t;
typedef struct { uint32_t w, h, frames, n; const cue_t *c; } vid_t;
static vid_t g_vid[16];
static int g_nvid;
static const uint8_t *g_res;
static uint8_t **g_psbuf;   /* SmackBuffer へのポインタを持つ変数（パレットは +0x38 から 4 バイトずつ B,G,R,x） */

#define SM(b, o) (*(int*)((uint8_t*)(b) + (o)))
/* Smack: +4 Width, +8 Height, +0xC Frames, +0x10 MSPerFrame（負なら 1/100 ms 単位）, +0x370 FrameNum（この版の SMACKW32。ゲームも Frames - [+0x370] で最後のコマを見ている） */

static const vid_t *find_vid(void *s)
{
    int i;
    if (!s) return NULL;
    for (i = 0; i < g_nvid; i++)
        if ((uint32_t)SM(s, 4) == g_vid[i].w && (uint32_t)SM(s, 8) == g_vid[i].h && (uint32_t)SM(s, 0xC) == g_vid[i].frames) return &g_vid[i];
    return NULL;
}

/* 字幕の帯の控え: px = 字幕を描く前の画素, after = 描いた後の画素（どのバッファに今その字幕が載っているかを見分ける） */
typedef struct { void *s; int y0, rows, w; unsigned seq; uint8_t px[320 * 64], after[320 * 64]; } save_t;
static save_t g_sv[2];
static unsigned g_seq;

#define LINE_H 13
#define MARGIN_B 4

static void colors(int *white, int *black)
{
    int i, bw = -1, bb = 1 << 30;
    const uint8_t *pal = (g_psbuf && *g_psbuf) ? *g_psbuf + 0x38 : NULL;
    *white = 255; *black = 0;
    if (!pal) return;
    for (i = 0; i < 256; i++) {
        int v = pal[i * 4] + pal[i * 4 + 1] * 2 + pal[i * 4 + 2];
        if (v > bw) { bw = v; *white = i; }
        if (v < bb) { bb = v; *black = i; }
    }
}

static void draw_cue(const char *t, int w, int h)
{
    char line[256];
    int nl = 1, y, white, black;
    const char *p;
    for (p = t; *p; p++) if (*p == '\n') nl++;
    y = h - MARGIN_B - nl * LINE_H + 1;
    colors(&white, &black);
    for (p = t; ; ) {
        const char *q = strchr(p, '\n');
        int n = q ? (int)(q - p) : (int)strlen(p), x, ox, oy;
        if (n > 255) n = 255;
        memcpy(line, p, n); line[n] = 0;
        x = (w - jp_width_raw(line)) / 2;
        for (oy = -1; oy <= 1; oy++)
            for (ox = -1; ox <= 1; ox++)
                if (ox || oy) jp_draw_raw(x + ox, y + oy, black, line);
        jp_draw_raw(x + 1, y + 1, black, line);
        jp_draw_raw(x, y, white, line);
        y += LINE_H;
        if (!q) break;
        p = q + 1;
    }
}

static int text_top(const char *t, int h)
{
    int nl = 1;
    for (; *t; t++) if (*t == '\n') nl++;
    return h - MARGIN_B - nl * LINE_H + 1 - 2;
}

/* 今のバッファの帯が、控え sv の「字幕を描いた後」と同じか */
static int band_matches(const save_t *sv, const uint8_t *buf, int pitch)
{
    int y;
    for (y = 0; y < sv->rows; y++)
        if (memcmp(buf + (sv->y0 + y) * pitch, sv->after + y * sv->w, sv->w)) return 0;
    return 1;
}

static unsigned (__cdecl *o_doframe)(void *);   /* 古い SMACKW32 は cdecl */
static unsigned __cdecl h_doframe(void *s)
{
    const vid_t *v = find_vid(s);
    int pitch = 0, i;
    uint8_t *buf = jp_frame_buffer(&pitch);
    unsigned r;
    if (v && buf) {
        /* 今のバッファに載っている字幕を消す: その字幕を描いたときの控えを、新しいものから探して戻す */
        save_t *hit = NULL;
        for (i = 0; i < 2; i++) {
            save_t *sv = &g_sv[i];
            if (!sv->rows || sv->s != s) continue;
            if (hit && hit->seq > sv->seq) continue;
            if (band_matches(sv, buf, pitch)) hit = sv;
        }
        if (hit) {
            int y;
            if (hit->seq != g_seq) dlog("subs: restored an older band (seq %u, latest %u)\n", hit->seq, g_seq);
            for (y = 0; y < hit->rows; y++) memcpy(buf + (hit->y0 + y) * pitch, hit->px + y * hit->w, hit->w);
            hit->rows = 0;
        } else if ((g_sv[0].rows && g_sv[0].s == s && g_sv[0].seq == g_seq) || (g_sv[1].rows && g_sv[1].s == s && g_sv[1].seq == g_seq))
            dlog("subs: frame %d: the last subtitle band is not in this buffer\n", SM(s, 0x370));
    }
    r = o_doframe(s);
    if (v && buf && v->w <= 320) {
        int mspf100 = SM(s, 0x10) > 0 ? SM(s, 0x10) * 100 : -SM(s, 0x10);
        unsigned ms = (unsigned)((__int64)SM(s, 0x370) * mspf100 / 100);
        uint32_t k;
        for (k = 0; k < v->n; k++)
            if (ms >= v->c[k].s && ms < v->c[k].e) {
                const char *t = (const char*)g_res + v->c[k].off;
                int y0 = text_top(t, v->h), y;
                save_t *sv = (g_sv[0].rows && (!g_sv[1].rows || g_sv[1].seq < g_sv[0].seq)) ? &g_sv[1] : &g_sv[0];   /* 空いている方か古い方 */
                if (!g_sv[0].rows) sv = &g_sv[0];
                if (y0 < 0) y0 = 0;
                if ((int)(v->h - y0) * (int)v->w <= (int)sizeof sv->px) {
                    for (y = y0; y < (int)v->h; y++) memcpy(sv->px + (y - y0) * v->w, buf + y * pitch, v->w);
                    sv->s = s; sv->y0 = y0; sv->rows = v->h - y0; sv->w = v->w; sv->seq = ++g_seq;
                    draw_cue(t, v->w, v->h);
                    for (y = y0; y < (int)v->h; y++) memcpy(sv->after + (y - y0) * v->w, buf + y * pitch, v->w);
                }
                break;
            }
    }
    return r;
}

/* IAT の中の、値が orig のものを差し替える（smackw32 は番号で取り込まれている） */
static void *iat_replace_ptr(HMODULE mod, const char *dll, void *orig, void *repl)
{
    uint8_t *base = (uint8_t*)mod;
    IMAGE_NT_HEADERS *nh = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *id = (IMAGE_IMPORT_DESCRIPTOR*)(base + dd->VirtualAddress);
    void *res = NULL;
    for (; id->Name; id++) {
        IMAGE_THUNK_DATA *ft;
        if (_stricmp((const char*)(base + id->Name), dll)) continue;
        for (ft = (IMAGE_THUNK_DATA*)(base + id->FirstThunk); ft->u1.Function; ft++) {
            DWORD old;
            if ((void*)ft->u1.Function != orig) continue;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
            ft->u1.Function = (DWORD)(uintptr_t)repl;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            res = orig;
        }
    }
    return res;
}

#define W 0x100
/* mov eax,[smk]; cmp dword [eax+68h],0; je; push 0; add eax,6Ch; push eax; mov ecx,[sbuf] */
static const uint16_t PAT_NEWPAL[] = { 0xa1,W,W,W,W, 0x83,0x78,0x68,0x00, 0x74,W, 0x6a,0x00, 0x83,0xc0,0x6c, 0x50, 0x8b,0x0d,W,W,W,W };

void subs_init(HMODULE self)
{
    HRSRC r = FindResourceA(self, MAKEINTRESOURCEA(IDR_SUBS), (LPCSTR)RT_RCDATA);
    HMODULE sm = GetModuleHandleA("smackw32.dll");
    uint8_t *q;
    uint32_t i, nv;
    void *df;
    if (!r || !sm) { dlog("subs: no resource / smackw32\n"); return; }
    g_res = (const uint8_t*)LockResource(LoadResource(self, r));
    if (!g_res || memcmp(g_res, "DKSB", 4)) { dlog("subs: bad resource\n"); return; }
    nv = *(uint32_t*)(g_res + 4);
    for (i = 0; i < nv && i < 16; i++) {
        const uint32_t *h = (const uint32_t*)(g_res + 8 + i * 20);
        g_vid[i].w = h[0]; g_vid[i].h = h[1]; g_vid[i].frames = h[2]; g_vid[i].n = h[3];
        g_vid[i].c = (const cue_t*)(g_res + h[4]);
    }
    g_nvid = nv < 16 ? nv : 16;
    q = find_code(PAT_NEWPAL, sizeof PAT_NEWPAL / 2);
    if (q) g_psbuf = *(uint8_t***)(q + 19);
    df = (void*)GetProcAddress(sm, "_SmackDoFrame@4");
    if (!df) df = (void*)GetProcAddress(sm, "_SmackDoFrame");
    if (!df) df = (void*)GetProcAddress(sm, MAKEINTRESOURCEA(17));
    if (!df) { dlog("subs: SmackDoFrame not found\n"); return; }
    o_doframe = (unsigned (__cdecl*)(void*))df;
    if (!iat_replace_ptr(GetModuleHandleA(NULL), "smackw32.dll", df, (void*)h_doframe)) { dlog("subs: SmackDoFrame import not found\n"); return; }
    dlog("subs: %d movies, sbuf var %p\n", g_nvid, g_psbuf);
}
