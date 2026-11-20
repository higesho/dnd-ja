/* Text drawing hooks.
   temple.dll's tig_font_draw (0x101EAF30) and tig_font_measure (0x101EA4E0) are redirected by TPlite to its own
   TextLayouter in temple.exe. Once TPlite has done that we redirect them again: strings that contain Japanese
   (EUC-JP) are laid out and drawn here with DirectWrite/Direct2D on the current D3D11 render target, everything
   else goes on to TPlite unchanged. */
#define COBJMACROS
#include "tjp.h"
#include <d3d11.h>
#include <stdlib.h>
#include <intrin.h>

#pragma pack(push, 1)
typedef struct { int x, y, width, height; } TigRect;
typedef struct { uint32_t topLeft, topRight, bottomLeft, bottomRight; } ColorRect;
typedef struct {
    int field0, tracking, kerning, leading, field10, field14;
    float rotation, rotationCenterX, rotationCenterY;
    int field24, flags, field2c, colorSlot;
    ColorRect *textColor, *colors2, *shadowColor, *colors4, *bgColor;
    int field48, field4c;
} TigTextStyle;
typedef struct { const char *text; int width, height, lines, lineheight; } TigFontMetrics;
typedef struct { int baseline, largestHeight, fontsize, antialiased, glyphCount, fileCount; void *glyphs; const char *name; int textureIds[4]; } TigFont;
#pragma pack(pop)

/* vanilla temple.dll addresses; the game dll (ToEESteam.dll) may be loaded elsewhere: add g_delta */
uint32_t g_delta;
#define VA(a)         ((uint32_t)(a) + g_delta)
#define FONT_DRAW     VA(0x101EAF30)
#define FONT_MEASURE  VA(0x101EA4E0)
#define LOADED_FONTS  ((TigFont *)VA(0x10EF1448))
#define FONT_STACK    ((int *)VA(0x10EF2C48))
#define STACK_SIZE    (*(int *)VA(0x10EF2E50))

typedef int (__cdecl *DrawFn)(const char *, TigRect *, TigTextStyle *);
typedef int (__cdecl *MeasureFn)(const TigTextStyle *, TigFontMetrics *);
static DrawFn o_draw;
static MeasureFn o_measure;

ID3D11Device *g_d3d;

int jp_has(const char *s)
{
    for (; *s; s++) if ((uint8_t)s[0] >= 0xa1 && (uint8_t)s[1] >= 0xa1) return 1;
    return 0;
}

static TigFont *cur_font(void) { return STACK_SIZE >= 1 ? &LOADED_FONTS[FONT_STACK[0]] : NULL; }

int jp_draw(const TigFont *tf, const char *text, TigRect *r, TigTextStyle *st);
int jp_measure(const TigFont *tf, const TigTextStyle *st, TigFontMetrics *m);
int jp_linebreak(const TigFont *tf, const char *text, const TigRect *r, const TigTextStyle *st);
int jp_hittest(const TigFont *tf, const char *text, const TigRect *r, const TigTextStyle *st, int mx, int my, int *out);

/* test: TJP_SAMPLE=1 shows a few Japanese strings without a translation table */
static int g_sample;
static const char *sample(const char *t)
{
    static const struct { const char *en; const wchar_t *ja; } tab[] = {
        { "New Game", L"新しいゲーム" }, { "Load Game", L"ゲームをロード" }, { "Tutorial", L"チュートリアル" },
        { "Options", L"オプション" }, { "Quit Game", L"ゲーム終了" },
        { "Loading object descriptions...", L"物体の説明を読み込み中……「テスト」です。" },
    };
    static char buf[16][256];
    static int nb;
    int i;
    for (i = 0; i < (int)(sizeof tab / sizeof tab[0]); i++)
        if (!strcmp(t, tab[i].en)) {
            char *b = buf[nb++ & 15];
            WideCharToMultiByte(20932, 0, tab[i].ja, -1, b, 256, NULL, NULL);
            return b;
        }
    return t;
}

