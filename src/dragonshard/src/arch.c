/* Archive redirection: Archives\Text.H2O and Archives\Fonts.H2O are rebuilt from the user's own files
   (strings replaced / Japanese glyphs added) into %TEMP%\DragonshardJP, and the game opens those instead.

   H2O (version 8) layout, as far as we need it:
     text header ... 0x1A
     u32 ver(8), u32 nent, u32 nfiles, u32, u32 packed total, u32, u32 raw total, u32, u32, u32   (0x28 bytes)
     nent * 0x28-byte entries: [4] raw size, [5] packed size, [6] file offset, [8] crc32 (0 sizes = unused slot)
     two packed blocks (directory names, file names), then the files.
   A file at its offset: u32 packed, u32 raw, u32 crc32, data (PKWARE DCL imploded, or stored when packed == raw). */
#include "dsjp.h"
#include <stdio.h>

static WCHAR g_dir[MAX_PATH];
static WCHAR g_text[MAX_PATH], g_fonts[MAX_PATH];
static int g_text_ok, g_fonts_ok, g_text_tried, g_fonts_tried;
static CRITICAL_SECTION g_cs;
static int g_ready;

static uint32_t crc_table[256];
static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xffffffffu, i;
    if (!crc_table[1]) {
        for (i = 0; i < 256; i++) {
            uint32_t k = i; int j;
            for (j = 0; j < 8; j++) k = k & 1 ? 0xedb88320u ^ (k >> 1) : k >> 1;
            crc_table[i] = k;
        }
    }
    for (i = 0; i < n; i++) c = crc_table[(c ^ p[i]) & 255] ^ (c >> 8);
    return ~c;
}

static uint8_t *read_all(const WCHAR *path, uint32_t *n, FILETIME *ft)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD sz, got;
    uint8_t *b;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL);
    if (ft) GetFileTime(h, NULL, NULL, ft);
    b = (uint8_t *)malloc(sz ? sz : 1);
    if (!b || !ReadFile(h, b, sz, &got, NULL) || got != sz) { CloseHandle(h); free(b); return NULL; }
    CloseHandle(h);
    *n = sz;
    return b;
}

static int write_all(const WCHAR *path, const uint8_t *b, uint32_t n)
{
    WCHAR tmp[MAX_PATH];
    HANDLE h;
    DWORD put;
    swprintf(tmp, MAX_PATH, L"%s.tmp", path);
    h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (!WriteFile(h, b, n, &put, NULL) || put != n) { CloseHandle(h); DeleteFileW(tmp); return 0; }
    CloseHandle(h);
    return MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING);
}

typedef int (*XformFn)(const uint8_t *src, uint32_t n, uint8_t **out, uint32_t *outn);

/* rebuild an archive, passing each file through fn (stored uncompressed in the result) */
static int rebuild(const WCHAR *src, const WCHAR *dst, XformFn fn)
{
    uint32_t n, *ent, nent, i, first = 0xffffffffu, outn, outcap;
    uint8_t *d = read_all(src, &n, NULL), *out = NULL, *p;
    int ok = 0;
    if (!d) { dlog("rebuild: cannot read %ls", src); return 0; }
    p = memchr(d, 0x1a, n < 4096 ? n : 4096);
    if (!p || (uint32_t)(p - d) + 1 + 0x28 > n) goto done;
    p++;
    nent = ((uint32_t *)p)[1];
    ent = (uint32_t *)(p + 0x28);
    if ((uint8_t *)(ent + nent * 10) > d + n) goto done;
    for (i = 0; i < nent; i++)
        if (ent[i * 10 + 4] && ent[i * 10 + 6] < first) first = ent[i * 10 + 6];
    if (first >= n) goto done;
    outcap = n * 2 + (64u << 20);
    out = (uint8_t *)malloc(outcap);
    if (!out) goto done;
    memcpy(out, d, first);
    outn = first;
    {
        uint32_t *oent = (uint32_t *)(out + ((uint8_t *)ent - d)), *ohdr = (uint32_t *)(out + (p - d));
        uint32_t tot = 0;
        /* files in archive order */
        for (;;) {
            uint32_t best = 0xffffffffu, bi = 0, off, packed, raw, crc;
            uint8_t *raw_data, *nd = NULL;
            uint32_t ndn = 0;
            for (i = 0; i < nent; i++) {
                uint32_t o = ent[i * 10 + 6];
                if (ent[i * 10 + 4] && o >= first && o < best && oent[i * 10 + 6] == o && !(oent[i * 10 + 7] & 0x80000000u)) { best = o; bi = i; }
            }
            if (best == 0xffffffffu) break;
            oent[bi * 10 + 7] |= 0x80000000u;    /* mark done (restored below) */
            off = best;
            if (off + 12 > n) goto done;
            packed = *(uint32_t *)(d + off); raw = *(uint32_t *)(d + off + 4); crc = *(uint32_t *)(d + off + 8);
            if (off + 12 + packed > n) goto done;
            raw_data = (uint8_t *)malloc(raw ? raw : 1);
            if (!raw_data) goto done;
            if (packed == raw) memcpy(raw_data, d + off + 12, raw);
            else if (explode(d + off + 12, packed, raw_data, raw) != (int)raw) { dlog("rebuild: explode failed at %x", off); free(raw_data); goto done; }
            if (crc32(raw_data, raw) != crc) dlog("rebuild: crc mismatch at %x", off);
            if (!fn(raw_data, raw, &nd, &ndn) || !nd) { nd = raw_data; ndn = raw; }
            if (outn + 12 + ndn > outcap) {
                uint8_t *o2;
                outcap = outn + 12 + ndn + (64u << 20);
                o2 = (uint8_t *)realloc(out, outcap);
                if (!o2) { if (nd != raw_data) free(nd); free(raw_data); goto done; }
                oent = (uint32_t *)(o2 + ((uint8_t *)oent - out));
                ohdr = (uint32_t *)(o2 + ((uint8_t *)ohdr - out));
                out = o2;
            }
            crc = crc32(nd, ndn);
            ((uint32_t *)(out + outn))[0] = ndn;
            ((uint32_t *)(out + outn))[1] = ndn;
            ((uint32_t *)(out + outn))[2] = crc;
            memcpy(out + outn + 12, nd, ndn);
            oent[bi * 10 + 4] = ndn;
            oent[bi * 10 + 5] = ndn;
            oent[bi * 10 + 6] = outn;
            oent[bi * 10 + 8] = crc;
            outn += 12 + ndn;
            tot += ndn;
            if (nd != raw_data) free(nd);
            free(raw_data);
        }
        for (i = 0; i < nent; i++) oent[i * 10 + 7] &= 0x7fffffffu;
        ohdr[4] = tot;   /* packed total */
        ohdr[6] = tot;   /* raw total */
    }
    ok = write_all(dst, out, outn);
    dlog("rebuild %ls -> %ls: %d (%u bytes)", src, dst, ok, outn);
done:
    free(out);
    free(d);
    return ok;
}

