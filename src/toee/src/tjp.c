/* ToEE Japanese patch: entry point, patch helpers, test-mode window hooks */
#include "tjp.h"
#ifdef TJP_TEST
#include <stdio.h>
#include <stdarg.h>
#endif

void proxy_init(void);
void text_init(void);
void render_init(void);

HMODULE g_self;
#ifdef TJP_TEST
int g_test;
static FILE *g_log;
static CRITICAL_SECTION g_logcs;

void tlog(const char *fmt, ...)
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

static void *set_slot(void **slot, void *repl)
{
    void *old = *slot;
    DWORD prot;
    VirtualProtect(slot, 4, PAGE_READWRITE, &prot);
    *slot = repl;
    VirtualProtect(slot, 4, prot, &prot);
    return old;
}

/* hooks a normal or delay-loaded import of module mod. For a delay import that has not been resolved
   yet the old value is the loader thunk; the real function is then fetched with GetProcAddress. */
void *iat_hook(HMODULE mod, const char *dll, const char *func, void *repl)
{
    uint8_t *base = (uint8_t *)(mod ? mod : GetModuleHandleA(NULL));
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_DATA_DIRECTORY *ddir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
    if (dir->VirtualAddress) {
        IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress);
        for (; imp->Name; imp++) {
            IMAGE_THUNK_DATA *names, *addrs;
            if (_stricmp((char *)(base + imp->Name), dll)) continue;
            names = (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk);
            addrs = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
            for (; names->u1.AddressOfData; names++, addrs++) {
                if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
                if (!strcmp((char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name, func))
                    return set_slot((void **)&addrs->u1.Function, repl);
            }
        }
    }
    if (ddir->VirtualAddress) {
        IMAGE_DELAYLOAD_DESCRIPTOR *d = (IMAGE_DELAYLOAD_DESCRIPTOR *)(base + ddir->VirtualAddress);
        for (; d->DllNameRVA; d++) {
            IMAGE_THUNK_DATA *names, *addrs;
            if (_stricmp((char *)(base + d->DllNameRVA), dll)) continue;
            names = (IMAGE_THUNK_DATA *)(base + d->ImportNameTableRVA);
            addrs = (IMAGE_THUNK_DATA *)(base + d->ImportAddressTableRVA);
            for (; names->u1.AddressOfData; names++, addrs++) {
                if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
                if (!strcmp((char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name, func)) {
                    /* never load a delay DLL early: tio.dll would take temple.dll's address 0x10000000 */
                    HMODULE m = GetModuleHandleA(dll);
                    if (!m && (!_stricmp(dll, "USER32.dll") || !_stricmp(dll, "GDI32.dll"))) m = LoadLibraryA(dll);
                    void *real = m ? (void *)GetProcAddress(m, func) : NULL;
                    set_slot((void **)&addrs->u1.Function, repl);
                    return real;
                }
            }
        }
    }
    tlog("iat_hook: %s!%s not found", dll, func);
    return NULL;
}

const void *res_get(int id, DWORD *size)
{
    HRSRC r = FindResourceA(g_self, MAKEINTRESOURCEA(id), "TJP");
    if (!r) { *size = 0; return NULL; }
    *size = SizeofResource(g_self, r);
    return LockResource(LoadResource(g_self, r));
}

/* ---------- the game dll: hook its imports as soon as it is loaded ---------- */
void text_on_game_dll(HMODULE g);
static int (WINAPI *o_LCMapStringA)(LCID, DWORD, LPCSTR, int, LPSTR, int);
static int (WINAPI *o_LCMapStringW)(LCID, DWORD, LPCWSTR, int, LPWSTR, int);
static int WINAPI h_LCMapStringA(LCID l, DWORD f, LPCSTR s, int n, LPSTR d, int dn);
static int WINAPI h_LCMapStringW(LCID l, DWORD f, LPCWSTR s, int n, LPWSTR d, int dn);
static void check_game_dll(HMODULE m)
{
    char p[MAX_PATH], *b;
    if (!m || !GetModuleFileNameA(m, p, MAX_PATH)) return;
    b = strrchr(p, 0x5c); b = b ? b + 1 : p;
    if (!_stricmp(b, "ToEESteam.dll") || !_stricmp(b, "temple.dll") || !_stricmp(b, "ToEE.dll")) {
        tlog("game dll %s at %p", b, m);
        text_on_game_dll(m);
#ifdef TJP_TEST
        if (g_test) testio_game_dll(m);
#endif
        if (!o_LCMapStringA) o_LCMapStringA = iat_hook(m, "KERNEL32.dll", "LCMapStringA", (void *)h_LCMapStringA);
        { void *o = iat_hook(m, "KERNEL32.dll", "LCMapStringW", (void *)h_LCMapStringW); if (!o_LCMapStringW) o_LCMapStringW = o; }
    }
}
/* TPlite converts game strings between the ANSI code page and UTF-16. Game text is cp1252 (English) or
   EUC-JP (our translations): never use the system code page (Shift-JIS on Japanese Windows breaks EUC-JP). */
static int is_ansi(UINT cp) { return cp == CP_ACP || cp == CP_THREAD_ACP || cp == CP_OEMCP; }
static int has_euc(const char *s, int n)
{
    int i;
    if (n < 0) n = (int)strlen(s);
    for (i = 0; i + 1 < n; i++) if ((uint8_t)s[i] >= 0xa1 && (uint8_t)s[i + 1] >= 0xa1) return 1;
    return 0;
}
static int (WINAPI *o_MultiByteToWideChar)(UINT, DWORD, LPCCH, int, LPWSTR, int);
static int WINAPI h_MultiByteToWideChar(UINT cp, DWORD fl, LPCCH s, int n, LPWSTR w, int wn)
{
    if (is_ansi(cp) && s) { cp = has_euc(s, n) ? 20932 : 1252; fl = 0; }
    return o_MultiByteToWideChar(cp, fl, s, n, w, wn);
}
static int (WINAPI *o_WideCharToMultiByte)(UINT, DWORD, LPCWCH, int, LPSTR, int, LPCCH, LPBOOL);
static int WINAPI h_WideCharToMultiByte(UINT cp, DWORD fl, LPCWCH w, int n, LPSTR s, int sn, LPCCH def, LPBOOL used)
{
    if (is_ansi(cp) && w) {
        int i, wide = 0, len = n < 0 ? (int)wcslen(w) : n;
        for (i = 0; i < len; i++) if (w[i] >= 0x100) { wide = 1; break; }
        cp = wide ? 20932 : 1252;
        if (cp == 20932) { def = NULL; used = NULL; fl = 0; }
    }
    return o_WideCharToMultiByte(cp, fl, w, n, s, sn, def, used);
}

/* The C runtimes upper/lower-case strings (button labels, race names) through LCMapString when a locale is set;
   that turns the second byte of an EUC-JP character into a different one. Leave bytes >= 0x80 alone. */
static int WINAPI h_LCMapStringA(LCID l, DWORD f, LPCSTR s, int n, LPSTR d, int dn)
{
    int r = o_LCMapStringA(l, f, s, n, d, dn);
    if (r > 0 && d && dn > 0 && (f & (LCMAP_UPPERCASE | LCMAP_LOWERCASE)) && !(f & (LCMAP_SORTKEY | LCMAP_BYTEREV))) {
        int i, len = n < 0 ? (int)strlen(s) + 1 : n;
        if (r == len) for (i = 0; i < len; i++) if ((uint8_t)s[i] >= 0x80) d[i] = s[i];
    }
    return r;
}
static int WINAPI h_LCMapStringW(LCID l, DWORD f, LPCWSTR s, int n, LPWSTR d, int dn)
{
    int r = o_LCMapStringW(l, f, s, n, d, dn);
    if (r > 0 && d && dn > 0 && (f & (LCMAP_UPPERCASE | LCMAP_LOWERCASE)) && !(f & (LCMAP_SORTKEY | LCMAP_BYTEREV))) {
        int i, len = n < 0 ? (int)wcslen(s) + 1 : n;
        if (r == len) for (i = 0; i < len; i++) if (s[i] >= 0x80) d[i] = s[i];
    }
    return r;
}
static int (WINAPI *o_LCMapStringEx)(LPCWSTR, DWORD, LPCWSTR, int, LPWSTR, int, LPNLSVERSIONINFO, LPVOID, LPARAM);
static int WINAPI h_LCMapStringEx(LPCWSTR l, DWORD f, LPCWSTR s, int n, LPWSTR d, int dn, LPNLSVERSIONINFO v, LPVOID r1, LPARAM r2)
{
    int r = o_LCMapStringEx(l, f, s, n, d, dn, v, r1, r2);
    if (r > 0 && d && dn > 0 && (f & (LCMAP_UPPERCASE | LCMAP_LOWERCASE)) && !(f & (LCMAP_SORTKEY | LCMAP_BYTEREV))) {
        int i, len = n < 0 ? (int)wcslen(s) + 1 : n;
        if (r == len) for (i = 0; i < len; i++) if (s[i] >= 0x80) d[i] = s[i];
    }
    return r;
}

FARPROC text_getproc(const char *name, FARPROC real);
static FARPROC (WINAPI *o_GetProcAddress)(HMODULE, LPCSTR);
static FARPROC WINAPI h_GetProcAddress(HMODULE m, LPCSTR n) { return text_getproc(n, o_GetProcAddress(m, n)); }
static HMODULE (WINAPI *o_LoadLibraryW)(LPCWSTR);
static HMODULE WINAPI h_LoadLibraryW(LPCWSTR n) { HMODULE m = o_LoadLibraryW(n); check_game_dll(m); return m; }
static HMODULE (WINAPI *o_LoadLibraryExW)(LPCWSTR, HANDLE, DWORD);
static HMODULE WINAPI h_LoadLibraryExW(LPCWSTR n, HANDLE h, DWORD f) { HMODULE m = o_LoadLibraryExW(n, h, f); if (!(f & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE | 0x40))) check_game_dll(m); return m; }

