/* Forgotten Realms: Demon Stone 日本語化パッチ（dinput8.dll の代理）
   - ゲームが UI\*.OBX（画面の文字）、subtitle\*.OBX（字幕）、UI\font*.XFT（フォント）を開くとき、
     日本語にしたコピー（%TEMP%\DemonStoneJP）を渡す（files.c）
   - 折り返しで日本語の字も区切りとして扱う（wrap.c）
   テスト（DSTJP_TEST=1）: 窓のまま動かし（全画面にしない）、画面を撮り、キーを送る（testio.c） */
#include "dst.h"
#include "resource.h"
#ifdef DSTJP_TEST
#include <stdio.h>
#include <intrin.h>
#include <d3d9.h>
#endif

HMODULE g_self;
#ifdef DSTJP_TEST
int g_test;
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
#endif

void patch_bytes(uint32_t at, const void *b, int n)
{
    DWORD old;
    VirtualProtect((void *)at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void *)at, b, n);
    VirtualProtect((void *)at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)at, n);
}
void patch_jmp(uint32_t at, void *to)
{
    uint8_t b[5];
    b[0] = 0xe9;
    *(int32_t *)(b + 1) = (int32_t)((uint32_t)to - (at + 5));
    patch_bytes(at, b, 5);
}

/* 書き換える場所を探す: まず Steam 版の番地 fixed を確かめ、違えば exe のコード全体から pat を探す
   （mask の '.' は何でもよいバイト）。GOG 版など、同じコードが別の番地にある exe 向け。見つからない／2 か所以上なら 0 */
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
#ifdef DSTJP_TEST
    char fs[4];
    if (match((const uint8_t *)fixed, pat, mask, n) && !(g_test && GetEnvironmentVariableA("DSTJP_FORCESCAN", fs, sizeof fs))) return fixed;
#else
    if (match((const uint8_t *)fixed, pat, mask, n)) return fixed;
#endif
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sh++) {
        uint8_t *s, *e;
        if (!(sh->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        s = base + sh->VirtualAddress;
        e = s + sh->Misc.VirtualSize - n;
        for (; s <= e; s++) if (*s == pat[0] && match(s, pat, mask, n)) { found = (uint32_t)s; hits++; }
    }
    dlog("find_code %08x: %d hit(s) -> %08x", fixed, hits, hits == 1 ? found : 0);
    return hits == 1 ? found : 0;
}

void *iat_hook_mod(HMODULE mod, const char *dll, const char *func, void *repl)
{
    uint8_t *base = (uint8_t *)mod;
    IMAGE_NT_HEADERS *nt;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    if (!base) return NULL;
    nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imp->Name; imp++) {
        IMAGE_THUNK_DATA *ot, *ft;
        if (_stricmp((const char *)(base + imp->Name), dll)) continue;
        ot = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        ft = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;
            void *o;
            if (ot->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)(base + ot->u1.AddressOfData);
            if (strcmp((const char *)ibn->Name, func)) continue;
            o = (void *)ft->u1.Function;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
            ft->u1.Function = (DWORD)(uintptr_t)repl;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            return o;
        }
    }
    return NULL;
}
static void *iat_hook(const char *dll, const char *func, void *repl) { return iat_hook_mod(GetModuleHandleA(NULL), dll, func, repl); }

const void *res_get(int id, DWORD *size)
{
    HRSRC r = FindResourceA(g_self, MAKEINTRESOURCEA(id), (LPCSTR)RT_RCDATA);
    HGLOBAL g;
    if (!r) return NULL;
    g = LoadResource(g_self, r);
    *size = SizeofResource(g_self, r);
    return LockResource(g);
}

static WCHAR g_work[MAX_PATH];
const WCHAR *work_dir(void)
{
    if (!g_work[0]) {
        GetTempPathW(MAX_PATH, g_work);
        wcscat(g_work, L"DemonStoneJP\\");
        CreateDirectoryW(g_work, NULL);
    }
    return g_work;
}

