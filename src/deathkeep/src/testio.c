/*
 * テスト用（環境変数 DK_TEST=1 のときだけ動く）: ゲームの画面を BMP に書き出し、キーとマウスを入れる。
 * 共有メモリ "DK_TESTIO": u32 magic, i32 req, i32 ack, i32 frame, char path[260] @16, u32 wr @300, u32 rd @304, cmd[64][4 i32] @320
 *   cmd: 1 = キー（vk, ms）, 2 = マウスを動かす（x, y）, 3 = クリック（0 左 / 1 右, ms）, 6 = 待つ（ms）
 * 画面: DirectDrawCreate（DK.dll）を差し替え、主画面のサーフェスの Blt/BltFast/Flip/Unlock の後で
 *       頼まれていれば主画面を Lock して 8 ビットの絵とパレットを書き出す
 */
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdint.h>

void dlog(const char *fmt, ...);
void *iat_hook(HMODULE mod, const char *dll, const char *func, void *repl);

#pragma pack(push, 1)
typedef struct { uint32_t magic; int32_t req, ack, frame; char path[284]; uint32_t wr, rd; uint32_t pad[3]; int32_t cmd[64][4]; } shm_t;
#pragma pack(pop)
static shm_t *g_shm;
static HWND g_hwnd;
static volatile LONG g_vx = 320, g_vy = 240;
static volatile LONG g_keys[256];

static BOOL CALLBACK enum_cb(HWND h, LPARAM l)
{
    DWORD pid;
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(h)) { *(HWND*)l = h; return FALSE; }
    return TRUE;
}
static HWND game_hwnd(void)
{
    if (!g_hwnd || !IsWindow(g_hwnd)) { HWND h = NULL; EnumWindows(enum_cb, (LPARAM)&h); g_hwnd = h; }
    return g_hwnd;
}

/* ---- 入力 */
static BOOL (WINAPI *o_GetCursorPos)(LPPOINT);
static BOOL WINAPI h_GetCursorPos(LPPOINT p)
{
    HWND h = game_hwnd();
    p->x = g_vx; p->y = g_vy;
    if (h) ClientToScreen(h, p);
    return TRUE;
}
static SHORT (WINAPI *o_GetAsyncKeyState)(int);
static SHORT WINAPI h_GetAsyncKeyState(int vk)
{
    return (vk >= 0 && vk < 256 && g_keys[vk]) ? (SHORT)0x8001 : 0;
}

static void post_key(int vk, int down)
{
    HWND h = game_hwnd();
    UINT sc = MapVirtualKeyA(vk, 0);
    g_keys[vk & 255] = down;
    if (h) PostMessageA(h, down ? WM_KEYDOWN : WM_KEYUP, vk, 1 | (sc << 16) | (down ? 0 : (3u << 30)));
    /* WM_CHAR はゲームの TranslateMessage が作る */
}
static void post_mouse(UINT msg, WPARAM wp)
{
    HWND h = game_hwnd();
    if (h) PostMessageA(h, msg, wp, MAKELPARAM(g_vx, g_vy));
}

static void save_shot(void);
static int g_in;
static DWORD WINAPI shot_thread(LPVOID p)
{
    int32_t seen = 0;
    DWORD t0 = 0;
    (void)p;
    for (;;) {
        Sleep(50);
        if (g_shm->req == g_shm->ack) { seen = g_shm->req; t0 = 0; continue; }
        if (!t0) t0 = GetTickCount();
        if (GetTickCount() - t0 > 300 && !g_in) { g_in = 1; save_shot(); g_in = 0; }
    }
}

static DWORD WINAPI cmd_thread(LPVOID p)
{
    (void)p;
    for (;;) {
        if (g_shm->rd == g_shm->wr) { Sleep(10); continue; }
        {
            int32_t *c = g_shm->cmd[g_shm->rd % 64];
            switch (c[0]) {
            case 1: post_key(c[1], 1); Sleep(c[2] > 0 ? c[2] : 60); post_key(c[1], 0); break;
            case 2: g_vx = c[1]; g_vy = c[2]; post_mouse(WM_MOUSEMOVE, 0); break;
            case 3:
                if (c[1]) { g_keys[VK_RBUTTON] = 1; post_mouse(WM_RBUTTONDOWN, MK_RBUTTON); Sleep(c[2] > 0 ? c[2] : 80); g_keys[VK_RBUTTON] = 0; post_mouse(WM_RBUTTONUP, 0); }
                else { g_keys[VK_LBUTTON] = 1; post_mouse(WM_LBUTTONDOWN, MK_LBUTTON); Sleep(c[2] > 0 ? c[2] : 80); g_keys[VK_LBUTTON] = 0; post_mouse(WM_LBUTTONUP, 0); }
                break;
            case 6: Sleep(c[1]); break;
            }
            g_shm->rd++;
        }
    }
}