#ifdef TJP_TEST
/* ---------- test mode: logging of file access, own window title, never activate / never minimised ---------- */
static HANDLE (WINAPI *o_CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static HANDLE WINAPI h_CreateFileW(LPCWSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t)
{
    HANDLE h = o_CreateFileW(n, a, s, sa, d, f, t);
    tlog("CreateFileW %ls (%x,%u) -> %p", n ? n : L"", a, d, h);
    return h;
}
static HANDLE (WINAPI *o_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static HANDLE WINAPI h_CreateFileA(LPCSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t)
{
    HANDLE h = o_CreateFileA(n, a, s, sa, d, f, t);
    tlog("CreateFileA %s (%x,%u) -> %p", n ? n : "", a, d, h);
    return h;
}

HWND g_hwnd;
static WNDPROC g_wndproc;
static LRESULT CALLBACK test_wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ACTIVATEAPP: wp = TRUE; break;
    case WM_ACTIVATE: wp = WA_ACTIVE; break;
    case WM_NCACTIVATE: wp = TRUE; break;
    case WM_KILLFOCUS: return 0;
    case WM_SIZE: if (wp == SIZE_MINIMIZED) return 0; break;
    case WM_SYSCOMMAND: if ((wp & 0xfff0) == SC_MINIMIZE) return 0; break;
    }
    return CallWindowProcW(g_wndproc, w, m, wp, lp);
}
static ATOM (WINAPI *o_RegisterClassExW)(const WNDCLASSEXW *);
static ATOM WINAPI h_RegisterClassExW(const WNDCLASSEXW *wc)
{
    WNDCLASSEXW c = *wc;
    if (!g_wndproc) { g_wndproc = c.lpfnWndProc; c.lpfnWndProc = test_wndproc; tlog("RegisterClassExW %ls", c.lpszClassName); }
    return o_RegisterClassExW(&c);
}
static HWND (WINAPI *o_CreateWindowExW)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
static HWND WINAPI h_CreateWindowExW(DWORD ex, LPCWSTR cls, LPCWSTR title, DWORD st, int x, int y, int w, int h, HWND par, HMENU m, HINSTANCE in, LPVOID p)
{
    HWND r;
    if (!par) { ex &= ~WS_EX_TOPMOST; ex |= WS_EX_NOACTIVATE; title = L"TJP TEST"; if (x != CW_USEDEFAULT) { x = 40; y = 40; } }
    r = o_CreateWindowExW(ex, cls, title, st, x, y, w, h, par, m, in, p);
    if (!par && !g_hwnd) g_hwnd = r;
    tlog("CreateWindowExW %08x %08x %d,%d %dx%d -> %p", ex, st, x, y, w, h, r);
    return r;
}
static BOOL WINAPI h_SetWindowTextW(HWND w, LPCWSTR t) { (void)t; return SetWindowTextW(w, L"TJP TEST"); }
static BOOL WINAPI h_SetForegroundWindow(HWND w) { (void)w; return TRUE; }
static BOOL WINAPI h_ClipCursor(const RECT *r) { (void)r; return TRUE; }
static BOOL WINAPI h_ShowWindow(HWND w, int cmd)
{
    if (w == g_hwnd && (cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED || cmd == SW_SHOWMINNOACTIVE || cmd == SW_FORCEMINIMIZE)) return TRUE;
    if (cmd == SW_SHOW || cmd == SW_SHOWNORMAL || cmd == SW_RESTORE || cmd == SW_SHOWDEFAULT) cmd = SW_SHOWNOACTIVATE;
    return ShowWindow(w, cmd);
}
#endif /* TJP_TEST */

