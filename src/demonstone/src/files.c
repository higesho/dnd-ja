/* ゲームが開くファイルのうち、日本語にするものは %TEMP%\DemonStoneJP に作ったコピーを渡す
   - UI\*.OBX（英語の画面の文字、OBSS）と subtitle\*.OBX（英語の字幕、OBST）: 訳のある文字列を差し替えて作り直す
   （フォントはファイルでなく、ゲームが組み立てるときに差し替える: font.c）
   言語ごとのフォルダ（UI\DE\ など）のファイルはそのまま */
#include "dst.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct { uint8_t *p; uint32_t n, cap; } Buf;
static void put(Buf *b, const void *d, uint32_t n)
{
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 4096;
        b->p = (uint8_t *)realloc(b->p, b->cap);
    }
    if (d) memcpy(b->p + b->n, d, n); else memset(b->p + b->n, 0, n);
    b->n += n;
}

uint8_t *files_read(const char *path, uint32_t *n)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD sz, got = 0;
    uint8_t *b;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(f, NULL);
    b = (uint8_t *)malloc(sz ? sz : 1);
    if (!ReadFile(f, b, sz, &got, NULL) || got != sz) { CloseHandle(f); free(b); return NULL; }
    CloseHandle(f);
    *n = sz;
    return b;
}

static int u16len(const uint8_t *d, uint32_t n, uint32_t o)
{
    uint32_t e = o;
    while (e + 2 <= n && (d[e] | d[e + 1])) e += 2;
    return e + 2 <= n ? (int)((e - o) / 2) : -1;
}

/* OBSS: 'OBSS', u32 1, u32 1, u32 n, n x {u32 id, u32 offset}
   OBST: 'OBST', u32 2, u32 n, u32 0, n x {u32 hash, u32 offset, f32 start, f32 end}
   文字列（UTF-16, 0 終わり）は表の後ろ。訳のあるものを差し替え、表の位置を書き直す */
static int obx_build(const uint8_t *d, uint32_t n, uint8_t **out, uint32_t *outn)
{
    uint32_t cnt, rec, ofs, hdr, i, done = 0;
    Buf b = { 0 };
    if (n < 16) return 0;
    if (!memcmp(d, "OBSS", 4)) { cnt = *(const uint32_t *)(d + 12); rec = 8; ofs = 4; }
    else if (!memcmp(d, "OBST", 4)) { cnt = *(const uint32_t *)(d + 8); rec = 16; ofs = 4; }
    else return 0;
    hdr = 16 + cnt * rec;
    if (hdr > n) return 0;
    put(&b, d, hdr);
    for (i = 0; i < cnt; i++) {
        uint32_t o = *(const uint32_t *)(d + 16 + i * rec + ofs);
        int len = u16len(d, n, o), jn = 0;
        const uint16_t *ja = len > 0 ? data_lookup((const uint16_t *)(d + o), len, &jn) : NULL;
        *(uint32_t *)(b.p + 16 + i * rec + ofs) = b.n;
        if (ja) { put(&b, ja, jn * 2); done++; }
        else if (len >= 0) put(&b, d + o, len * 2);
        put(&b, NULL, 2);
    }
    dlog("obx: %u/%u strings translated", done, cnt);
    *out = b.p; *outn = b.n;
    return 1;
}

int files_write(const WCHAR *path, const uint8_t *b, uint32_t n)
{
    WCHAR tmp[MAX_PATH];
    HANDLE f;
    DWORD w = 0;
    swprintf(tmp, MAX_PATH, L"%s.tmp", path);
    f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    WriteFile(f, b, n, &w, NULL);
    CloseHandle(f);
    if (w != n) { DeleteFileW(tmp); return 0; }
    DeleteFileW(path);
    return MoveFileW(tmp, path);
}

static uint32_t g_stamp;   /* 訳の表とこの DLL が変わったら作り直す */
uint32_t files_stamp(void)
{
    if (!g_stamp) {
        DWORD n;
        const uint8_t *p = (const uint8_t *)res_get(1, &n);
        uint32_t h = 2166136261u, i;
        for (i = 0; p && i < n; i++) h = (h ^ p[i]) * 16777619u;
        h ^= 0x0103;      /* 作り方を変えたら上げる */
        g_stamp = h ? h : 1;
    }
    return g_stamp;
}

static CRITICAL_SECTION g_cs;
static int g_csinit;
static char g_out[64][MAX_PATH];
static int g_nout;

const char *files_redirect(const char *path)
{
    const char *bn = strrchr(path, '\\'), *b2 = strrchr(path, '/'), *dir;
    char up[MAX_PATH], name[MAX_PATH], tag[16];
    int kind = 0, i;
    size_t L;
    WCHAR wout[MAX_PATH];
    uint8_t *src, *out = NULL;
    uint32_t n, outn = 0;
    if (b2 > bn) bn = b2;
    bn = bn ? bn + 1 : path;
    L = strlen(path);
    if (L >= MAX_PATH - 1) return NULL;
    for (i = 0; path[i]; i++) up[i] = (path[i] >= 'a' && path[i] <= 'z') ? path[i] - 32 : (path[i] == '/' ? '\\' : path[i]);
    up[i] = 0;
    /* どのフォルダのファイルか（直前のフォルダ名） */
    dir = up + (bn - path) - 1;
    {
        const char *s = dir;
        while (s > up && s[-1] != '\\') s--;
        memcpy(tag, s, (dir - s) < 15 ? (dir - s) : 15);
        tag[(dir - s) < 15 ? (dir - s) : 15] = 0;
    }
    if (L > 4 && !strcmp(up + L - 4, ".OBX") && (!strcmp(tag, "UI") || !strcmp(tag, "SUBTITLE"))) kind = 1;
    if (!kind) return NULL;
    if (!g_csinit) { InitializeCriticalSection(&g_cs); g_csinit = 1; }
    EnterCriticalSection(&g_cs);
    wsprintfA(name, "%08X_%s_%s", files_stamp(), tag, bn);
    for (i = 0; i < g_nout; i++) if (!_stricmp(strrchr(g_out[i], '\\') + 1, name)) { LeaveCriticalSection(&g_cs); return g_out[i]; }
    swprintf(wout, MAX_PATH, L"%s%hs", work_dir(), name);
    if (GetFileAttributesW(wout) == INVALID_FILE_ATTRIBUTES) {
        src = files_read(path, &n);
        if (src) {
            int ok = obx_build(src, n, &out, &outn);
            if (ok) files_write(wout, out, outn);
            dlog("built %s (%d, %u bytes)", name, ok, outn);
            free(out);
            free(src);
        }
    }
    if (GetFileAttributesW(wout) == INVALID_FILE_ATTRIBUTES || g_nout >= 64) { LeaveCriticalSection(&g_cs); return NULL; }
    WideCharToMultiByte(CP_ACP, 0, wout, -1, g_out[g_nout], MAX_PATH, NULL, NULL);
    LeaveCriticalSection(&g_cs);
    return g_out[g_nout++];
}
