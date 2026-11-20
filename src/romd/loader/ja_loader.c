/*
 * Pool of Radiance: Ruins of Myth Drannor 日本語化ローダー（wsock32.dll）
 *
 * ゲームのフォルダに置くと por2main.exe が最初のほうで読み込む（ddraw.dll より先に初期化される）。
 *  - wsock32 の関数は Windows の本物の wsock32.dll にそのまま中継する。
 *  - ddraw.dll と sneg.dll のメモリ上の数か所だけを日本語表示用に書き換える（ファイルは書き換えない）。
 *    sneg.dll は初期化の時点で言語の判定を済ませてしまうため、DLL 読み込み通知を使い、
 *    メモリに置かれた直後・初期化の前に書き換える。元のバイト列が一致しない版では何もしない。
 *  - 会話文の枠で、事前の改行と描画時の折り返しの文字サイズが食い違う問題を直す（make_style_hook）。
 *  - 言語がまだ選ばれていなければ、ゲームの設定を日本語にする。
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "patches.h"
#include "wsock32_fwd.h"

static volatile LONG g_sneg_done, g_ddraw_done;
static PVOID g_cookie;

/* ---- パッチ適用（すべて照合してから書き換える） ---- */
static int apply(HMODULE mod, unsigned preferred, const JaPatch *p, int n)
{
    unsigned char *base = (unsigned char *)mod;
    unsigned delta = (unsigned)(ULONG_PTR)base - preferred;
    int i;
    for (i = 0; i < n; i++) {
        unsigned char *at = base + p[i].rva;
        if (p[i].is_va) {
            unsigned vo, vn;
            memcpy(&vo, p[i].old_, 4); vo += delta;
            memcpy(&vn, p[i].new_, 4); vn += delta;
            if (memcmp(at, &vo, 4) != 0 && memcmp(at, &vn, 4) != 0) return 0;
        } else if (memcmp(at, p[i].old_, p[i].len) != 0 && memcmp(at, p[i].new_, p[i].len) != 0) {
            return 0;
        }
    }
    for (i = 0; i < n; i++) {
        unsigned char *at = base + p[i].rva;
        unsigned char buf[4];
        const unsigned char *src = p[i].new_;
        DWORD prot;
        if (p[i].is_va) {
            unsigned v;
            memcpy(&v, p[i].new_, 4); v += delta;
            memcpy(buf, &v, 4);
            src = buf;
        }
        if (VirtualProtect(at, p[i].len, PAGE_EXECUTE_READWRITE, &prot)) {
            memcpy(at, src, p[i].len);
            VirtualProtect(at, p[i].len, prot, &prot);
        }
    }
    FlushInstructionCache(GetCurrentProcess(), base, 0);
    return 1;
}

/* ---- 呼び出し先の差し替え（call rel32 の飛び先が想定どおりのときだけ） ---- */
static void *redirect_call(unsigned char *base, unsigned site_rva, unsigned target_rva, void *fn)
{
    unsigned char *site = base + site_rva;
    DWORD prot;
    int rel;
    if (site[0] != 0xE8) return NULL;
    memcpy(&rel, site + 1, 4);
    if (site + 5 + rel != base + target_rva) return NULL;
    rel = (int)((unsigned char *)fn - (site + 5));
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &prot)) return NULL;
    memcpy(site + 1, &rel, 4);
    VirtualProtect(site, 5, prot, &prot);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return base + target_rva;
}

/*
 * DM などの文章枠（ddraw.dll 0x100b34a0）の改行幅の食い違いを直す。
 * CJK の文章は、先に既定の文字サイズ（例 23.4）で改行位置を決めて改行を入れておき、
 * 描画時には CJK 用に計算し直した文字サイズ（例 25.7）でもう一度折り返している。
 * そのため描画時に行があふれ、「…仕／組みを説／明している…」のような細切れの行ができる。
 * 文字設定（フォント・サイズ）の構造体を作った直後に、描画時と同じ関数でフォントとサイズを
 * 求めて上書きし、事前の改行も描画と同じ大きさで行うようにする。
 */
typedef void *(__cdecl *STYLE_FN)(void *, int, int, float, float);
typedef void *(__cdecl *FONTSIZE_FN)(void *, float *);
static STYLE_FN g_make_style;
static FONTSIZE_FN g_style_font;
static void *__cdecl make_style_hook(void *out, int a, int b, float c, float d)
{
    void *r = g_make_style(out, a, b, c, d);
    if (r && g_style_font) {
        float size = 0;
        void *font = g_style_font(r, &size);
        if (font && size > 0) {
            ((void **)r)[3] = font;   /* +0x0c フォント */
            ((float *)r)[4] = size;   /* +0x10 文字サイズ */
        }
    }
    return r;
}
static const unsigned char STYLE_FONT_HEAD[] = { 0x55, 0x8b, 0xec, 0x56, 0x8b, 0x75, 0x08, 0x57, 0x8b, 0x7d, 0x0c, 0x8b, 0x46, 0x10, 0x89, 0x07 };


