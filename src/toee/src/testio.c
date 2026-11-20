/* Test mode only (TJP_TEST=1): back-buffer screenshots on request through the shared memory "TJP_TESTIO"
   (tools/win.py writes a path and bumps shot_req). Input is posted to the window by win.py. */
#define COBJMACROS
#include "tjp.h"
#include <stdio.h>
#include <d3d11.h>
#include <dxgi.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

typedef struct {
    volatile LONG magic;
    volatile LONG shot_req, shot_done;
    volatile LONG frame;
    char shot_path[260];
    volatile LONG kwr;            /* key events written */
    uint8_t keys[256];            /* DIK codes held down (0x80) */
    uint16_t kev[64];             /* ring: DIK | 0x100 when pressed */
    uint8_t vkeys[256];           /* virtual keys held down (0x80) */
} Shm;

static Shm *g_shm;

static void save_bmp(const char *path, int w, int h, const uint8_t *bits, int pitch, int bgra)
{
    FILE *f = fopen(path, "wb");
    BITMAPFILEHEADER fh = { 0 };
    BITMAPINFOHEADER ih = { 0 };
    int y, x;
    uint8_t *row;
    if (!f) return;
    fh.bfType = 0x4d42;
    fh.bfOffBits = sizeof fh + sizeof ih;
    fh.bfSize = fh.bfOffBits + w * h * 4;
    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 32;
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&ih, sizeof ih, 1, f);
    row = (uint8_t *)malloc(w * 4);
    for (y = 0; y < h; y++) {
        const uint8_t *s = bits + y * pitch;
        for (x = 0; x < w; x++) {
            if (bgra) { row[x * 4] = s[x * 4]; row[x * 4 + 1] = s[x * 4 + 1]; row[x * 4 + 2] = s[x * 4 + 2]; }
            else { row[x * 4] = s[x * 4 + 2]; row[x * 4 + 1] = s[x * 4 + 1]; row[x * 4 + 2] = s[x * 4]; }
            row[x * 4 + 3] = 255;
        }
        fwrite(row, 4, w, f);
    }
    free(row);
    fclose(f);
}

void testio_present(void *sc_)
{
    IDXGISwapChain *sc = (IDXGISwapChain *)sc_;
    ID3D11Texture2D *bb = NULL, *st = NULL;
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;
    D3D11_TEXTURE2D_DESC d;
    D3D11_MAPPED_SUBRESOURCE m;
    if (!g_shm) return;
    g_shm->frame++;
    if (g_shm->shot_req == g_shm->shot_done) return;
    if (SUCCEEDED(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb))) {
        ID3D11Texture2D_GetDevice(bb, &dev);
        ID3D11Device_GetImmediateContext(dev, &ctx);
        ID3D11Texture2D_GetDesc(bb, &d);
        d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
        d.SampleDesc.Count = 1; d.SampleDesc.Quality = 0;
        if (SUCCEEDED(ID3D11Device_CreateTexture2D(dev, &d, NULL, &st))) {
            ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)st, (ID3D11Resource *)bb);
            if (SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m))) {
                save_bmp(g_shm->shot_path, d.Width, d.Height, (uint8_t *)m.pData, m.RowPitch,
                         d.Format == DXGI_FORMAT_B8G8R8A8_UNORM || d.Format == DXGI_FORMAT_B8G8R8X8_UNORM);
                ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0);
            }
            ID3D11Texture2D_Release(st);
        }
        ID3D11DeviceContext_Release(ctx);
        ID3D11Device_Release(dev);
        ID3D11Texture2D_Release(bb);
    }
    g_shm->shot_done = g_shm->shot_req;
}