/* ---- 画面 */
static IDirectDrawSurface *g_primary;
static void save_shot(void)
{
    DDSURFACEDESC sd;
    IDirectDrawPalette *pal = NULL;
    PALETTEENTRY pe[256];
    FILE *f;
    int y, x;
    if (!g_primary || g_shm->req == g_shm->ack) return;
    memset(&sd, 0, sizeof sd); sd.dwSize = sizeof sd;
    memset(pe, 0, sizeof pe);
    if (g_primary->lpVtbl->GetPalette(g_primary, &pal) == DD_OK && pal) {
        pal->lpVtbl->GetEntries(pal, 0, 0, 256, pe);
        pal->lpVtbl->Release(pal);
    }
    if (g_primary->lpVtbl->Lock(g_primary, NULL, &sd, DDLOCK_WAIT | DDLOCK_READONLY, NULL) != DD_OK) return;
    f = fopen(g_shm->path, "wb");
    if (f) {
        int w = sd.dwWidth, h = sd.dwHeight, bpp = sd.ddpfPixelFormat.dwRGBBitCount ? sd.ddpfPixelFormat.dwRGBBitCount : 8;
        BITMAPFILEHEADER fh = {0};
        BITMAPINFOHEADER ih = {0};
        int stride = w * 3, padr = (4 - stride % 4) % 4;
        fh.bfType = 0x4d42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + (stride + padr) * h;
        ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 24;
        fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f);
        for (y = 0; y < h; y++) {
            uint8_t *row = (uint8_t*)sd.lpSurface + y * sd.lPitch, z[3] = {0};
            for (x = 0; x < w; x++) {
                uint8_t px[3];
                if (bpp == 8) { PALETTEENTRY e = pe[row[x]]; px[0] = e.peBlue; px[1] = e.peGreen; px[2] = e.peRed; }
                else if (bpp == 32) { px[0] = row[x * 4]; px[1] = row[x * 4 + 1]; px[2] = row[x * 4 + 2]; }
                else { uint16_t v = ((uint16_t*)row)[x]; px[0] = (uint8_t)((v & 31) << 3); px[1] = (uint8_t)(((v >> 5) & 63) << 2); px[2] = (uint8_t)((v >> 11) << 3); }
                fwrite(px, 3, 1, f);
            }
            fwrite(z, padr, 1, f);
        }
        fclose(f);
    }
    g_primary->lpVtbl->Unlock(g_primary, NULL);
    g_shm->ack = g_shm->req;
}
static void after_present(void)
{
    if (g_in) return;
    g_in = 1;
    g_shm->frame++;
    save_shot();
    g_in = 0;
}

typedef HRESULT (STDMETHODCALLTYPE *blt_t)(IDirectDrawSurface*, LPRECT, IDirectDrawSurface*, LPRECT, DWORD, LPDDBLTFX);
typedef HRESULT (STDMETHODCALLTYPE *bltfast_t)(IDirectDrawSurface*, DWORD, DWORD, IDirectDrawSurface*, LPRECT, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *flip_t)(IDirectDrawSurface*, IDirectDrawSurface*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *unlock_t)(IDirectDrawSurface*, LPVOID);
static blt_t o_Blt; static bltfast_t o_BltFast; static flip_t o_Flip; static unlock_t o_Unlock;
static HRESULT STDMETHODCALLTYPE h_Blt(IDirectDrawSurface *s, LPRECT a, IDirectDrawSurface *b, LPRECT c, DWORD d, LPDDBLTFX e)
{ HRESULT r = o_Blt(s, a, b, c, d, e); if (s == g_primary) after_present(); return r; }
static HRESULT STDMETHODCALLTYPE h_BltFast(IDirectDrawSurface *s, DWORD x, DWORD y, IDirectDrawSurface *b, LPRECT c, DWORD d)
{ HRESULT r = o_BltFast(s, x, y, b, c, d); if (s == g_primary) after_present(); return r; }
static HRESULT STDMETHODCALLTYPE h_Flip(IDirectDrawSurface *s, IDirectDrawSurface *t, DWORD d)
{ HRESULT r = o_Flip(s, t, d); if (s == g_primary) after_present(); return r; }
static HRESULT STDMETHODCALLTYPE h_Unlock(IDirectDrawSurface *s, LPVOID p)
{ HRESULT r = o_Unlock(s, p); if (s == g_primary) after_present(); return r; }

