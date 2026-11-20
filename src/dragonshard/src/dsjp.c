/* Dragonshard Japanese patch: entry point, patch helpers, file redirection, D3D9 test hooks */
#include "dsjp.h"
#include <stdio.h>
#include <stdarg.h>
#include <d3d9.h>

void proxy_init(void);
HRESULT WINAPI real_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPUNKNOWN u);

int g_test;
HMODULE g_self;
static FILE *g_log;
static CRITICAL_SECTION g_logcs;

void dlog(const char *fmt, ...)
{
    va_list ap;
    if (!g_log) return;
    EnterCriticalSection(&g_logcs);
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
    LeaveCriticalSection(&g_logcs);
}

void patch_bytes(uint32_t at, const void *b, int n)
{
    DWORD old;
    VirtualProtect((void *)at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void *)at, b, n);
    VirtualProtect((void *)at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)at, n);
}

/* 書き換える場所を探す。まず Steam 版（英語版 v1.3）の番地 fixed を確かめ、違えば exe のコード（実行できる節）全体から
   pat を探す（mask の '.' は何でもよいバイト = 呼び出し先などの番地）。GOG 版など、同じコードが別の番地にある exe 向け。
   見つからないか、2 か所以上あれば 0 */
static int match(const uint8_t *p, const uint8_t *pat, const char *mask, int n)
{
    int i;
    for (i = 0; i < n; i++) if (mask[i] == 'x' && p[i] != pat[i]) return 0;
    return 1;
}
uint32_t find_code(uint32_t fixed, const uint8_t *pat, const char *mask)
{
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_SECTION_HEADER *sh = IMAGE_FIRST_SECTION(nt);
    int n = (int)strlen(mask), i, hits = 0;
    uint32_t found = 0;
    char fs[4];
    if (match((const uint8_t *)fixed, pat, mask, n) && !(g_test && GetEnvironmentVariableA("DSJP_FORCESCAN", fs, sizeof fs))) return fixed;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sh++) {
        uint8_t *s, *e;
        if (!(sh->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        s = base + sh->VirtualAddress;
        e = s + sh->Misc.VirtualSize - n;
        for (; s <= e; s++) if (*s == pat[0] && match(s, pat, mask, n)) { found = (uint32_t)s; hits++; }
    }
    dlog("find_code %08x: %d hit(s)%s %08x", fixed, hits, hits == 1 ? " ->" : "", found);
    return hits == 1 ? found : 0;
}

static void patch_rel(uint32_t at, uint8_t op, void *to)
{
    uint8_t b[5];
    b[0] = op;
    *(int32_t *)(b + 1) = (int32_t)((uint32_t)to - (at + 5));
    patch_bytes(at, b, 5);
}
void patch_jmp(uint32_t at, void *to) { patch_rel(at, 0xe9, to); }
void patch_call(uint32_t at, void *to) { patch_rel(at, 0xe8, to); }

void *iat_hook(const char *dll, const char *func, void *repl)
{
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress);
    for (; imp->Name; imp++) {
        IMAGE_THUNK_DATA *names, *addrs;
        if (_stricmp((char *)(base + imp->Name), dll)) continue;
        names = (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk);
        addrs = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, addrs++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (!strcmp((char *)ibn->Name, func)) {
                void *old = (void *)addrs->u1.Function;
                DWORD prot;
                VirtualProtect(&addrs->u1.Function, 4, PAGE_READWRITE, &prot);
                addrs->u1.Function = (ULONG_PTR)repl;
                VirtualProtect(&addrs->u1.Function, 4, prot, &prot);
                return old;
            }
        }
    }
    dlog("iat_hook: %s!%s not found", dll, func);
    return NULL;
}

const void *res_get(int id, DWORD *size)
{
    HRSRC r = FindResourceA(g_self, MAKEINTRESOURCEA(id), "DSJP");
    HGLOBAL g;
    if (!r) { *size = 0; return NULL; }
    g = LoadResource(g_self, r);
    *size = SizeofResource(g_self, r);
    return LockResource(g);
}