/* ---- DXGI: wrap the factory to catch the swap chain, hot-patch its Present ---- */
static HRESULT (STDMETHODCALLTYPE *o_Present)(IDXGISwapChain *, UINT, UINT);
static HRESULT STDMETHODCALLTYPE h_Present(IDXGISwapChain *sc, UINT sync, UINT fl)
{
    testio_present(sc);
    return o_Present(sc, sync, fl);
}
static void hotpatch(uint8_t *f, void *hook, void **orig, const char *what)
{
    static const uint8_t jshort[2] = { 0xeb, 0xf9 };
    if (*orig) return;
    if (f[0] == 0x8b && f[1] == 0xff && f[-5] == 0xcc && f[-1] == 0xcc) {
        *orig = (void *)(f + 2);
        patch_jmp((uint32_t)(f - 5), hook);
        patch_bytes((uint32_t)f, jshort, 2);
        tlog("%s hot-patched at %p", what, f);
    } else tlog("%s: no hot-patch prologue at %p (%02x %02x)", what, f, f[0], f[1]);
}
static HRESULT STDMETHODCALLTYPE h_SetFullscreenState(IDXGISwapChain *sc, BOOL fs, IDXGIOutput *o)
{
    (void)sc; (void)o;
    tlog("SetFullscreenState(%d) refused", fs);
    return S_OK;
}
static HRESULT (STDMETHODCALLTYPE *o_CreateSwapChain)(IDXGIFactory *, IUnknown *, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **);
static HRESULT STDMETHODCALLTYPE h_CreateSwapChain(IDXGIFactory *f, IUnknown *dev, DXGI_SWAP_CHAIN_DESC *d, IDXGISwapChain **out)
{
    HRESULT hr;
    tlog("CreateSwapChain %ux%u fmt %d windowed %d swap %d hwnd %p", d->BufferDesc.Width, d->BufferDesc.Height, d->BufferDesc.Format, d->Windowed, d->SwapEffect, d->OutputWindow);
    d->Windowed = TRUE;
    hr = o_CreateSwapChain(f, dev, d, out);
    if (SUCCEEDED(hr)) {
        void **vt = *(void ***)*out;
        DWORD p;
        hotpatch((uint8_t *)vt[8], (void *)h_Present, (void **)&o_Present, "Present");
        VirtualProtect(&vt[10], 4, PAGE_EXECUTE_READWRITE, &p);
        vt[10] = (void *)h_SetFullscreenState;
        VirtualProtect(&vt[10], 4, p, &p);
    }
    return hr;
}
static HRESULT (WINAPI *o_CreateDXGIFactory1)(REFIID, void **);
static HRESULT WINAPI h_CreateDXGIFactory1(REFIID r, void **out)
{
    HRESULT hr = o_CreateDXGIFactory1(r, out);
    if (SUCCEEDED(hr) && !o_CreateSwapChain) {
        void **vt = *(void ***)*out;
        DWORD p;
        VirtualProtect(&vt[10], 4, PAGE_EXECUTE_READWRITE, &p);
        o_CreateSwapChain = vt[10];
        vt[10] = (void *)h_CreateSwapChain;
        VirtualProtect(&vt[10], 4, p, &p);
    }
    return hr;
}

