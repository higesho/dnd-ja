/* DWrite.dll proxy: temple.exe imports DWriteCreateFactory from DWrite.dll; forward it to the system one. */
#include <windows.h>

typedef HRESULT (WINAPI *DWriteCreateFactoryFn)(int, REFIID, IUnknown **);
static DWriteCreateFactoryFn g_real;

void proxy_init(void)
{
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH);
    HMODULE m;
    lstrcpyA(path + n, "\\DWrite.dll");
    m = LoadLibraryA(path);
    if (m) g_real = (DWriteCreateFactoryFn)GetProcAddress(m, "DWriteCreateFactory");
}

HRESULT WINAPI p_DWriteCreateFactory(int type, REFIID iid, IUnknown **out)
{
    if (!g_real) return E_FAIL;
    return g_real(type, iid, out);
}