/* ---------- CreateFileW: translated archives ---------- */
static HANDLE (WINAPI *o_CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static HANDLE WINAPI h_CreateFileW(LPCWSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl)
{
    const WCHAR *r = name ? arch_redirect(name) : NULL;
    HANDLE h = o_CreateFileW(r ? r : name, acc, share, sa, disp, flags, tmpl);
    if (g_test) dlog("CreateFileW %ls%s -> %p", name ? name : L"(null)", r ? L" [JP]" : L"", h);
    return h;
}
static HANDLE (WINAPI *o_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static HANDLE WINAPI h_CreateFileA(LPCSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl)
{
    HANDLE h = o_CreateFileA(name, acc, share, sa, disp, flags, tmpl);
    if (g_test) dlog("CreateFileA %s -> %p", name ? name : "(null)", h);
    return h;
}

/* ---------- test mode: windowed D3D, own window title, no cursor clipping ---------- */
static HRESULT (STDMETHODCALLTYPE *o_CreateDevice)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
static HRESULT (STDMETHODCALLTYPE *o_Reset)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
static HWND g_hwnd;
static HRESULT (STDMETHODCALLTYPE *o_Present)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
static volatile LONG g_presents;
static int g_ex;
static void vt_patch(void **vt, int idx, void *repl, void **orig);
static int g_wx = 40, g_wy = 40;
static BOOL (WINAPI *o_SetWindowPos)(HWND, HWND, int, int, int, int, UINT);
static WNDPROC g_wndproc;
/* test mode: the game pauses when it loses activation; hide that from it */
static volatile LONG g_cx = 512, g_cy = 384;   /* injected cursor position (client coords) */
static BOOL WINAPI h_GetCursorPos(POINT *p)
{
    p->x = g_cx; p->y = g_cy;
    if (g_hwnd) ClientToScreen(g_hwnd, p);
    return TRUE;
}
static LRESULT CALLBACK test_wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    if (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST && m != WM_MOUSEWHEEL) { g_cx = (short)LOWORD(lp); g_cy = (short)HIWORD(lp); }
    switch (m) {
    case WM_SYSCOMMAND:
        dlog("WM_SYSCOMMAND %x", (unsigned)wp);
        if ((wp & 0xfff0) == SC_MINIMIZE) return 0;
        break;
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) { dlog("WM_SIZE minimized (dropped)"); return 0; }
        break;
    case WM_WINDOWPOSCHANGING: {
        WINDOWPOS *wpos = (WINDOWPOS *)lp;
        if (IsIconic(w) || (wpos->x <= -32000)) dlog("WM_WINDOWPOSCHANGING to %d,%d fl %x", wpos->x, wpos->y, wpos->flags);
        break;
    }
    case WM_ACTIVATEAPP: wp = TRUE; break;
    case WM_ACTIVATE: wp = WA_ACTIVE; break;
    case WM_NCACTIVATE: wp = TRUE; break;
    case WM_KILLFOCUS: return 0;
    }
    return CallWindowProcW(g_wndproc, w, m, wp, lp);
}
static LRESULT CALLBACK cwp_hook(int code, WPARAM wp, LPARAM lp)
{
    CWPSTRUCT *c = (CWPSTRUCT *)lp;
    if (code == HC_ACTION && c->hwnd == g_hwnd) {
        UINT m = c->message;
        if (m == WM_SYSCOMMAND || m == WM_ACTIVATEAPP || m == WM_ACTIVATE || (m == WM_SIZE && c->wParam == SIZE_MINIMIZED) ||
            (m == WM_WINDOWPOSCHANGING && ((WINDOWPOS *)c->lParam)->x <= -32000) || m == WM_DISPLAYCHANGE || m == WM_KILLFOCUS) {
            HWND fg = GetForegroundWindow();
            DWORD pid = 0; char t[64] = "";
            GetWindowThreadProcessId(fg, &pid);
            GetWindowTextA(fg, t, sizeof t);
            dlog("msg %04x wp %x lp %x (fg %p pid %u '%s')", m, (unsigned)c->wParam, (unsigned)c->lParam, fg, pid, t);
        }
    }
    return CallNextHookEx(NULL, code, wp, lp);
}
static ATOM (WINAPI *o_RegisterClassW)(const WNDCLASSW *);
static ATOM WINAPI h_RegisterClassW(const WNDCLASSW *wc)
{
    WNDCLASSW c = *wc;
    if (!g_wndproc) { g_wndproc = c.lpfnWndProc; c.lpfnWndProc = test_wndproc; dlog("RegisterClassW %ls", c.lpszClassName); }
    return o_RegisterClassW(&c);
}
static HWND WINAPI h_GetForegroundWindow(void) { return g_hwnd; }
static HWND WINAPI h_GetActiveWindow(void) { return g_hwnd; }
static HWND WINAPI h_GetFocus(void) { return g_hwnd; }

