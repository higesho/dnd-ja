// Japanese text layout and drawing for ToEE (TPlite), used for strings that contain EUC-JP.
// Follows TPlite's vanilla TextLayouter rules (word wrap, @0-@9 colour codes, @t tab stops, centring,
// drop shadow, background/border, "..." truncation) but measures and draws with DirectWrite/Direct2D,
// directly on the current D3D11 render target. Japanese may break between any two characters (kinsoku rules).
#include <windows.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <dwrite_3.h>
#include <stdint.h>
#include <vector>
#include <string>

extern "C" {
void tlog(const char *fmt, ...);
const void *res_get(int id, DWORD *size);
extern int g_test;
extern ID3D11Device *g_d3d;
}

#pragma pack(push, 1)
struct TigRect { int x, y, width, height; };
struct ColorRect { uint32_t topLeft, topRight, bottomLeft, bottomRight; };
struct TigTextStyle {
    int field0, tracking, kerning, leading, field10, field14;
    float rotation, rotationCenterX, rotationCenterY;
    int field24, flags, field2c, colorSlot;
    ColorRect *textColor, *colors2, *shadowColor, *colors4, *bgColor;
    int field48, field4c;
};
struct TigFontMetrics { const char *text; int width, height, lines, lineheight; };
struct TigFont { int baseline, largestHeight, fontsize, antialiased, glyphCount, fileCount; void *glyphs; const char *name; int textureIds[4]; };
#pragma pack(pop)

enum { F_SHADOW = 0x8, F_CENTER = 0x10, F_INDENT = 0x200, F_BG = 0x400, F_BORDER = 0x800, F_TRUNC = 0x4000, F_ROTATE = 0x8000, F_ROTATE_OFF = 0x10000 };

// ---------- fonts ----------
struct JFont {
    const TigFont *tf;
    int serif;
    float em, ascent;          // pixels
    int lh, base;
    float adv[0x10000];        // advance per code point (0 = not yet measured)
};
static IDWriteFactory5 *g_dw;
static IDWriteFontFace *g_face[2];
static DWRITE_FONT_METRICS g_fm[2];
static ID2D1Factory1 *g_d2f;
static ID2D1Device *g_d2dev;
static ID2D1DeviceContext *g_dc;
static JFont *g_fonts[64];
static int g_nfonts;

static int init_fonts(void)
{
    static int done, ok;
    if (done) return ok;
    done = 1;
    typedef HRESULT (WINAPI *Fn)(DWRITE_FACTORY_TYPE, REFIID, IUnknown **);
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH);
    lstrcpyA(path + n, "\\DWrite.dll");
    Fn create = (Fn)GetProcAddress(LoadLibraryA(path), "DWriteCreateFactory");
    if (!create || FAILED(create(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory5), (IUnknown **)&g_dw))) { tlog("jp: no IDWriteFactory5"); return 0; }
    IDWriteInMemoryFontFileLoader *loader = NULL;
    if (FAILED(g_dw->CreateInMemoryFontFileLoader(&loader)) || FAILED(g_dw->RegisterFontFileLoader(loader))) return 0;
    for (int i = 0; i < 2; i++) {
        DWORD sz;
        const void *p = res_get(3 + i, &sz);
        IDWriteFontFile *file = NULL;
        if (!p || FAILED(loader->CreateInMemoryFontFileReference(g_dw, p, sz, NULL, &file))) { tlog("jp: font %d missing", i); return 0; }
        if (FAILED(g_dw->CreateFontFace(DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &g_face[i]))) { tlog("jp: font face %d failed", i); return 0; }
        g_face[i]->GetMetrics(&g_fm[i]);
        file->Release();
    }
    ok = 1;
    return 1;
}