/* ---------- keyboard: DirectInput keyboard of the game gets the keys win.py injects ---------- */
static void *g_kbd[4];
static LONG g_krd[4];
static HANDLE g_kevt[4];
static int kbd_index(void *d) { int i; for (i = 0; i < 4; i++) if (g_kbd[i] == d) return i; return -1; }
static HRESULT (STDMETHODCALLTYPE *o_CreateDeviceDI)(IDirectInput8A *, REFGUID, LPDIRECTINPUTDEVICE8A *, LPUNKNOWN);
static HRESULT (STDMETHODCALLTYPE *o_GetDeviceState)(IDirectInputDevice8A *, DWORD, LPVOID);
static HRESULT (STDMETHODCALLTYPE *o_GetDeviceData)(IDirectInputDevice8A *, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
static HRESULT (STDMETHODCALLTYPE *o_SetEvent)(IDirectInputDevice8A *, HANDLE);
static HRESULT (STDMETHODCALLTYPE *o_Acquire)(IDirectInputDevice8A *);
static HRESULT (STDMETHODCALLTYPE *o_SetCoop)(IDirectInputDevice8A *, HWND, DWORD);

static HRESULT STDMETHODCALLTYPE h_Acquire(IDirectInputDevice8A *d) { HRESULT hr = o_Acquire(d); return kbd_index(d) >= 0 ? DI_OK : hr; }
static HRESULT STDMETHODCALLTYPE h_SetCoop(IDirectInputDevice8A *d, HWND w, DWORD fl)
{
    if (kbd_index(d) >= 0) fl = DISCL_BACKGROUND | DISCL_NONEXCLUSIVE;
    return o_SetCoop(d, w, fl);
}
static HRESULT STDMETHODCALLTYPE h_SetEvent(IDirectInputDevice8A *d, HANDLE e)
{
    int i = kbd_index(d);
    if (i >= 0) g_kevt[i] = e;
    return o_SetEvent(d, e);
}
static HRESULT STDMETHODCALLTYPE h_GetDeviceState(IDirectInputDevice8A *d, DWORD n, LPVOID out)
{
    HRESULT hr = o_GetDeviceState(d, n, out);
    if (kbd_index(d) >= 0 && g_shm) {
        DWORD i;
        if (FAILED(hr)) { memset(out, 0, n); hr = DI_OK; }
        for (i = 0; i < n && i < 256; i++) ((uint8_t *)out)[i] |= g_shm->keys[i];
    }
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_GetDeviceData(IDirectInputDevice8A *d, DWORD sz, LPDIDEVICEOBJECTDATA out, LPDWORD inout, DWORD fl)
{
    int q = kbd_index(d);
    DWORD max, n = 0;
    HRESULT hr;
    if (q < 0 || !g_shm) return o_GetDeviceData(d, sz, out, inout, fl);
    max = *inout;
    while (n < max && g_krd[q] < g_shm->kwr) {
        uint16_t e = g_shm->kev[g_krd[q] & 63];
        if (out) {
            DIDEVICEOBJECTDATA o = { 0 };
            o.dwOfs = e & 0xff; o.dwData = (e & 0x100) ? 0x80 : 0; o.dwTimeStamp = GetTickCount();
            memcpy((uint8_t *)out + n * sz, &o, sz < sizeof o ? sz : sizeof o);
        }
        n++;
        if (!(fl & DIGDD_PEEK)) g_krd[q]++;
    }
    if (n) { *inout = n; return DI_OK; }
    hr = o_GetDeviceData(d, sz, out, inout, fl);
    if (FAILED(hr)) { *inout = 0; hr = DI_OK; }
    return hr;
}
static void vtp(void **vt, int i, void *r, void **o)
{
    DWORD p;
    if (vt[i] == r) return;
    VirtualProtect(&vt[i], 4, PAGE_EXECUTE_READWRITE, &p);
    *o = vt[i]; vt[i] = r;
    VirtualProtect(&vt[i], 4, p, &p);
}
static HRESULT STDMETHODCALLTYPE h_CreateDeviceDI(IDirectInput8A *di, REFGUID g, LPDIRECTINPUTDEVICE8A *out, LPUNKNOWN u)
{
    HRESULT hr = o_CreateDeviceDI(di, g, out, u);
    if (SUCCEEDED(hr) && IsEqualGUID(g, &GUID_SysKeyboard)) {
        void **vt = *(void ***)*out;
        int i;
        for (i = 0; i < 4; i++) if (!g_kbd[i]) { g_kbd[i] = *out; g_krd[i] = g_shm ? g_shm->kwr : 0; break; }
        vtp(vt, 7, (void *)h_Acquire, (void **)&o_Acquire);
        vtp(vt, 9, (void *)h_GetDeviceState, (void **)&o_GetDeviceState);
        vtp(vt, 10, (void *)h_GetDeviceData, (void **)&o_GetDeviceData);
        vtp(vt, 12, (void *)h_SetEvent, (void **)&o_SetEvent);
        vtp(vt, 13, (void *)h_SetCoop, (void **)&o_SetCoop);
        tlog("DI keyboard %p", *out);
    }
    return hr;
}
static HRESULT (WINAPI *o_DI8Create)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
static HRESULT WINAPI h_DI8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *out, LPUNKNOWN u)
{
    HRESULT hr = o_DI8Create(h, v, r, out, u);
    if (SUCCEEDED(hr)) vtp(*(void ***)*out, 3, (void *)h_CreateDeviceDI, (void **)&o_CreateDeviceDI);
    return hr;
}
/* nudges the game after win.py queued keys (event notification) */
static DWORD WINAPI key_pump(LPVOID p)
{
    LONG seen = 0;
    (void)p;
    for (;;) {
        Sleep(10);
        if (g_shm && g_shm->kwr != seen) {
            int i;
            seen = g_shm->kwr;
            for (i = 0; i < 4; i++) if (g_kevt[i]) SetEvent(g_kevt[i]);
        }
    }
}
static BOOL (WINAPI *o_GetKeyboardState)(PBYTE);
static BOOL WINAPI h_GetKeyboardState(PBYTE k)
{
    BOOL r = o_GetKeyboardState(k);
    int i;
    if (g_shm) for (i = 0; i < 256; i++) k[i] |= g_shm->vkeys[i];
    return r;
}
static SHORT (WINAPI *o_GetKeyState)(int);
static SHORT WINAPI h_GetKeyState(int vk) { SHORT r = o_GetKeyState(vk); if (g_shm && vk >= 0 && vk < 256 && g_shm->vkeys[vk]) r |= (SHORT)0x8000; return r; }
static SHORT (WINAPI *o_GetAsyncKeyState)(int);
static SHORT WINAPI h_GetAsyncKeyState(int vk) { SHORT r = o_GetAsyncKeyState(vk); if (g_shm && vk >= 0 && vk < 256 && g_shm->vkeys[vk]) r |= (SHORT)0x8000; return r; }

void testio_game_dll(HMODULE g)
{
    if (!o_DI8Create) o_DI8Create = iat_hook(g, "DINPUT8.dll", "DirectInput8Create", (void *)h_DI8Create);
    tlog("test: DirectInput8Create hook %s", o_DI8Create ? "ok" : "not found");
    iat_hook(g, "USER32.dll", "GetKeyState", (void *)h_GetKeyState);
}

void testio_init(void)
{
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Shm), "TJP_TESTIO");
    if (m) g_shm = (Shm *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shm));
    if (g_shm) { memset((void *)g_shm, 0, sizeof(Shm)); g_shm->magic = 0x50454f54; }
    o_CreateDXGIFactory1 = iat_hook(NULL, "dxgi.dll", "CreateDXGIFactory1", (void *)h_CreateDXGIFactory1);
    CreateThread(NULL, 0, key_pump, NULL, 0, NULL);
    o_GetKeyboardState = (BOOL (WINAPI *)(PBYTE))GetProcAddress(GetModuleHandleA("user32.dll"), "GetKeyboardState");
    o_GetKeyState = (SHORT (WINAPI *)(int))GetProcAddress(GetModuleHandleA("user32.dll"), "GetKeyState");
    o_GetAsyncKeyState = (SHORT (WINAPI *)(int))GetProcAddress(GetModuleHandleA("user32.dll"), "GetAsyncKeyState");
    iat_hook(NULL, "USER32.dll", "GetKeyboardState", (void *)h_GetKeyboardState);
    iat_hook(NULL, "USER32.dll", "GetKeyState", (void *)h_GetKeyState);
    iat_hook(NULL, "USER32.dll", "GetAsyncKeyState", (void *)h_GetAsyncKeyState);
}