static int g_logged;
static int g_trace = -1;
static int __cdecl h_draw(const char *text, TigRect *r, TigTextStyle *st)
{
    if (g_sample) text = sample(text);
    if (g_trace < 0) g_trace = getenv("TJP_TRACE") != NULL;
    if (text && jp_has(text) && r->x >= 0 && r->width >= 0) {
        TigFont *f = cur_font();
        st->colorSlot = 0;
        if (g_trace) {
            int w = 0, h = 0, fmt = 0, ms = 0; D3D11_VIEWPORT vp = {0}; UINT nvp = 1;
            if (g_d3d) {
                ID3D11DeviceContext *c = NULL; ID3D11RenderTargetView *rtv = NULL;
                ID3D11Device_GetImmediateContext(g_d3d, &c);
                ID3D11DeviceContext_OMGetRenderTargets(c, 1, &rtv, NULL);
                ID3D11DeviceContext_RSGetViewports(c, &nvp, &vp);
                if (rtv) { ID3D11Resource *res; D3D11_TEXTURE2D_DESC d; ID3D11RenderTargetView_GetResource(rtv, &res);
                    ID3D11Texture2D_GetDesc((ID3D11Texture2D *)res, &d); w = d.Width; h = d.Height; fmt = d.Format; ms = d.SampleDesc.Count;
                    ID3D11Resource_Release(res); ID3D11RenderTargetView_Release(rtv); }
                ID3D11DeviceContext_Release(c);
            }
            tlog("DRAW ra %p %d,%d %dx%d fl %x trk %d rt %dx%d f%d ms%d vp %.0f,%.0f %.0fx%.0f | %.40s", _ReturnAddress(), r->x, r->y, r->width, r->height, st->flags, st->tracking, w, h, fmt, ms, vp.TopLeftX, vp.TopLeftY, vp.Width, vp.Height, text);
        }
        if (f && jp_draw(f, text, r, st) == 0) return 0;
    }
    if (g_test && g_logged < 400) {
        TigFont *f = cur_font();
        ID3D11DeviceContext *c = NULL;
        ID3D11RenderTargetView *rtv = NULL;
        int w = 0, h = 0;
        if (g_d3d) {
            ID3D11Device_GetImmediateContext(g_d3d, &c);
            ID3D11DeviceContext_OMGetRenderTargets(c, 1, &rtv, NULL);
            if (rtv) {
                ID3D11Resource *res; D3D11_TEXTURE2D_DESC d;
                ID3D11RenderTargetView_GetResource(rtv, &res);
                ID3D11Texture2D_GetDesc((ID3D11Texture2D *)res, &d); w = d.Width; h = d.Height;
                ID3D11Resource_Release(res); ID3D11RenderTargetView_Release(rtv);
            }
            ID3D11DeviceContext_Release(c);
        }
        g_logged++;
        tlog("draw [%s] %d,%d %dx%d font %s/%d h%d b%d fl %x trk %d kern %d slot %d tab %d rt %dx%d", text, r->x, r->y, r->width, r->height,
             f ? f->name : "-", f ? f->fontsize : 0, f ? f->largestHeight : 0, f ? f->baseline : 0, st->flags, st->tracking, st->kerning, st->colorSlot, st->field4c, w, h);
    }
    return o_draw(text, r, st);
}

static int __cdecl h_measure(const TigTextStyle *st, TigFontMetrics *m)
{
    if (g_sample && m->text) m->text = sample(m->text);
    if (g_trace < 0) g_trace = getenv("TJP_TRACE") != NULL;
    if (m->text && jp_has(m->text)) {
        TigFont *f = cur_font();
        int iw = m->width, ih = m->height;
        if (f && jp_measure(f, st, m) == 0) {
            if (g_trace) tlog("MEAS ra %p in %dx%d fl %x trk %d -> %dx%d lines %d | %.40s", _ReturnAddress(), iw, ih, st->flags, st->tracking, m->width, m->height, m->lines, m->text);
            return 0;
        }
    }
    return o_measure(st, m);
}

/* tig's word wrap (0x101E8B20): bytes that fit on the first line. Original kept in a trampoline. */
typedef int (__cdecl *BreakFn)(const char *, const TigRect *, const TigTextStyle *);
static BreakFn o_break;
static int __cdecl h_break(const char *text, const TigRect *r, const TigTextStyle *st)
{
    if (text && jp_has(text)) {
        TigFont *f = cur_font();
        int n;
        if (f && (n = jp_linebreak(f, text, r, st)) >= 0) return n;
    }
    return o_break(text, r, st);
}