/* the cache is valid while the source archive and this DLL stay the same */
static int cache_valid(const WCHAR *src, const WCHAR *dst, const WCHAR *stamp)
{
    WCHAR want[256], have[256] = L"";
    WIN32_FILE_ATTRIBUTE_DATA a, b;
    FILE *f;
    WCHAR self[MAX_PATH];
    GetModuleFileNameW(g_self, self, MAX_PATH);
    if (!GetFileAttributesExW(src, GetFileExInfoStandard, &a) || !GetFileAttributesExW(self, GetFileExInfoStandard, &b)) return 0;
    swprintf(want, 256, L"%08x%08x %08x %08x%08x %08x", a.ftLastWriteTime.dwHighDateTime, a.ftLastWriteTime.dwLowDateTime, a.nFileSizeLow,
             b.ftLastWriteTime.dwHighDateTime, b.ftLastWriteTime.dwLowDateTime, b.nFileSizeLow);
    f = _wfopen(stamp, L"r, ccs=UTF-8");
    if (f) { fgetws(have, 256, f); fclose(f); }
    if (!wcscmp(want, have) && GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES) return 1;
    return 0;
}
static void cache_stamp(const WCHAR *src, const WCHAR *stamp)
{
    WIN32_FILE_ATTRIBUTE_DATA a, b;
    WCHAR self[MAX_PATH];
    FILE *f;
    GetModuleFileNameW(g_self, self, MAX_PATH);
    if (!GetFileAttributesExW(src, GetFileExInfoStandard, &a) || !GetFileAttributesExW(self, GetFileExInfoStandard, &b)) return;
    f = _wfopen(stamp, L"w, ccs=UTF-8");
    if (!f) return;
    fwprintf(f, L"%08x%08x %08x %08x%08x %08x", a.ftLastWriteTime.dwHighDateTime, a.ftLastWriteTime.dwLowDateTime, a.nFileSizeLow,
             b.ftLastWriteTime.dwHighDateTime, b.ftLastWriteTime.dwLowDateTime, b.nFileSizeLow);
    fclose(f);
}

static int prepare(const WCHAR *src, const WCHAR *dst, const WCHAR *name, XformFn fn)
{
    WCHAR stamp[MAX_PATH];
    swprintf(stamp, MAX_PATH, L"%s\\%s.stamp", g_dir, name);
    if (!g_test || !GetEnvironmentVariableW(L"DSJP_REBUILD", NULL, 0))
        if (cache_valid(src, dst, stamp)) return 1;
    {
        DWORD t0 = GetTickCount();
        if (!rebuild(src, dst, fn)) return 0;
        dlog("built %ls in %u ms", name, GetTickCount() - t0);
    }
    cache_stamp(src, stamp);
    return 1;
}

void arch_init(void)
{
    WCHAR t[MAX_PATH];
    InitializeCriticalSection(&g_cs);
    GetTempPathW(MAX_PATH, t);
    swprintf(g_dir, MAX_PATH, L"%sDragonshardJP", t);
    CreateDirectoryW(g_dir, NULL);
    swprintf(g_text, MAX_PATH, L"%s\\Text.H2O", g_dir);
    swprintf(g_fonts, MAX_PATH, L"%s\\Fonts.H2O", g_dir);
    text_init();
    g_ready = 1;
}

static int ends_with(const WCHAR *s, const WCHAR *tail)
{
    size_t a = wcslen(s), b = wcslen(tail);
    return a >= b && !_wcsicmp(s + a - b, tail) && (a == b || s[a - b - 1] == L'\\' || s[a - b - 1] == L'/');
}

const WCHAR *arch_redirect(const WCHAR *path)
{
    const WCHAR *r = NULL;
    if (!g_ready) return NULL;
    if (ends_with(path, L"Archives\\Text.H2O")) {
        EnterCriticalSection(&g_cs);
        if (!g_text_tried) { g_text_tried = 1; g_text_ok = prepare(path, g_text, L"Text", text_build); }
        LeaveCriticalSection(&g_cs);
        if (g_text_ok) r = g_text;
    } else if (ends_with(path, L"Archives\\Fonts.H2O")) {
        EnterCriticalSection(&g_cs);
        if (!g_fonts_tried) { g_fonts_tried = 1; g_fonts_ok = prepare(path, g_fonts, L"Fonts", font_build); }
        LeaveCriticalSection(&g_cs);
        if (g_fonts_ok) r = g_fonts;
    }
    return r;
}