static JFont *jfont(const TigFont *tf)
{
    for (int i = 0; i < g_nfonts; i++) if (g_fonts[i]->tf == tf && g_fonts[i]->lh == tf->largestHeight) return g_fonts[i];
    if (g_nfonts == 64) return g_fonts[0];
    JFont *f = (JFont *)calloc(1, sizeof(JFont));
    f->tf = tf;
    f->lh = tf->largestHeight;
    f->base = tf->baseline;
    const char *nm = tf->name ? tf->name : "";
    f->serif = !_strnicmp(nm, "scurlock", 8) || !_strnicmp(nm, "priory", 6) || !_strnicmp(nm, "morph", 5);
    // the em box should fill the line: a bit larger than the Latin font's cap height
    f->em = (float)tf->largestHeight * (tf->largestHeight <= 14 ? 1.0f : 0.78f);
    if (!_strnicmp(nm, "scurlock", 8)) f->em = (float)tf->largestHeight * 0.62f;
    const DWRITE_FONT_METRICS &m = g_fm[f->serif];
    f->ascent = f->em * m.ascent / m.designUnitsPerEm;
    g_fonts[g_nfonts++] = f;
    if (g_test) tlog("jp font %s/%d: lh %d base %d em %.1f", nm, tf->fontsize, f->lh, f->base, f->em);
    return f;
}

static float advance(JFont *f, uint32_t cp)
{
    if (cp >= 0x10000) return f->em;
    if (f->adv[cp] == 0) {
        UINT16 gi = 0;
        DWRITE_GLYPH_METRICS gm;
        UINT32 c = cp;
        g_face[f->serif]->GetGlyphIndices(&c, 1, &gi);
        g_face[f->serif]->GetDesignGlyphMetrics(&gi, 1, &gm, FALSE);
        f->adv[cp] = f->em * gm.advanceWidth / g_fm[f->serif].designUnitsPerEm;
        if (f->adv[cp] <= 0) f->adv[cp] = 0.01f;
    }
    return f->adv[cp];
}

// ---------- text units ----------
enum { U_CHAR, U_SPACE, U_NL, U_COLOR, U_TAB };
struct Unit { int type; uint32_t cp; float w; int off; };

static uint32_t euc_to_cp(uint8_t a, uint8_t b)
{
    char s[2] = { (char)a, (char)b };
    wchar_t w = 0;
    if (MultiByteToWideChar(20932, 0, s, 2, &w, 1) != 1) return 0x30fb;
    return w;
}
static uint32_t cp1252(uint8_t c)
{
    if (c < 0x80) return c;
    char s[1] = { (char)c };
    wchar_t w = '?';
    MultiByteToWideChar(1252, 0, s, 1, &w, 1);
    return w;
}

static void parse(const char *t, JFont *f, const TigTextStyle *st, std::vector<Unit> &u)
{
    const uint8_t *s = (const uint8_t *)t;
    while (*s) {
        Unit x = { U_CHAR, 0, 0, (int)(s - (const uint8_t *)t) };
        if (s[0] == '@' && s[1] >= '0' && s[1] <= '9') { x.type = U_COLOR; x.cp = s[1] - '0'; s += 2; }
        else if (s[0] == '@' && s[1] == 't' && st->field4c > 0) { x.type = U_TAB; s += 2; }
        else if (s[0] == '\n') { x.type = U_NL; s++; }
        else if (s[0] == ' ' || s[0] == '\t' || s[0] == '\r') { x.type = U_SPACE; x.w = (float)st->tracking; s++; }
        else if (s[0] >= 0xa1 && s[1] >= 0xa1) { x.cp = euc_to_cp(s[0], s[1]); s += 2; }
        else { x.cp = cp1252(s[0]); s++; }
        if (x.type == U_CHAR) x.w = advance(f, x.cp) + (x.cp < 0x2e80 ? (float)st->kerning * 0.5f : 0);
        u.push_back(x);
    }
}

static int is_jp(uint32_t c) { return (c >= 0x2e80 && c <= 0x9fff) || (c >= 0xf900 && c <= 0xfaff) || (c >= 0xff00 && c <= 0xffef) || c == 0x2015 || c == 0x2026 || c == 0x2025; }
static int no_start(uint32_t c) { return wcschr(L"、。，．・：；？！ー―…‥」』）】〕〉》｝］’”ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々!?),.:;", (wchar_t)c) != NULL; }
static int no_end(uint32_t c) { return wcschr(L"「『（【〔〈《｛［‘“(", (wchar_t)c) != NULL; }

struct Line { int a, b; float w; };   // unit range [a, b)

// may we break before unit i (between i-1 and i)?
static int can_break(const std::vector<Unit> &u, int i)
{
    if (i <= 0 || i >= (int)u.size()) return 0;
    const Unit &p = u[i - 1], &c = u[i];
    if (c.type != U_CHAR || p.type != U_CHAR) return 0;
    if (!is_jp(p.cp) && !is_jp(c.cp)) return 0;
    return !no_start(c.cp) && !no_end(p.cp);
}