static void patch_vt(void **vt, int i, void *fn, void **orig)
{
    DWORD old;
    if (vt[i] == fn) return;
    VirtualProtect(&vt[i], 4, PAGE_EXECUTE_READWRITE, &old);
    *orig = vt[i]; vt[i] = fn;
    VirtualProtect(&vt[i], 4, old, &old);
}

typedef HRESULT (STDMETHODCALLTYPE *cs_t)(IDirectDraw*, LPDDSURFACEDESC, LPDIRECTDRAWSURFACE*, IUnknown*);
static cs_t o_CreateSurface;
static HRESULT STDMETHODCALLTYPE h_CreateSurface(IDirectDraw *dd, LPDDSURFACEDESC d, LPDIRECTDRAWSURFACE *s, IUnknown *u)
{
    HRESULT r = o_CreateSurface(dd, d, s, u);
    if (r == DD_OK && d && (d->dwFlags & DDSD_CAPS) && (d->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)) {
        void **vt = *(void***)*s;
        g_primary = *s;
        patch_vt(vt, 5, (void*)h_Blt, (void**)&o_Blt);
        patch_vt(vt, 7, (void*)h_BltFast, (void**)&o_BltFast);
        patch_vt(vt, 11, (void*)h_Flip, (void**)&o_Flip);
        patch_vt(vt, 32, (void*)h_Unlock, (void**)&o_Unlock);
        dlog("primary surface %p\n", *s);
    }
    return r;
}
typedef HRESULT (WINAPI *ddc_t)(GUID*, LPDIRECTDRAW*, IUnknown*);
static ddc_t o_DirectDrawCreate;
static HRESULT WINAPI h_DirectDrawCreate(GUID *g, LPDIRECTDRAW *dd, IUnknown *u)
{
    HRESULT r = o_DirectDrawCreate(g, dd, u);
    if (r == DD_OK && dd && *dd) {
        void **vt = *(void***)*dd;
        if (vt[6] != (void*)h_CreateSurface) patch_vt(vt, 6, (void*)h_CreateSurface, (void**)&o_CreateSurface);
        dlog("DirectDrawCreate %p\n", *dd);
    }
    return r;
}

void testio_init(void)
{
    char v[8];
    HANDLE h;
    HMODULE exe = GetModuleHandleA(NULL);
    if (!GetEnvironmentVariableA("DK_TEST", v, sizeof v) || v[0] != '1') return;
    h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(shm_t), "DK_TESTIO");
    if (!h) return;
    g_shm = (shm_t*)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(shm_t));
    if (!g_shm) return;
    memset(g_shm, 0, sizeof *g_shm);
    g_shm->magic = 0x4b445453;
    o_DirectDrawCreate = (ddc_t)iat_hook(exe, "DK.dll", "DirectDrawCreate", (void*)h_DirectDrawCreate);
    o_GetCursorPos = iat_hook(exe, "USER32.dll", "GetCursorPos", (void*)h_GetCursorPos);
    o_GetAsyncKeyState = iat_hook(exe, "USER32.dll", "GetAsyncKeyState", (void*)h_GetAsyncKeyState);
    CreateThread(NULL, 0, cmd_thread, NULL, 0, NULL);
    CreateThread(NULL, 0, shot_thread, NULL, 0, NULL);
    dlog("testio: ddc %p cursor %p keys %p\n", o_DirectDrawCreate, o_GetCursorPos, o_GetAsyncKeyState);
}