/* tig's text hit test (0x101EA7F0): which character of text (drawn in rect) is under (mx, my) - the help links use it */
typedef int (__cdecl *HitFn)(const char *, const TigRect *, const TigTextStyle *, int, int, int *);
static HitFn o_hit;
static int __cdecl h_hit(const char *text, const TigRect *r, const TigTextStyle *st, int mx, int my, int *out)
{
    if (text && jp_has(text)) {
        TigFont *f = cur_font();
        int n;
        if (f && (n = jp_hittest(f, text, r, st, mx, my, out)) >= 0) return n;
    }
    return o_hit(text, r, st, mx, my, out);
}

static uint32_t jmp_target(uint32_t at)
{
    return *(uint8_t *)at == 0xe9 ? at + 5 + *(int32_t *)(at + 1) : 0;
}

/* tig's toupper (party alignment buttons, ...) also maps 0xE0-0xFF; that breaks EUC-JP second bytes. */
static int __cdecl h_toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

static DWORD WINAPI install_thread(LPVOID p)
{
    int i;
    (void)p;
    for (i = 0; i < 1200; i++) {
        uint32_t d, m;
        HMODULE g;
        Sleep(50);
        g = GetModuleHandleA("ToEESteam.dll");
        if (!g) g = GetModuleHandleA("temple.dll");
        if (!g) continue;
        g_delta = (uint32_t)g - 0x10000000;
        d = jmp_target(FONT_DRAW); m = jmp_target(FONT_MEASURE);
        if (d && m && d < 0x10000000 && m < 0x10000000) {
            o_draw = (DrawFn)d; o_measure = (MeasureFn)m;
            patch_jmp(FONT_DRAW, (void *)h_draw);
            patch_jmp(FONT_MEASURE, (void *)h_measure);
            patch_jmp(VA(0x101E8970), (void *)h_toupper);
            {   /* trampoline: the first 11 bytes (mov eax,[abs]; sub esp,48h; cmp eax,1) + jmp back */
                static uint8_t tramp[16];
                uint32_t at = VA(0x101E8B20);
                DWORD old;
                memcpy(tramp, (void *)at, 11);
                tramp[11] = 0xe9; *(int32_t *)(tramp + 12) = (int32_t)(at + 11 - ((uint32_t)tramp + 16));
                VirtualProtect(tramp, sizeof tramp, PAGE_EXECUTE_READWRITE, &old);
                o_break = (BreakFn)(void *)tramp;
                patch_jmp(at, (void *)h_break);
            }
            {   /* same for the hit test: sub esp,6Ch (3) + cmp [abs],1 (7) */
                static uint8_t tramp2[16];
                uint32_t at = VA(0x101EA7F0);
                DWORD old;
                memcpy(tramp2, (void *)at, 10);
                tramp2[10] = 0xe9; *(int32_t *)(tramp2 + 11) = (int32_t)(at + 10 - ((uint32_t)tramp2 + 15));
                VirtualProtect(tramp2, sizeof tramp2, PAGE_EXECUTE_READWRITE, &old);
                o_hit = (HitFn)(void *)tramp2;
                patch_jmp(at, (void *)h_hit);
            }
            tlog("font hooks installed (game dll %p, TPlite draw %08x measure %08x)", g, d, m);
            return 0;
        }
    }
    tlog("font hooks: TPlite redirection not found");
    return 0;
}

static HRESULT (WINAPI *o_D3D11CreateDevice)(void *, int, HMODULE, UINT, const void *, UINT, UINT, ID3D11Device **, void *, ID3D11DeviceContext **);
static HRESULT WINAPI h_D3D11CreateDevice(void *a, int t, HMODULE s, UINT fl, const void *lv, UINT nlv, UINT sdk, ID3D11Device **dev, void *ol, ID3D11DeviceContext **ctx)
{
    HRESULT hr = o_D3D11CreateDevice(a, t, s, fl, lv, nlv, sdk, dev, ol, ctx);
    if (SUCCEEDED(hr) && dev && *dev && !g_d3d) { g_d3d = *dev; tlog("D3D11 device %p flags %x", g_d3d, fl); }
    return hr;
}

void render_init(void)
{
    char b[8];
    if (g_test && GetEnvironmentVariableA("TJP_SAMPLE", b, sizeof b)) g_sample = 1;
    o_D3D11CreateDevice = iat_hook(NULL, "d3d11.dll", "D3D11CreateDevice", (void *)h_D3D11CreateDevice);
    CreateThread(NULL, 0, install_thread, NULL, 0, NULL);
}