/* ---------------------------------------------------------------- ファイルの差し替え */
static HANDLE (WINAPI *o_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static HANDLE WINAPI h_CreateFileA(LPCSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl)
{
    const char *r = (name && !(acc & GENERIC_WRITE)) ? files_redirect(name) : NULL;
    HANDLE h = o_CreateFileA(r ? r : name, acc, share, sa, disp, flags, tmpl);
    if (g_test > 1) dlog("CreateFileA %s%s%s flags %08x -> %p", name, r ? " -> " : "", r ? r : "", flags, h);
    return h;
}

#ifdef DSTJP_TEST
static BOOL (WINAPI *o_ReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
static BOOL WINAPI h_ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, LPOVERLAPPED ov)
{
    BOOL r = o_ReadFile(h, buf, n, got, ov);
    DWORD e = r ? 0 : GetLastError();
    if (!r && e != ERROR_IO_PENDING) dlog("ReadFile %p buf %p n %08x ov %p fail %u", h, buf, n, ov, e);
    SetLastError(e);
    return r;
}

/* ---------------------------------------------------------------- テスト: 窓のまま動かす */
static HWND g_hwnd;
static int g_wx = -3200, g_wy = 0;   /* テストの窓は画面の外に置く（ゲームはデスクトップと同じ大きさの窓を作る） */
static HRESULT (STDMETHODCALLTYPE *o_CreateDevice)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
static HRESULT (STDMETHODCALLTYPE *o_Reset)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
static HRESULT (STDMETHODCALLTYPE *o_Present)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);

static void force_windowed(D3DPRESENT_PARAMETERS *pp)
{
    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;
    pp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (pp->SwapEffect == D3DSWAPEFFECT_FLIP) pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
}
static HRESULT STDMETHODCALLTYPE h_Present(IDirect3DDevice9 *d, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    testio_present(d);
    return o_Present(d, a, b, w, r);
}
static HRESULT STDMETHODCALLTYPE h_Reset(IDirect3DDevice9 *d, D3DPRESENT_PARAMETERS *pp)
{
    HRESULT hr;
    force_windowed(pp);
    hr = o_Reset(d, pp);
    dlog("Reset %dx%d -> %08x", pp->BackBufferWidth, pp->BackBufferHeight, hr);
    return hr;
}
/* d3d9 の関数は hot-patch 用の前置き（mov edi,edi と前の 5 バイトの余白）で始まる。
   そこに飛び先を書けば、d3d9 が vtable を戻しても必ず通る（vtable だけでは全画面の Reset がすり抜ける） */