static void layout(std::vector<Unit> &u, float maxw, int x0, const TigTextStyle *st, std::vector<Line> &lines)
{
    int n = (int)u.size(), i = 0;
    while (i <= n) {
        Line L = { i, i, 0 };
        float w = 0;
        int lastbrk = -1;  // unit index to start the next line at
        float wbrk = 0;
        int j = i;
        if (lines.size() && (st->flags & F_INDENT)) w += 8.0f * st->tracking;
        for (; j < n; j++) {
            Unit &x = u[j];
            if (x.type == U_NL) break;
            if (x.type == U_COLOR) continue;
            float add = x.w;
            if (x.type == U_TAB) {
                float tx = (float)(st->field4c - x0);
                add = tx > w ? tx - w : 0;
                x.w = add;
            }
            if (x.type == U_SPACE || x.type == U_TAB) { lastbrk = j + 1; wbrk = w; }
            else if (can_break(u, j)) { lastbrk = j; wbrk = w; }
            if (maxw > 0 && x.type == U_CHAR && w + add > maxw && j > i) {
                if (lastbrk > i) { L.b = lastbrk; L.w = wbrk; }
                else { L.b = j; L.w = w; }   // a single word longer than the line: cut it
                goto emit;
            }
            w += add;
        }
        L.b = j; L.w = w;
        lines.push_back(L);
        if (j >= n) break;
        i = j + 1;   // skip the newline
        continue;
    emit:
        // trailing spaces do not count
        {
            int k = L.b;
            while (k > L.a && u[k - 1].type == U_SPACE) { k--; L.w -= u[k].w; }
            L.b = k;
        }
        lines.push_back(L);
        i = L.b;
        while (i < n && u[i].type == U_SPACE) i++;
        if (i >= n) break;
    }
    if (lines.empty()) { Line L = { 0, 0, 0 }; lines.push_back(L); }
}

// ---------- D2D ----------
struct Target { ID3D11Resource *res; ID2D1Bitmap1 *bmp; };
static Target g_targets[8];
static int g_ntarget;

static int init_d2d(void)
{
    static int done, ok;
    if (done) return ok;
    if (!g_d3d) return 0;
    done = 1;
    IDXGIDevice *dx = NULL;
    D2D1_FACTORY_OPTIONS fo = {};
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &fo, (void **)&g_d2f))) return 0;
    if (FAILED(g_d3d->QueryInterface(__uuidof(IDXGIDevice), (void **)&dx))) return 0;
    HRESULT hr = g_d2f->CreateDevice(dx, &g_d2dev);
    dx->Release();
    if (FAILED(hr) || FAILED(g_d2dev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g_dc))) { tlog("jp: D2D device failed %08x", hr); return 0; }
    g_dc->SetDpi(96, 96);
    g_dc->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
    g_dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ok = 1;
    return 1;
}

static ID2D1Bitmap1 *current_target(ID3D11DeviceContext *ctx)
{
    ID3D11RenderTargetView *rtv = NULL;
    ctx->OMGetRenderTargets(1, &rtv, NULL);
    if (!rtv) return NULL;
    ID3D11Resource *res = NULL;
    rtv->GetResource(&res);
    rtv->Release();
    for (int i = 0; i < g_ntarget; i++) if (g_targets[i].res == res) { res->Release(); return g_targets[i].bmp; }
    IDXGISurface *surf = NULL;
    ID2D1Bitmap1 *bmp = NULL;
    if (SUCCEEDED(res->QueryInterface(__uuidof(IDXGISurface), (void **)&surf))) {
        DXGI_SURFACE_DESC sd;
        surf->GetDesc(&sd);
        D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                             D2D1::PixelFormat(sd.Format, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
        HRESULT hr = g_dc->CreateBitmapFromDxgiSurface(surf, &bp, &bmp);
        if (FAILED(hr)) tlog("jp: CreateBitmapFromDxgiSurface fmt %d failed %08x", sd.Format, hr);
        surf->Release();
    }
    if (!bmp) { res->Release(); return NULL; }
    if (g_ntarget == 8) {   // forget the oldest
        g_targets[0].res->Release(); g_targets[0].bmp->Release();
        memmove(g_targets, g_targets + 1, 7 * sizeof(Target));
        g_ntarget--;
    }
    g_targets[g_ntarget].res = res;   // keeps a reference so the pointer is never reused for another texture
    g_targets[g_ntarget].bmp = bmp;
    g_ntarget++;
    return bmp;
}

static D2D1_COLOR_F col(uint32_t c)
{
    return D2D1::ColorF(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, (c >> 24) / 255.0f);
}

static ID2D1Brush *brush_for(const ColorRect *cr, float y0, float y1)
{
    if (!cr) {
        ID2D1SolidColorBrush *b = NULL;
        g_dc->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &b);
        return b;
    }
    if (cr->topLeft == cr->bottomLeft) {
        ID2D1SolidColorBrush *b = NULL;
        g_dc->CreateSolidColorBrush(col(cr->topLeft), &b);
        return b;
    }
    D2D1_GRADIENT_STOP gs[2] = { { 0, col(cr->topLeft) }, { 1, col(cr->bottomLeft) } };
    ID2D1GradientStopCollection *sc = NULL;
    ID2D1LinearGradientBrush *b = NULL;
    g_dc->CreateGradientStopCollection(gs, 2, &sc);
    g_dc->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, y0), D2D1::Point2F(0, y1)), sc, &b);
    sc->Release();
    return b;
}