static void init(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
#ifdef TJP_TEST
    char buf[MAX_PATH];
    InitializeCriticalSection(&g_logcs);
    if (GetEnvironmentVariableA("TJP_TEST", buf, sizeof buf) && buf[0] >= '1') g_test = buf[0] - '0';
    if (GetEnvironmentVariableA("TJP_LOG", buf, sizeof buf)) g_log = fopen(buf, "w");
    tlog("tjp start test=%d", g_test);
    if (g_test) {
        o_CreateFileW = iat_hook(exe, "KERNEL32.dll", "CreateFileW", (void *)h_CreateFileW);
        o_CreateFileA = iat_hook(exe, "KERNEL32.dll", "CreateFileA", (void *)h_CreateFileA);
        o_RegisterClassExW = iat_hook(exe, "USER32.dll", "RegisterClassExW", (void *)h_RegisterClassExW);
        o_CreateWindowExW = iat_hook(exe, "USER32.dll", "CreateWindowExW", (void *)h_CreateWindowExW);
        iat_hook(exe, "USER32.dll", "SetWindowTextW", (void *)h_SetWindowTextW);
        iat_hook(exe, "USER32.dll", "SetForegroundWindow", (void *)h_SetForegroundWindow);
        iat_hook(exe, "USER32.dll", "ClipCursor", (void *)h_ClipCursor);
        iat_hook(exe, "USER32.dll", "ShowWindow", (void *)h_ShowWindow);
        testio_init();
    }
    /* test: TJP_OFF=1 keeps only the test hooks (game as without the patch) */
    if (g_test && GetEnvironmentVariableA("TJP_OFF", buf, sizeof buf)) { tlog("TJP_OFF"); return; }
#endif
    text_init();
    o_LoadLibraryW = iat_hook(exe, "KERNEL32.dll", "LoadLibraryW", (void *)h_LoadLibraryW);
    o_GetProcAddress = iat_hook(exe, "KERNEL32.dll", "GetProcAddress", (void *)h_GetProcAddress);
    { void *o = iat_hook(exe, "KERNEL32.dll", "LCMapStringW", (void *)h_LCMapStringW); if (o) o_LCMapStringW = o; }
    o_LCMapStringEx = iat_hook(exe, "KERNEL32.dll", "LCMapStringEx", (void *)h_LCMapStringEx);
    o_MultiByteToWideChar = iat_hook(exe, "KERNEL32.dll", "MultiByteToWideChar", (void *)h_MultiByteToWideChar);
    o_WideCharToMultiByte = iat_hook(exe, "KERNEL32.dll", "WideCharToMultiByte", (void *)h_WideCharToMultiByte);
    o_LoadLibraryExW = iat_hook(exe, "KERNEL32.dll", "LoadLibraryExW", (void *)h_LoadLibraryExW);
    render_init();
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
    return TRUE;
}