static int hotpatch(uint8_t *f, void *hook, void **orig, const char *what)
{
    static const uint8_t jshort[2] = { 0xeb, 0xf9 };
    if (f[0] == 0xeb && f[1] == 0xf9) return 1;
    if (f[0] != 0x8b || f[1] != 0xff || f[-5] != 0xcc || f[-1] != 0xcc) { dlog("%s: no hot-patch prologue", what); return 0; }
    *orig = (void *)(f + 2);
    patch_jmp((uint32_t)(f - 5), hook);
    patch_bytes((uint32_t)f, jshort, 2);
    return 1;
}
static void fit_window(HWND w, int cw, int ch)
{
    RECT r = { 0, 0, cw, ch };
    LONG st = GetWindowLongA(w, GWL_STYLE);
    st = (st & ~(WS_POPUP | WS_MAXIMIZE)) | WS_OVERLAPPEDWINDOW;
    SetWindowLongA(w, GWL_STYLE, st);
    SetWindowLongA(w, GWL_EXSTYLE, GetWindowLongA(w, GWL_EXSTYLE) & ~WS_EX_TOPMOST);
    AdjustWindowRectEx(&r, st, FALSE, GetWindowLongA(w, GWL_EXSTYLE));
    SetWindowPos(w, HWND_NOTOPMOST, g_wx, g_wy, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
static HRESULT STDMETHODCALLTYPE h_CreateDevice(IDirect3D9 *d3d, UINT ad, D3DDEVTYPE t, HWND w, DWORD fl, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    HRESULT hr;
    force_windowed(pp);
    if (!g_hwnd) g_hwnd = w;
    hr = o_CreateDevice(d3d, ad, t, w, fl, pp, out);
    dlog("CreateDevice %dx%d hwnd %p -> %08x", pp->BackBufferWidth, pp->BackBufferHeight, w, hr);
    if (SUCCEEDED(hr)) {
        void **vt = *(void ***)*out;
        hotpatch((uint8_t *)vt[16], (void *)h_Reset, (void **)&o_Reset, "Reset");
        hotpatch((uint8_t *)vt[17], (void *)h_Present, (void **)&o_Present, "Present");
        SetWindowPos(w, HWND_NOTOPMOST, g_wx, g_wy, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    return hr;
}
static IDirect3D9 *(WINAPI *o_Direct3DCreate9)(UINT);
static IDirect3D9 *WINAPI h_Direct3DCreate9(UINT v)
{
    IDirect3D9 *d = o_Direct3DCreate9(v);
    dlog("Direct3DCreate9(%u) -> %p", v, d);
    if (d && (*(void ***)d)[16] != (void *)h_CreateDevice) {       /* 2 回目は同じ vtable（覚え直すと自分を呼び続ける） */
        void **vt = *(void ***)d;
        DWORD p;
        VirtualProtect(&vt[16], 4, PAGE_EXECUTE_READWRITE, &p);
        o_CreateDevice = (void *)vt[16];
        vt[16] = (void *)h_CreateDevice;
        VirtualProtect(&vt[16], 4, p, &p);
    }
    return d;
}
static HWND (WINAPI *o_CreateWindowExA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
static HWND WINAPI h_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR title, DWORD st, int x, int y, int w, int h, HWND par, HMENU m, HINSTANCE inst, LPVOID p)
{
    HWND r;
    if (!par) {
        ex &= ~WS_EX_TOPMOST; ex |= WS_EX_NOACTIVATE; title = "DSTJP TEST"; x = g_wx; y = g_wy; st = (st & ~WS_POPUP) | WS_OVERLAPPEDWINDOW;
    }
    r = o_CreateWindowExA(ex, cls, title, st, x, y, w, h, par, m, inst, p);
    if (!par && !g_hwnd) g_hwnd = r;
    dlog("CreateWindowExA %08x %d,%d %dx%d -> %p", st, x, y, w, h, r);
    return r;
}
static BOOL WINAPI h_ShowWindow(HWND w, int cmd)
{
    if (cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED || cmd == SW_HIDE || cmd == SW_SHOWMAXIMIZED) cmd = SW_SHOWNOACTIVATE;
    if (cmd == SW_SHOW || cmd == SW_SHOWNORMAL || cmd == SW_RESTORE) cmd = SW_SHOWNOACTIVATE;
    return ShowWindow(w, cmd);
}
static LONG WINAPI h_SetWindowLongA(HWND w, int i, LONG v) { if (w == g_hwnd && (i == GWL_STYLE || i == GWL_EXSTYLE)) return GetWindowLongA(w, i); return SetWindowLongA(w, i, v); }
static LONG WINAPI h_SetWindowLongW(HWND w, int i, LONG v) { if (w == g_hwnd && (i == GWL_STYLE || i == GWL_EXSTYLE)) return GetWindowLongW(w, i); return SetWindowLongW(w, i, v); }
static BOOL WINAPI h_SetWindowPos(HWND w, HWND after, int x, int y, int cx, int cy, UINT fl) { (void)after; (void)x; (void)y; (void)cx; (void)cy; (void)fl; return w ? TRUE : FALSE; }
static BOOL WINAPI h_True(HWND w) { (void)w; return TRUE; }
static HWND WINAPI h_NullHwnd(HWND w) { (void)w; return NULL; }
/* dsfix.dll は User.ini の解像度（なければ画面の大きさ）で窓を作る。テストでは 1024x768 にする */
static UINT WINAPI h_GetPrivateProfileIntW(LPCWSTR sec, LPCWSTR key, INT def, LPCWSTR file)
{
    UINT r;
    if (key && !_wcsicmp(key, L"ResolutionX")) r = 1024;
    else if (key && !_wcsicmp(key, L"ResolutionY")) r = 768;
    else if (key && !_wcsicmp(key, L"DisplayMode")) r = 2;        /* 枠付きの窓 */
    else if (key && !_wcsicmp(key, L"SkipIntroMovies")) r = 1;
    else r = GetPrivateProfileIntW(sec, key, def, file);
    dlog("GetPrivateProfileIntW %ls %ls def %d -> %u", sec ? sec : L"", key ? key : L"", def, r);
    return r;
}
static BOOL WINAPI h_EnumDisplaySettingsW(LPCWSTR dev, DWORD mode, DEVMODEW *dm)
{
    BOOL r = EnumDisplaySettingsW(dev, mode, dm);
    if (r && mode == ENUM_CURRENT_SETTINGS) { dm->dmPelsWidth = 1024; dm->dmPelsHeight = 768; }
    return r;
}
static BOOL WINAPI h_PathFileExistsA(LPCSTR p)
{
    const char *b = p ? strrchr(p, '\\') : NULL;
    if (b && !_stricmp(b + 1, "User.ini")) return TRUE;
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}
static BOOL WINAPI h_WritePrivateProfileStringW(LPCWSTR a, LPCWSTR b, LPCWSTR c, LPCWSTR d) { (void)a; (void)b; (void)c; (void)d; return TRUE; }

#endif /* DSTJP_TEST */

HRESULT WINAPI real_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPUNKNOWN u);
HRESULT WINAPI p_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPUNKNOWN u)
{
    HRESULT hr = real_DirectInput8Create(h, v, r, o, u);
#ifdef DSTJP_TEST
    if (g_test && SUCCEEDED(hr)) testio_wrap_dinput(*o);
#endif
    return hr;
}

#ifdef DSTJP_TEST
static LONG CALLBACK veh(EXCEPTION_POINTERS *e)
{
    DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == 0xc0000005 || c == 0xc0000094 || c == 0xc000001d || c == 0xc00000fd) {
        CONTEXT *x = e->ContextRecord;
        HMODULE m = NULL;
        char mn[MAX_PATH] = "?";
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)x->Eip, &m);
        if (m) GetModuleFileNameA(m, mn, MAX_PATH);
        dlog("EXCEPTION %08x at %08x (%s+%x) eax %08x ebx %08x ecx %08x edx %08x esi %08x edi %08x esp %08x [%08x %08x]",
             c, x->Eip, mn, x->Eip - (DWORD)m, x->Eax, x->Ebx, x->Ecx, x->Edx, x->Esi, x->Edi, x->Esp,
             (DWORD)e->ExceptionRecord->ExceptionInformation[0], (DWORD)e->ExceptionRecord->ExceptionInformation[1]);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void log_stack(const char *what, void *ret)
{
    uint32_t *sp = (uint32_t *)_AddressOfReturnAddress(), i;
    dlog("%s called from %p", what, ret);
    for (i = 0; i < 64; i++) if (sp[i] >= 0x401000 && sp[i] < 0x520000) dlog("   stack %08x", sp[i]);
}
static void WINAPI h_PostQuitMessage(int c) { log_stack("PostQuitMessage", _ReturnAddress()); PostQuitMessage(c); }
static void WINAPI h_ExitProcess(UINT c) { log_stack("ExitProcess", _ReturnAddress()); dlog("code %u", c); ExitProcess(c); }
static BOOL WINAPI h_TerminateProcess(HANDLE p, UINT c) { log_stack("TerminateProcess", _ReturnAddress()); dlog("code %u", c); return TerminateProcess(p, c); }
static HANDLE WINAPI h_CreateMutexA(LPSECURITY_ATTRIBUTES a, BOOL o, LPCSTR n) { HANDLE h = CreateMutexA(a, o, n); dlog("CreateMutexA %s -> %p err %u", n ? n : "", h, GetLastError()); SetLastError(h ? GetLastError() : 0); return h; }
static void WINAPI h_OutputDebugStringA(LPCSTR t) { dlog("ODS: %s", t ? t : ""); }
static int WINAPI h_MessageBoxA(HWND w, LPCSTR t, LPCSTR c, UINT f) { dlog("MessageBoxA %s / %s", t, c); (void)w; (void)f; return IDOK; }
#endif /* DSTJP_TEST */

void proxy_init(void);
static void init(void)
{
#ifdef DSTJP_TEST
    char buf[MAX_PATH];
    InitializeCriticalSection(&g_logcs);
    if (GetEnvironmentVariableA("DSTJP_TEST", buf, sizeof buf) && buf[0] >= '1') g_test = buf[0] - '0';
    if (GetEnvironmentVariableA("DSTJP_LOG", buf, sizeof buf)) g_log = fopen(buf, "w");
    dlog("dstjp start test=%d", g_test);
    if (g_test) {
        HMODULE fx = GetModuleHandleA("dsfix.dll");
        o_Direct3DCreate9 = iat_hook("d3d9.dll", "Direct3DCreate9", (void *)h_Direct3DCreate9);
        o_CreateWindowExA = iat_hook("USER32.dll", "CreateWindowExA", (void *)h_CreateWindowExA);
        iat_hook("USER32.dll", "ShowWindow", (void *)h_ShowWindow);
        iat_hook("USER32.dll", "SetWindowLongA", (void *)h_SetWindowLongA);
        iat_hook("USER32.dll", "SetForegroundWindow", (void *)h_True);
        iat_hook("USER32.dll", "SetActiveWindow", (void *)h_NullHwnd);
        iat_hook("USER32.dll", "SetCapture", (void *)h_NullHwnd);
        iat_hook("KERNEL32.dll", "OutputDebugStringA", (void *)h_OutputDebugStringA);
        if (GetEnvironmentVariableA("DSTJP_DIAG", buf, sizeof buf)) {     /* 調べもの用 */
            AddVectoredExceptionHandler(1, veh);
            iat_hook("USER32.dll", "PostQuitMessage", (void *)h_PostQuitMessage);
            iat_hook("USER32.dll", "MessageBoxA", (void *)h_MessageBoxA);
            iat_hook_mod(fx, "KERNEL32.dll", "ExitProcess", (void *)h_ExitProcess);
            iat_hook_mod(fx, "KERNEL32.dll", "TerminateProcess", (void *)h_TerminateProcess);
            iat_hook("KERNEL32.dll", "ExitProcess", (void *)h_ExitProcess);
            iat_hook("KERNEL32.dll", "TerminateProcess", (void *)h_TerminateProcess);
            iat_hook("KERNEL32.dll", "CreateMutexA", (void *)h_CreateMutexA);
        }
        if (fx) {
            iat_hook_mod(fx, "USER32.dll", "SetWindowLongW", (void *)h_SetWindowLongW);
            iat_hook_mod(fx, "USER32.dll", "SetWindowPos", (void *)h_SetWindowPos);
            iat_hook_mod(fx, "KERNEL32.dll", "GetPrivateProfileIntW", (void *)h_GetPrivateProfileIntW);
            iat_hook_mod(fx, "KERNEL32.dll", "WritePrivateProfileStringW", (void *)h_WritePrivateProfileStringW);
            if (0) iat_hook_mod(fx, "SHLWAPI.dll", "PathFileExistsA", (void *)h_PathFileExistsA);
            if (0) iat_hook_mod(fx, "USER32.dll", "EnumDisplaySettingsW", (void *)h_EnumDisplaySettingsW);
        }
        dlog("dsfix %p", fx);
        testio_init();
    }
    if (g_test && GetEnvironmentVariableA("DSTJP_NOJP", buf, sizeof buf)) { dlog("NOJP"); return; }
#endif
    if (!data_init()) { dlog("no data"); return; }
    o_CreateFileA = iat_hook("KERNEL32.dll", "CreateFileA", (void *)h_CreateFileA);
#ifdef DSTJP_TEST
    if (g_test > 1) o_ReadFile = iat_hook("KERNEL32.dll", "ReadFile", (void *)h_ReadFile);
#endif
    wrap_init();
    font_init();
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        g_self = h;
        proxy_init();
        init();
    }
#ifdef DSTJP_TEST
    else if (reason == DLL_PROCESS_DETACH && g_test) {
        uint32_t *sp = (uint32_t *)_AddressOfReturnAddress(), i;
        dlog("process detach (exit), exe/dsfix return addresses on the stack:");
        for (i = 0; i < 4096; i++) {
            HMODULE m = NULL;
            if (sp[i] < 0x10000) continue;
            if ((sp[i] >= 0x401000 && sp[i] < 0x520000)) dlog("   exe %08x", sp[i]);
            else if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)sp[i], &m) && m == GetModuleHandleA("dsfix.dll"))
                dlog("   dsfix +%x", sp[i] - (uint32_t)m);
        }
    }
#endif
    return TRUE;
}