// draws units [a,b) of one line starting at pen x, baseline y
static void draw_run(JFont *f, std::vector<Unit> &u, int a, int b, float x, float base, float top, const TigTextStyle *st, int &slot, float dx, float dy, const ColorRect *force)
{
    std::vector<UINT16> gi;
    std::vector<FLOAT> adv;
    float runx = x;
    auto flush = [&](void) {
        if (gi.empty()) return;
        const ColorRect *cr = force ? force : (st->textColor ? &st->textColor[slot] : NULL);
        ID2D1Brush *br = brush_for(cr, top + dy, top + dy + f->lh);
        DWRITE_GLYPH_RUN run = {};
        run.fontFace = g_face[f->serif];
        run.fontEmSize = f->em;
        run.glyphCount = (UINT32)gi.size();
        run.glyphIndices = gi.data();
        run.glyphAdvances = adv.data();
        if (br) { g_dc->DrawGlyphRun(D2D1::Point2F(runx + dx, base + dy), &run, br); br->Release(); }
        gi.clear(); adv.clear();
    };
    for (int i = a; i < b; i++) {
        Unit &x2 = u[i];
        if (x2.type == U_COLOR) { flush(); slot = (int)x2.cp; runx = x; continue; }
        if (x2.type != U_CHAR) { flush(); x += x2.w; runx = x; continue; }
        UINT16 g = 0;
        UINT32 c = x2.cp;
        g_face[f->serif]->GetGlyphIndices(&c, 1, &g);
        if (gi.empty()) runx = x;
        gi.push_back(g);
        adv.push_back(x2.w);
        x += x2.w;
    }
    flush();
}

// TPlite (2026-11 update) draws the UI on a canvas of window size / scale and scales it up to the window
// (canvasFitToWindow, uiScalePercent). Text rects are in canvas units: scale our drawing the same way.
// The scale is a float global that TPlite's setter stores at the end:  movaps xmm2,xmm3; movss [g],xmm2;
// mov esp,ebp; pop ebp; ret; movss xmm0,[one]; movss [g],xmm0
static float *find_ui_scale(void)
{
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        uint8_t *p = base + sec->VirtualAddress, *e = p + sec->Misc.VirtualSize - 40;
        for (; p < e; p++) {
            if (p[0] != 0x0f || p[1] != 0x28 || p[2] != 0xd3 || p[3] != 0xf3 || p[4] != 0x0f || p[5] != 0x11 || p[6] != 0x15) continue;
            if (memcmp(p + 11, "\x8b\xe5\x5d\xc3\xf3\x0f\x10\x05", 8) || memcmp(p + 23, "\xf3\x0f\x11\x05", 4)) continue;
            if (memcmp(p + 7, p + 27, 4)) continue;
            uint32_t a = *(uint32_t *)(p + 7);
            tlog("jp: ui scale at %08x (code %p)", a, p);
            return (float *)a;
        }
    }
    tlog("jp: ui scale not found (scale 1)");
    return NULL;
}
static float ui_scale(void)
{
    static int done;
    static float *g;
    if (!done) { done = 1; g = find_ui_scale(); }
    float s = g ? *g : 1.0f;
    return s > 0.1f && s < 20.0f ? s : 1.0f;
}