static void patch_sneg(HMODULE m)
{
    if (m && InterlockedExchange(&g_sneg_done, 1) == 0)
        apply(m, SNEG_PREFERRED_BASE, sneg_patches, sizeof(sneg_patches) / sizeof(sneg_patches[0]));
}

static void patch_ddraw(HMODULE m)
{
    unsigned char *base = (unsigned char *)m;
    if (!m || InterlockedExchange(&g_ddraw_done, 1) != 0) return;
    if (!apply(m, DDRAW_PREFERRED_BASE, ddraw_patches, sizeof(ddraw_patches) / sizeof(ddraw_patches[0])))
        return;
    if (memcmp(base + 0xaa750, STYLE_FONT_HEAD, sizeof(STYLE_FONT_HEAD)) == 0) {
        g_style_font = (FONTSIZE_FN)(base + 0xaa750);
        g_make_style = (STYLE_FN)redirect_call(base, 0xb373d, 0xaa0c0, (void *)make_style_hook);
    }
}

/* ---- DLL 読み込み通知（DLL の初期化より前に呼ばれる） ---- */
typedef struct { USHORT Length, MaximumLength; PWSTR Buffer; } JA_USTR;
typedef struct { ULONG Flags; const JA_USTR *FullDllName; const JA_USTR *BaseDllName; PVOID DllBase; ULONG SizeOfImage; } JA_DLL_NOTIFY;
typedef VOID(CALLBACK *JA_NOTIFY_FN)(ULONG, const JA_DLL_NOTIFY *, PVOID);
typedef LONG(NTAPI *JA_LDR_REG)(ULONG, JA_NOTIFY_FN, PVOID, PVOID *);

static int name_is(const JA_USTR *s, const wchar_t *name)
{
    int n = lstrlenW(name);
    return s && s->Buffer && s->Length == n * sizeof(WCHAR) &&
           CompareStringOrdinal(s->Buffer, n, name, n, TRUE) == CSTR_EQUAL;
}

static VOID CALLBACK on_dll(ULONG reason, const JA_DLL_NOTIFY *d, PVOID ctx)
{
    (void)ctx;
    if (reason != 1 || !d) return;  /* 1 = 読み込み */
    if (name_is(d->BaseDllName, L"sneg.dll")) patch_sneg((HMODULE)d->DllBase);
    else if (name_is(d->BaseDllName, L"ddraw.dll")) patch_ddraw((HMODULE)d->DllBase);
}

/* ---- 言語がまだ選ばれていなければ、ゲームの設定を日本語にする ---- */
static void default_language(void)
{
    char path[MAX_PATH], val[64];
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", path, MAX_PATH - 40);
    if (n == 0 || n >= MAX_PATH - 40) return;
    lstrcatA(path, "\\SNEG");
    CreateDirectoryA(path, NULL);
    lstrcatA(path, "\\POR2");
    CreateDirectoryA(path, NULL);
    lstrcatA(path, "\\game_settings.ini");
    GetPrivateProfileStringA("Text", "Language", "", val, sizeof(val), path);
    if (val[0] == '\0')
        WritePrivateProfileStringA("Text", "Language", "ja", path);
}

/* ---- 本物の wsock32.dll を読み込み、中継先を決める ---- */
static void load_real_wsock32(void)
{
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH - 16);
    HMODULE real;
    int i;
    if (n == 0 || n >= MAX_PATH - 16) return;
    lstrcpyA(path + n, "\\wsock32.dll");
    real = LoadLibraryA(path);
    if (!real) return;
    for (i = 0; i < JA_FWD_COUNT; i++)
        ja_fwd_ptr[i] = GetProcAddress(real, ja_fwd_name[i]);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        JA_LDR_REG reg;
        DisableThreadLibraryCalls(inst);
        default_language();
        reg = (JA_LDR_REG)GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrRegisterDllNotification");
        if (reg) reg(0, on_dll, NULL, &g_cookie);
        /* すでにメモリにあるもの（ddraw.dll は通常ここで見つかる。まだ初期化前） */
        patch_ddraw(GetModuleHandleA("ddraw.dll"));
        patch_sneg(GetModuleHandleA("sneg.dll"));
        load_real_wsock32();
    }
    return TRUE;
}