static void force_windowed(D3DPRESENT_PARAMETERS *pp)
{
    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;
    pp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}
static HRESULT STDMETHODCALLTYPE h_Present(IDirect3DDevice9 *d, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    InterlockedIncrement(&g_presents);
    if (g_test) testio_present(d);
    return o_Present(d, a, b, w, r);
}
static HRESULT (STDMETHODCALLTYPE *o_VBLock)(IDirect3DVertexBuffer9 *, UINT, UINT, void **, DWORD);
static HRESULT STDMETHODCALLTYPE h_VBLock(IDirect3DVertexBuffer9 *b, UINT o, UINT n, void **p, DWORD fl)
{
    HRESULT hr = o_VBLock(b, o, n, p, fl);
    if (FAILED(hr)) {
        MEMORYSTATUSEX ms; ms.dwLength = sizeof ms; GlobalMemoryStatusEx(&ms);
        dlog("VB Lock %u+%u fl %x -> %08x (avail virtual %llu MB)", o, n, fl, hr, ms.ullAvailVirtual >> 20);
    }
    return hr;
}
static HRESULT (STDMETHODCALLTYPE *o_CreateVB0)(IDirect3DDevice9 *, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9 **, HANDLE *);
static HRESULT STDMETHODCALLTYPE h_CreateVB0(IDirect3DDevice9 *d, UINT n, DWORD u, DWORD fvf, D3DPOOL p, IDirect3DVertexBuffer9 **o, HANDLE *s)
{
    HRESULT hr = o_CreateVB0(d, n, u, fvf, p, o, s);
    if (FAILED(hr)) {
        MEMORYSTATUSEX ms; ms.dwLength = sizeof ms; GlobalMemoryStatusEx(&ms);
        dlog("CreateVertexBuffer %u u%x p%d -> %08x (avail virtual %llu MB)", n, u, p, hr, ms.ullAvailVirtual >> 20);
    } else if (!o_VBLock) { void **vt = *(void ***)*o; vt_patch(vt, 11, (void *)h_VBLock, (void **)&o_VBLock); }
    return hr;
}
static HRESULT (STDMETHODCALLTYPE *o_TestCoop)(IDirect3DDevice9 *);
static HRESULT STDMETHODCALLTYPE h_TestCoop(IDirect3DDevice9 *d)
{
    static HRESULT last;
    HRESULT hr = o_TestCoop(d);
    if (hr != last) { dlog("TestCooperativeLevel -> %08x", hr); last = hr; }
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_Reset(IDirect3DDevice9 *d, D3DPRESENT_PARAMETERS *pp)
{
    HRESULT hr;
    force_windowed(pp);
    hr = o_Reset(d, pp);
    dlog("Reset %dx%d -> %08x", pp->BackBufferWidth, pp->BackBufferHeight, hr);
    return hr;
}
static void vt_patch(void **vt, int idx, void *repl, void **orig)
{
    DWORD prot;
    if (vt[idx] == repl) return;
    VirtualProtect(&vt[idx], 4, PAGE_EXECUTE_READWRITE, &prot);
    *orig = vt[idx];
    vt[idx] = repl;
    VirtualProtect(&vt[idx], 4, prot, &prot);
}
static IDirect3DDevice9 *g_dev;
static void ex_device_hooks(IDirect3DDevice9 *dev);
static HRESULT STDMETHODCALLTYPE h_Present(IDirect3DDevice9 *d, const RECT *a, const RECT *b, HWND w, const RGNDATA *r);
/* d3d9's method implementations start with the hot-patch prologue (mov edi,edi with 5 bytes of padding
   before it); patching the code catches every call, whatever d3d9 does to the device vtables
   (it does put its own pointers back, so vtable hooks alone let a fullscreen Reset slip through) */
static int hotpatch(uint8_t *f, void *hook, void **orig, const char *what)
{
    static const uint8_t jshort[2] = { 0xeb, 0xf9 };
    if (f[0] == 0xeb && f[1] == 0xf9) { dlog("%s: already hot-patched", what); return 1; }
    if (f[0] != 0x8b || f[1] != 0xff || f[-5] != 0xcc || f[-1] != 0xcc) { dlog("%s: no hot-patch prologue", what); return 0; }
    *orig = (void *)(f + 2);
    patch_jmp((uint32_t)(f - 5), hook);
    patch_bytes((uint32_t)f, jshort, 2);
    dlog("%s hot-patched at %p", what, f);
    return 1;
}
/* test mode: other programs on this desktop (full screen games, Win+D ...) minimize or deactivate the
   test window now and then; the game then stops (active flag 0xE67148 = 0, blocking GetMessage). */
static DWORD WINAPI keepalive_thread(LPVOID p)
{
    (void)p;
    for (;;) {
        Sleep(200);
        if (!g_hwnd) continue;
        if (IsIconic(g_hwnd)) { dlog("keepalive: window was minimized, restoring"); ShowWindow(g_hwnd, SW_SHOWNOACTIVATE); }
        if (*(volatile uint8_t *)0xaa5571) {      /* game's device-lost flag */
            static DWORD since; static HRESULT lasthr = 1;
            if (!since) since = GetTickCount();
            if (GetTickCount() - since > 3000 && g_dev) {
                HRESULT hr = IDirect3DDevice9_TestCooperativeLevel(g_dev);
                if (hr != lasthr) { dlog("device lost for %us, TestCooperativeLevel %08x iconic %d", (GetTickCount() - since) / 1000, hr, IsIconic(g_hwnd)); lasthr = hr; }
            }
        } else {
            static int x; (void)x;
        }
        if (*(volatile uint8_t *)0xe67148 == 0) {
            *(volatile uint8_t *)0xe67148 = 1;
            PostMessageW(g_hwnd, WM_NULL, 0, 0);
        }
    }
}
static DWORD WINAPI watch_thread(LPVOID p)
{
    LONG last = -1;
    (void)p;
    for (;;) {
        Sleep(1000);
        if (!g_dev) continue;
        if (g_presents == last) dlog("no Present for 1s (iconic=%d)", IsIconic(g_hwnd));
        last = g_presents;
    }
}
static HRESULT STDMETHODCALLTYPE h_CreateDevice(IDirect3D9 *d3d, UINT ad, D3DDEVTYPE t, HWND w, DWORD fl, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    HRESULT hr;
    force_windowed(pp);
    dlog("CreateDevice %dx%d fmt %d cnt %d ms %d/%d swap %d dw %p ds %d/%d fl %x pi %x behav %x hwnd %p", pp->BackBufferWidth, pp->BackBufferHeight, pp->BackBufferFormat,
         pp->BackBufferCount, pp->MultiSampleType, pp->MultiSampleQuality, pp->SwapEffect, pp->hDeviceWindow, pp->EnableAutoDepthStencil, pp->AutoDepthStencilFormat,
         pp->Flags, pp->PresentationInterval, fl, w);
    if (pp->SwapEffect == D3DSWAPEFFECT_FLIP) pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
    if (!g_hwnd) {
        g_hwnd = w;
        SetWindowsHookExW(WH_CALLWNDPROC, cwp_hook, NULL, GetCurrentThreadId());
        SetWindowTextW(w, L"DSJP TEST");
        SetWindowLongW(w, GWL_EXSTYLE, GetWindowLongW(w, GWL_EXSTYLE) & ~WS_EX_TOPMOST);
        o_SetWindowPos ? o_SetWindowPos(w, HWND_NOTOPMOST, g_wx, g_wy, pp->BackBufferWidth, pp->BackBufferHeight, SWP_NOACTIVATE)
                       : SetWindowPos(w, HWND_NOTOPMOST, g_wx, g_wy, pp->BackBufferWidth, pp->BackBufferHeight, SWP_NOACTIVATE);
    }
    hr = o_CreateDevice(d3d, ad, t, w, fl, pp, out);
    if (FAILED(hr)) {   /* D3DERR_NOTAVAILABLE while another program holds exclusive mode; retry */
        int i;
        for (i = 0; i < 120 && FAILED(hr); i++) { Sleep(1000); hr = o_CreateDevice(d3d, ad, t, w, fl, pp, out); dlog("CreateDevice retry -> %08x", hr); }
    }
    if (SUCCEEDED(hr)) {
        void **vt = *(void ***)*out;
        if (!o_Reset && !hotpatch((uint8_t *)vt[16], (void *)h_Reset, (void **)&o_Reset, "Reset")) {
            /* without a working Reset hook the game would switch the desktop to full screen: refuse */
            dlog("cannot hook Reset, test mode stops here");
            ExitProcess(1);
        }
        if (!o_Present) hotpatch((uint8_t *)vt[17], (void *)h_Present, (void **)&o_Present, "Present");
        ex_device_hooks(*out);
        if (!g_dev) { g_dev = *out; CreateThread(NULL, 0, watch_thread, NULL, 0, NULL); CreateThread(NULL, 0, keepalive_thread, NULL, 0, NULL); }
    } else dlog("CreateDevice failed %08x", hr);
    return hr;
}
/* Test mode runs on D3D9Ex: a plain D3D9 device cannot be created (D3DERR_NOTAVAILABLE) while another
   program on this desktop holds exclusive full screen. D3D9Ex has no managed pool, so managed resources
   become dynamic default-pool ones (lockable, never lost on an Ex device). */
static HRESULT (STDMETHODCALLTYPE *o_CreateTexture)(IDirect3DDevice9 *, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9 **, HANDLE *);
static HRESULT (STDMETHODCALLTYPE *o_CreateVolumeTexture)(IDirect3DDevice9 *, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9 **, HANDLE *);
static HRESULT (STDMETHODCALLTYPE *o_CreateCubeTexture)(IDirect3DDevice9 *, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9 **, HANDLE *);
static HRESULT (STDMETHODCALLTYPE *o_CreateVertexBuffer)(IDirect3DDevice9 *, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9 **, HANDLE *);
static HRESULT (STDMETHODCALLTYPE *o_CreateIndexBuffer)(IDirect3DDevice9 *, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9 **, HANDLE *);
static void fix_pool(DWORD *usage, D3DPOOL *pool)
{
    if (*pool == D3DPOOL_MANAGED) { *pool = D3DPOOL_DEFAULT; *usage |= D3DUSAGE_DYNAMIC; }
}
static HRESULT (STDMETHODCALLTYPE *o_TexLock)(IDirect3DTexture9 *, UINT, D3DLOCKED_RECT *, const RECT *, DWORD);
static HRESULT STDMETHODCALLTYPE h_TexLock(IDirect3DTexture9 *t, UINT l, D3DLOCKED_RECT *lr, const RECT *r, DWORD fl)
{
    HRESULT hr = o_TexLock(t, l, lr, r, fl);
    if (FAILED(hr)) dlog("Texture LockRect l%u fl%x -> %08x", l, fl, hr);
    return hr;
}
static void __stdcall log_texload(int id, int ok) { dlog("texload %d -> %d", id, ok); }
static __declspec(naked) void h_texload(void)
{
    __asm {
        push dword ptr [esp + 8]
        push dword ptr [esp + 8]
        mov eax, 0x655dd0
        call eax
        movzx eax, al
        push eax
        push eax
        push dword ptr [esp + 0x0c]
        call log_texload
        pop eax
        ret 8
    }
}
static HRESULT STDMETHODCALLTYPE h_CreateTexture(IDirect3DDevice9 *d, UINT w, UINT h, UINT l, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DTexture9 **o, HANDLE *s)
{
    HRESULT hr;
    D3DPOOL p0 = p;
    fix_pool(&u, &p);
    hr = o_CreateTexture(d, w, h, l, u, f, p, o, s);
    if (SUCCEEDED(hr) && !o_TexLock) { void **vt = *(void ***)*o; vt_patch(vt, 19, (void *)h_TexLock, (void **)&o_TexLock); }
    if (FAILED(hr) || g_test > 1) dlog("CreateTexture %ux%u l%u u%x f%d p%d->%d -> %08x", w, h, l, u, f, p0, p, hr);
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_CreateVolumeTexture(IDirect3DDevice9 *d, UINT w, UINT h, UINT dd, UINT l, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DVolumeTexture9 **o, HANDLE *s)
{ fix_pool(&u, &p); return o_CreateVolumeTexture(d, w, h, dd, l, u, f, p, o, s); }
static HRESULT STDMETHODCALLTYPE h_CreateCubeTexture(IDirect3DDevice9 *d, UINT e, UINT l, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DCubeTexture9 **o, HANDLE *s)
{ fix_pool(&u, &p); return o_CreateCubeTexture(d, e, l, u, f, p, o, s); }
static HRESULT STDMETHODCALLTYPE h_CreateVertexBuffer(IDirect3DDevice9 *d, UINT n, DWORD u, DWORD fvf, D3DPOOL p, IDirect3DVertexBuffer9 **o, HANDLE *s)
{ fix_pool(&u, &p); return o_CreateVertexBuffer(d, n, u, fvf, p, o, s); }
static HRESULT STDMETHODCALLTYPE h_CreateIndexBuffer(IDirect3DDevice9 *d, UINT n, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DIndexBuffer9 **o, HANDLE *s)
{ fix_pool(&u, &p); return o_CreateIndexBuffer(d, n, u, f, p, o, s); }
static HRESULT STDMETHODCALLTYPE h_EvictManaged(IDirect3DDevice9 *d) { (void)d; return D3D_OK; }
static UINT STDMETHODCALLTYPE h_AvailTexMem(IDirect3DDevice9 *d) { (void)d; return 0x7ff00000u; }

static IDirect3D9 *(WINAPI *o_Direct3DCreate9)(UINT);
static HRESULT (STDMETHODCALLTYPE *o_CheckDeviceFormat)(IDirect3D9 *, UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT);
static HRESULT STDMETHODCALLTYPE h_CheckDeviceFormat(IDirect3D9 *d, UINT a, D3DDEVTYPE t, D3DFORMAT af, DWORD u, D3DRESOURCETYPE rt, D3DFORMAT f)
{
    HRESULT hr = o_CheckDeviceFormat(d, a, t, af, u, rt, f);
    if (g_test > 1 || FAILED(hr)) dlog("CheckDeviceFormat af%d u%x rt%d f%d -> %08x", af, u, rt, f, hr);
    return hr;
}
static IDirect3D9 *WINAPI h_Direct3DCreate9(UINT v)
{
    IDirect3D9 *d = NULL;
    typedef HRESULT (WINAPI *ExFn)(UINT, IDirect3D9Ex **);
    ExFn ex = (ExFn)GetProcAddress(GetModuleHandleA("d3d9.dll"), "Direct3DCreate9Ex");
    char e[8];
    if (GetEnvironmentVariableA("DSJP_EX", e, sizeof e) && e[0] == '1' && ex && SUCCEEDED(ex(v, (IDirect3D9Ex **)&d))) g_ex = 1;
    else d = o_Direct3DCreate9(v);
    dlog("Direct3DCreate9 ex=%d", g_ex);
    if (d) vt_patch(*(void ***)d, 16, (void *)h_CreateDevice, (void **)&o_CreateDevice);
    if (d) vt_patch(*(void ***)d, 10, (void *)h_CheckDeviceFormat, (void **)&o_CheckDeviceFormat);
    return d;
}
/* Ex mode: texture 0xC5 (clouds) is not found; carry on with a 256x256 copy instead of crashing */
static __declspec(naked) void clouds_fix(void)
{
    __asm {
        mov dword ptr [edi + 4], eax
        test eax, eax
        je none
        mov eax, dword ptr [eax + 8]
        mov ecx, dword ptr [eax + 0x14]
        mov eax, dword ptr [eax + 0x10]
        jmp back
    none:
        mov ecx, 256
        mov eax, 256
    back:
        push 0x5883c6
        ret
    }
}
static void ex_device_hooks(IDirect3DDevice9 *dev)
{
    if (g_ex) patch_jmp(0x5883b7, (void *)clouds_fix);
    void **vt = *(void ***)dev;
    if (!g_ex) return;
    { void *o; vt_patch(vt, 4, (void *)h_AvailTexMem, &o); vt_patch(vt, 5, (void *)h_EvictManaged, &o); }
    vt_patch(vt, 23, (void *)h_CreateTexture, (void **)&o_CreateTexture);
    vt_patch(vt, 24, (void *)h_CreateVolumeTexture, (void **)&o_CreateVolumeTexture);
    vt_patch(vt, 25, (void *)h_CreateCubeTexture, (void **)&o_CreateCubeTexture);
    vt_patch(vt, 26, (void *)h_CreateVertexBuffer, (void **)&o_CreateVertexBuffer);
    vt_patch(vt, 27, (void *)h_CreateIndexBuffer, (void **)&o_CreateIndexBuffer);
}

/* test window: titled "DSJP TEST", kept where it does not cover the user's work */
static HWND (WINAPI *o_CreateWindowExW)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
static HWND WINAPI h_CreateWindowExW(DWORD ex, LPCWSTR cls, LPCWSTR title, DWORD st, int x, int y, int w, int h, HWND par, HMENU m, HINSTANCE inst, LPVOID p)
{
    HWND r;
    if (!par) { ex &= ~WS_EX_TOPMOST; ex |= WS_EX_NOACTIVATE; title = L"DSJP TEST"; x = g_wx; y = g_wy; }
    r = o_CreateWindowExW(ex, cls, title, st, x, y, w, h, par, m, inst, p);
    if (!par && !g_hwnd) g_hwnd = r;
    dlog("CreateWindowExW %08x %d,%d %dx%d -> %p", st, x, y, w, h, r);
    return r;
}
static BOOL WINAPI h_SetWindowPos(HWND w, HWND after, int x, int y, int cx, int cy, UINT fl)
{
    if (w == g_hwnd) {
        x = g_wx; y = g_wy;
        if (after == HWND_TOPMOST || after == HWND_TOP) fl |= SWP_NOZORDER;
        fl |= SWP_NOACTIVATE;
    }
    return o_SetWindowPos(w, after, x, y, cx, cy, fl);
}
static BOOL WINAPI h_SetWindowTextW(HWND w, LPCWSTR t) { (void)t; return SetWindowTextW(w, L"DSJP TEST"); }
static BOOL WINAPI h_ShowWindow(HWND w, int cmd)
{
    dlog("ShowWindow %p %d", w, cmd);
    if (w == g_hwnd && (cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED || cmd == SW_SHOWMINNOACTIVE || cmd == SW_FORCEMINIMIZE || cmd == SW_HIDE)) return TRUE;
    if (cmd == SW_SHOW || cmd == SW_SHOWNORMAL || cmd == SW_RESTORE) cmd = SW_SHOWNOACTIVATE;
    return ShowWindow(w, cmd);
}
static BOOL WINAPI h_ClipCursor(const RECT *r) { (void)r; return TRUE; }
static BOOL WINAPI h_SetForegroundWindow(HWND w) { (void)w; return TRUE; }
static HWND WINAPI h_SetActiveWindow(HWND w) { (void)w; return NULL; }
static HWND WINAPI h_SetFocus(HWND w) { (void)w; return NULL; }
static BOOL WINAPI h_BringWindowToTop(HWND w) { (void)w; return TRUE; }

HRESULT WINAPI p_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPUNKNOWN u)
{
    HRESULT hr = real_DirectInput8Create(h, v, r, o, u);
    if (g_test && SUCCEEDED(hr)) testio_wrap_dinput(*o);
    return hr;
}

static void init(HMODULE self)
{
    char buf[MAX_PATH];
    /* cmp [esp+0Ch],'FONT'; jne ..; fld [esp+10h]; fcomp [..]; fnstsw ax; test ah,41h（フォントを読む関数。この exe かどうかの目印） */
    static const uint8_t sig[] = { 0x81, 0x7c, 0x24, 0x0c, 0x46, 0x4f, 0x4e, 0x54, 0x75, 0x3b, 0xd9, 0x44, 0x24, 0x10, 0xd8, 0x1d,
                                   0, 0, 0, 0, 0xdf, 0xe0, 0xf6, 0xc4, 0x41 };
    InitializeCriticalSection(&g_logcs);
    if (GetEnvironmentVariableA("DSJP_TEST", buf, sizeof buf) && buf[0] >= '1') g_test = buf[0] - '0';
    if (GetEnvironmentVariableA("DSJP_LOG", buf, sizeof buf)) g_log = fopen(buf, "w");
    if (!find_code(0x6bd534, sig, "xxxxxxxxxxxxxxxx....xxxxx")) { dlog("unknown exe, patch disabled"); return; }
    dlog("dsjp start test=%d", g_test);
    o_CreateFileW = iat_hook("KERNEL32.dll", "CreateFileW", (void *)h_CreateFileW);
    if (g_test) o_CreateFileA = iat_hook("KERNEL32.dll", "CreateFileA", (void *)h_CreateFileA);
    if (g_test) {
        if (GetEnvironmentVariableA("DSJP_WX", buf, sizeof buf)) g_wx = atoi(buf);
        if (GetEnvironmentVariableA("DSJP_WY", buf, sizeof buf)) g_wy = atoi(buf);
        o_Direct3DCreate9 = iat_hook("d3d9.dll", "Direct3DCreate9", (void *)h_Direct3DCreate9);
        o_CreateWindowExW = iat_hook("USER32.dll", "CreateWindowExW", (void *)h_CreateWindowExW);
        o_SetWindowPos = iat_hook("USER32.dll", "SetWindowPos", (void *)h_SetWindowPos);
        iat_hook("USER32.dll", "SetWindowTextW", (void *)h_SetWindowTextW);
        iat_hook("USER32.dll", "ClipCursor", (void *)h_ClipCursor);
        iat_hook("USER32.dll", "ShowWindow", (void *)h_ShowWindow);
        iat_hook("USER32.dll", "GetCursorPos", (void *)h_GetCursorPos);
        iat_hook("USER32.dll", "SetForegroundWindow", (void *)h_SetForegroundWindow);
        iat_hook("USER32.dll", "SetActiveWindow", (void *)h_SetActiveWindow);
        iat_hook("USER32.dll", "SetFocus", (void *)h_SetFocus);
        iat_hook("USER32.dll", "BringWindowToTop", (void *)h_BringWindowToTop);
        o_RegisterClassW = iat_hook("USER32.dll", "RegisterClassW", (void *)h_RegisterClassW);
        iat_hook("USER32.dll", "GetForegroundWindow", (void *)h_GetForegroundWindow);
        iat_hook("USER32.dll", "GetActiveWindow", (void *)h_GetActiveWindow);
        iat_hook("USER32.dll", "GetFocus", (void *)h_GetFocus);
        testio_init();
        *(volatile uint8_t *)0xaa53b0 = 1;
        if (g_test > 1) patch_call(0x6448a9, (void *)h_texload);   /* same as the nointro command line switch */
    }
    if (g_test && GetEnvironmentVariableA("DSJP_NOJP", buf, sizeof buf)) { dlog("NOJP: Japanese disabled"); return; }
    if (!font_init()) return;
    arch_init();
    wrap_init();
    glyph_init();
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        g_self = h;
        proxy_init();
        init(h);
    }
    return TRUE;
}