extern "C" int jp_draw(const TigFont *tf, const char *text, TigRect *r, TigTextStyle *st)
{
    if (!init_fonts() || !init_d2d()) return -1;
    JFont *f = jfont(tf);
    std::vector<Unit> u;
    parse(text, f, st, u);
    float maxw = r->width > 0 ? (float)r->width : 0;
    std::vector<Line> lines;
    layout(u, maxw, r->x, st, lines);
    if (r->width <= 0 || r->height <= 0) {
        float mw = 0;
        for (auto &L : lines) if (L.w > mw) mw = L.w;
        if (r->width <= 0) r->width = (int)(mw + 0.99f);
        if (r->height <= 0) r->height = (int)lines.size() * f->lh;
    }
    // truncation: lines that do not fit; the last one gets "…"
    int maxlines = (int)lines.size();
    if (r->height > 0) {
        int fit = r->height / (f->lh ? f->lh : 1);
        if (fit < 1) fit = 1;
        if ((st->flags & F_TRUNC) && maxlines > fit) maxlines = fit;
    }

    ID3D11DeviceContext *ctx = NULL;
    g_d3d->GetImmediateContext(&ctx);
    ID2D1Bitmap1 *bmp = current_target(ctx);
    if (!bmp) { ctx->Release(); return -1; }
    // honour the D3D scissor rectangle if scissoring is on
    D3D11_RECT sc;
    UINT nsc = 1;
    ID3D11RasterizerState *rs = NULL;
    BOOL scissor = FALSE;
    ctx->RSGetState(&rs);
    if (rs) { D3D11_RASTERIZER_DESC rd; rs->GetDesc(&rd); scissor = rd.ScissorEnable; rs->Release(); }
    if (scissor) ctx->RSGetScissorRects(&nsc, &sc);
    ctx->Release();

    g_dc->SetTarget(bmp);
    g_dc->BeginDraw();
    float s_sc = ui_scale();
    D2D1::Matrix3x2F base_tf = D2D1::Matrix3x2F::Scale(s_sc, s_sc);
    if (scissor && nsc) g_dc->PushAxisAlignedClip(D2D1::RectF((float)sc.left, (float)sc.top, (float)sc.right, (float)sc.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
    g_dc->SetTransform(base_tf);
    // radial menu: text turned around a point (like tig: the rect's left/baseline unless an explicit centre is given)
    if (st->flags & F_ROTATE) {
        float cx = (float)r->x, cy = (float)r->y + (float)f->base;
        if (st->flags & F_ROTATE_OFF) { cx = st->rotationCenterX; cy = st->rotationCenterY; }
        g_dc->SetTransform(D2D1::Matrix3x2F::Rotation(st->rotation * 57.2957795f, D2D1::Point2F(cx, cy)) * base_tf);
    }

    if ((st->flags & (F_BG | F_BORDER))) {
        D2D1_RECT_F rc = D2D1::RectF((float)r->x - 3, (float)r->y - 3, (float)(r->x + r->width) + 3, (float)(r->y + r->height) + 3);
        if ((st->flags & F_BG) && st->bgColor) {
            ID2D1SolidColorBrush *b = NULL;
            g_dc->CreateSolidColorBrush(col(st->bgColor->topLeft), &b);
            if (b) { g_dc->FillRectangle(rc, b); b->Release(); }
        }
        if (st->flags & F_BORDER) {
            ID2D1SolidColorBrush *b = NULL;
            g_dc->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &b);
            if (b) { g_dc->DrawRectangle(D2D1::RectF(rc.left - 0.5f, rc.top - 0.5f, rc.right + 0.5f, rc.bottom + 0.5f), b); b->Release(); }
        }
    }

    // ellipsis width
    Unit ell = { U_CHAR, 0x2026, advance(f, 0x2026) };
    int slot = st->colorSlot;
    for (int pass = (st->flags & F_SHADOW) && st->shadowColor ? 0 : 1; pass < 2; pass++) {
        int sl = slot;
        float y = (float)r->y;
        for (int li = 0; li < maxlines; li++) {
            Line L = lines[li];
            int trunc = (st->flags & F_TRUNC) && li == maxlines - 1 && (maxlines < (int)lines.size() || (r->width > 0 && L.w > r->width));
            if (trunc) {   // drop characters until "…" fits
                while (L.b > L.a && L.w + ell.w > r->width) { L.b--; if (u[L.b].type != U_COLOR) L.w -= u[L.b].w; }
            }
            float x = (float)r->x + ((li > 0 && (st->flags & F_INDENT)) ? 8.0f * st->tracking : 0);
            if (st->flags & F_CENTER) x = (float)r->x + ((float)r->width - L.w) / 2;
            float base = y + (float)f->base + (f->ascent - (float)f->base) * 0.5f;
            if (base < y + f->ascent * 0.8f) base = y + f->ascent * 0.8f;
            float dx = pass == 0 ? 1.0f : 0, dy = pass == 0 ? 1.0f : 0;
            draw_run(f, u, L.a, L.b, x, base, y, st, sl, dx, dy, pass == 0 ? st->shadowColor : NULL);
            if (trunc) {
                std::vector<Unit> e(1, ell);
                int s2 = sl;
                draw_run(f, e, 0, 1, x + L.w, base, y, st, s2, dx, dy, pass == 0 ? st->shadowColor : NULL);
            }
            y += (float)f->lh;
        }
    }

    g_dc->SetTransform(D2D1::Matrix3x2F::Identity());
    if (scissor && nsc) g_dc->PopAxisAlignedClip();
    HRESULT hr = g_dc->EndDraw();
    g_dc->SetTarget(NULL);
    if (FAILED(hr)) tlog("jp: EndDraw %08x", hr);
    return 0;
}

// tig's word wrap: how many bytes of text go on the first line of rect r (the scroll boxes split their text with it)
extern "C" int jp_linebreak(const TigFont *tf, const char *text, const TigRect *r, const TigTextStyle *st)
{
    if (!init_fonts()) return -1;
    JFont *f = jfont(tf);
    std::vector<Unit> u;
    parse(text, f, st, u);
    std::vector<Line> lines;
    layout(u, (float)r->width, r->x, st, lines);
    int len = (int)strlen(text);
    if (lines.size() < 2) return len;
    int b = lines[0].b;
    if (b < (int)u.size() && u[b].type == U_NL) return u[b].off;   // like tig: stop at the newline itself
    int a = lines[1].a;
    return a < (int)u.size() ? u[a].off : len;
}

// tig's hit test (help links): byte offset of the character under (mx, my) when the text is drawn in rect r
extern "C" int jp_hittest(const TigFont *tf, const char *text, const TigRect *r, const TigTextStyle *st, int mx, int my, int *out)
{
    if (!init_fonts()) return -1;
    JFont *f = jfont(tf);
    std::vector<Unit> u;
    parse(text, f, st, u);
    std::vector<Line> lines;
    layout(u, r->width > 0 ? (float)r->width : 0, r->x, st, lines);
    int li = (my - r->y) / (f->lh ? f->lh : 1);
    if (my < r->y || li >= (int)lines.size()) return 2;
    const Line &L = lines[li];
    float x = (float)r->x + ((li > 0 && (st->flags & F_INDENT)) ? 8.0f * st->tracking : 0);
    if (st->flags & F_CENTER) x = (float)r->x + ((float)r->width - L.w) / 2;
    for (int i = L.a; i < L.b; i++) {
        if (u[i].type == U_COLOR) continue;
        if (mx >= x && mx < x + u[i].w) { *out = u[i].off; return 0; }
        x += u[i].w;
    }
    return 2;
}

extern "C" int jp_measure(const TigFont *tf, const TigTextStyle *st, TigFontMetrics *m)
{
    if (!init_fonts()) return -1;
    JFont *f = jfont(tf);
    std::vector<Unit> u;
    parse(m->text ? m->text : "", f, st, u);
    std::vector<Line> lines;
    layout(u, m->width > 0 ? (float)m->width : 0, 0, st, lines);
    if (!m->width) {
        float mw = 0;
        for (auto &L : lines) if (L.w > mw) mw = L.w;
        m->width = (int)(mw + 0.99f);
    }
    int n = (int)lines.size();
    if (m->height) {
        int maxl = m->height / (f->lh ? f->lh : 1);
        if (!(st->flags & F_TRUNC)) maxl++;
        if (n > maxl && maxl > 0) n = maxl;
    } else if (st->flags & F_TRUNC) n = 1;
    m->lines = n;
    if (!m->height) m->height = n * f->lh + (f->base - f->lh);
    m->lineheight = f->lh;
    return 0;
}
