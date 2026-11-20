/* Translated text files.
   The game reads its text (.mes "{id}{text}", .dlg "{n}{male}{female}{iq}{test}{answer}{effect}", mes/help.tab with
   tab separated fields) through tio.dll. tio_fopen is hooked: a text file we have translations for is copied once
   per DLL build to %TEMP%\ToEEJP\<build>\<path> with every field whose English text is in the table replaced by its
   Japanese (EUC-JP) version, and the game opens that copy instead.
   Resource TJP/1: u32 count, count * {u32 lo, u32 hi, u32 offset} sorted by FNV-64 of the English (bytes as stored),
   then the EUC-JP strings (NUL terminated). Resource TJP/2: the list of files to translate (one path per line). */
#include "tjp.h"
#include <stdio.h>

typedef void *(__cdecl *FopenFn)(const char *, const char *);
typedef int (__cdecl *FcloseFn)(void *);
typedef unsigned (__cdecl *FreadFn)(void *, unsigned, unsigned, void *);
typedef long (__cdecl *FlenFn)(void *);

static FopenFn o_fopen;
static FcloseFn t_fclose;
static FreadFn t_fread;
static FlenFn t_flen;

static const uint8_t *g_tr;
static uint32_t g_trn;
static const char *g_files;
static DWORD g_filesn;
static char g_cache[MAX_PATH];
static CRITICAL_SECTION g_cs;
static int g_ready;
static char g_build[64];

static void rm_tree(const char *dir)
{
    char pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    _snprintf(pat, MAX_PATH, "%s\\*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.cFileName[0] == '.') continue;
            _snprintf(p, MAX_PATH, "%s\\%s", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) rm_tree(p); else DeleteFileA(p);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}

/* forget the copies made by older builds (not from DllMain) */
static void clean_old(void)
{
    char base[MAX_PATH], pat[MAX_PATH], d[MAX_PATH], *e;
    WIN32_FIND_DATAA fd;
    HANDLE h;
    strcpy(base, g_cache);
    e = strrchr(base, 0x5c);
    if (!e) return;
    *e = 0;
    _snprintf(pat, MAX_PATH, "%s\\*", base);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.' && strcmp(fd.cFileName, g_build)) {
            _snprintf(d, MAX_PATH, "%s\\%s", base, fd.cFileName);
            rm_tree(d);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

#pragma pack(push, 1)
typedef struct { uint32_t lo, hi, off; } TrEnt;
#pragma pack(pop)

static uint64_t fnv64(const uint8_t *s, int n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    int i;
    for (i = 0; i < n; i++) { h ^= s[i]; h *= 0x100000001b3ull; }
    return h;
}

static const char *find(uint64_t h)
{
    const TrEnt *t = (const TrEnt *)(g_tr + 4);
    uint32_t lo = 0, hi = g_trn;
    while (lo < hi) {
        uint32_t m = (lo + hi) / 2;
        uint64_t k = ((uint64_t)t[m].hi << 32) | t[m].lo;
        if (k == h) return (const char *)(g_tr + t[m].off);
        if (k < h) lo = m + 1; else hi = m;
    }
    return NULL;
}

/* per-file entries are keyed on "path|text" (the same English word can need two translations, e.g. Heal) */
static const char *lookup_in(const char *path, const uint8_t *s, int n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    int i;
    while (n > 0 && (s[0] == ' ' || s[0] == '\r' || s[0] == '\n')) { s++; n--; }
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\r' || s[n - 1] == '\n')) n--;
    if (n <= 0) return NULL;
    for (i = 0; path[i]; i++) {
        uint8_t c = (uint8_t)path[i];
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c += 32;
        h ^= c; h *= 0x100000001b3ull;
    }
    h ^= '|'; h *= 0x100000001b3ull;
    for (i = 0; i < n; i++) { h ^= s[i]; h *= 0x100000001b3ull; }
    return find(h);
}

static const char *lookup(const uint8_t *s, int n)
{
    uint64_t h;
    const TrEnt *t = (const TrEnt *)(g_tr + 4);
    uint32_t lo = 0, hi = g_trn;
    /* the table is keyed on the text without surrounding blanks */
    while (n > 0 && (s[0] == ' ' || s[0] == '\r' || s[0] == '\n')) { s++; n--; }
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\r' || s[n - 1] == '\n')) n--;
    if (n <= 0) return NULL;
    h = fnv64(s, n);
    while (lo < hi) {
        uint32_t m = (lo + hi) / 2;
        uint64_t k = ((uint64_t)t[m].hi << 32) | t[m].lo;
        if (k == h) return (const char *)(g_tr + t[m].off);
        if (k < h) lo = m + 1; else hi = m;
    }
    return NULL;
}

/* is this path one of ours?  (lower case, '/' or '\' separated, relative to the data root) */
static int wanted(const char *path)
{
    char norm[MAX_PATH];
    int i, n = 0;
    const char *p;
    if (!g_files) return 0;
    for (i = 0; path[i] && n < MAX_PATH - 1; i++) {
        char c = path[i];
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c += 32;
        norm[n++] = c;
    }
    norm[n] = 0;
    p = g_files;
    while (p < g_files + g_filesn) {
        const char *e = memchr(p, '\n', g_files + g_filesn - p);
        int len = e ? (int)(e - p) : (int)(g_files + g_filesn - p);
        if (len == n && !memcmp(p, norm, n)) return 1;
        p += len + 1;
    }
    return 0;
}

typedef struct { char *p; uint32_t n, cap; } Buf;
static void put(Buf *b, const void *d, uint32_t n)
{
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = (char *)realloc(b->p, b->cap); }
    memcpy(b->p + b->n, d, n);
    b->n += n;
}

/* replaces translated fields: {...} in .mes/.dlg, tab/line separated fields in .tab */
static int transform(const char *path, const uint8_t *s, uint32_t n, Buf *out)
{
    uint32_t i = 0, start;
    /* only the text fields: .mes {id}{text}, .dlg {n}{male}{female}{iq}{test}{answer}{effect}, help.tab columns 5+
       (the others are ids, conditions and script code the game compares) */
    int tab = strstr(path, ".tab") != NULL, dlg = strstr(path, ".dlg") != NULL, hits = 0, field = 0, use;
    while (i < n) {
        if (!tab) {
            if (s[i] == '\n') field = 0;
            if (s[i] != '{') { put(out, s + i, 1); i++; continue; }
            put(out, s + i, 1); i++;
            start = i;
            while (i < n && s[i] != '}') i++;
            use = dlg ? (field == 1 || field == 2) : field >= 1;
        } else {
            start = i;
            while (i < n && s[i] != '\t' && s[i] != '\n' && s[i] != '\r' && s[i] != 0x0b) i++;   /* 0x0b: line feed inside a help text */
            use = field >= 4;
        }
        {
            /* surrounding blanks (help.tab ends texts with vertical tabs = line feeds) stay as they are */
            const char *ja = NULL;
            uint32_t a = start, b = i;
            while (a < b && s[a] <= ' ') a++;
            while (b > a && s[b - 1] <= ' ') b--;
            if (use && b > a) {
                ja = lookup_in(path, s + a, b - a);
                if (!ja) ja = lookup(s + a, b - a);
            }
            if (ja) { put(out, s + start, a - start); put(out, ja, (uint32_t)strlen(ja)); put(out, s + b, i - b); hits++; }
            else put(out, s + start, i - start);
        }
        if (!(tab && i < n && s[i] == 0x0b)) field++;
        if (i < n) {
            if (tab && (s[i] == '\n' || s[i] == '\r')) field = 0;
            put(out, s + i, 1); i++;
        }
    }
    return hits;
}

static void make_dirs(char *p)
{
    char *q;
    for (q = p + 3; *q; q++)
        if (*q == '\\') { *q = 0; CreateDirectoryA(p, NULL); *q = '\\'; }
}

/* builds (once) the translated copy and returns its path, or NULL */
static int cached(const char *path, char *out)
{
    char rel[MAX_PATH];
    int i;
    void *f;
    long len;
    uint8_t *data;
    Buf b = { 0 };
    FILE *fp;
    for (i = 0; path[i] && i < MAX_PATH - 1; i++) rel[i] = path[i] == '/' ? '\\' : path[i];
    rel[i] = 0;
    _snprintf(out, MAX_PATH, "%s\\%s", g_cache, rel);
    if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) return 1;
    f = o_fopen(path, "rb");
    if (!f) return 0;
    len = t_flen(f);
    data = (uint8_t *)malloc(len > 0 ? len : 1);
    if (len > 0) t_fread(data, 1, len, f);
    t_fclose(f);
    i = transform(path, data, len > 0 ? len : 0, &b);
    free(data);
    make_dirs(out);
    fp = fopen(out, "wb");
    if (!fp) { free(b.p); return 0; }
    fwrite(b.p, 1, b.n, fp);
    fclose(fp);
    free(b.p);
    if (g_test) tlog("translated %s (%d fields)", path, i);
    return 1;
}

static int resolve_tio(void)
{
    HMODULE tio;
    if (o_fopen && t_fclose && t_fread && t_flen) return 1;
    tio = GetModuleHandleA("tio.dll");
    if (!tio) return 0;
    t_fclose = (FcloseFn)GetProcAddress(tio, "tio_fclose");
    t_fread = (FreadFn)GetProcAddress(tio, "tio_fread");
    t_flen = (FlenFn)GetProcAddress(tio, "tio_filelength");
    o_fopen = (FopenFn)GetProcAddress(tio, "tio_fopen");
    return o_fopen && t_fclose && t_fread && t_flen;
}

static void *__cdecl h_fopen(const char *path, const char *mode)
{
    char alt[MAX_PATH];
    if (!resolve_tio()) return NULL;
    if (g_ready && path && mode && mode[0] == 'r' && wanted(path)) {
        int ok;
        EnterCriticalSection(&g_cs);
        ok = cached(path, alt);
        LeaveCriticalSection(&g_cs);
        if (ok) {
            void *f = o_fopen(alt, mode);
            if (f) return f;
            tlog("tio_fopen(%s) of the translated copy failed", alt);
        }
    }
    return o_fopen(path, mode);
}

/* TPlite's file system fetches tio functions with GetProcAddress */
FARPROC text_getproc(const char *name, FARPROC real)
{
    if (g_tr && HIWORD(name) && !strcmp(name, "tio_fopen")) { if (!o_fopen) o_fopen = (FopenFn)real; return (FARPROC)h_fopen; }
    return real;
}

/* called right after the game dll (ToEESteam.dll / temple.dll) is loaded, before it reads any file */
void text_on_game_dll(HMODULE g)
{
    HMODULE tio = GetModuleHandleA("tio.dll");
    if (!g_tr || g_ready || !tio || !resolve_tio()) return;
    iat_hook(g, "tio.dll", "tio_fopen", (void *)h_fopen);
    iat_hook(NULL, "tio.dll", "tio_fopen", (void *)h_fopen);   /* TPlite's own file system (delay import) */
    clean_old();
    g_ready = 1;
    tlog("text: tio_fopen hooked (%u translations)", g_trn);
}

void text_init(void)
{
    DWORD n;
    char tmp[MAX_PATH], self[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA a;
    const uint8_t *r = (const uint8_t *)res_get(1, &n);
    InitializeCriticalSection(&g_cs);
    if (r && n >= 4) { g_tr = r; g_trn = *(const uint32_t *)r; }
    g_files = (const char *)res_get(2, &g_filesn);
    if (!g_tr || !g_files) { tlog("text: no translation data"); return; }
    /* one cache folder per DLL build */
    GetModuleFileNameA(g_self, self, MAX_PATH);
    memset(&a, 0, sizeof a);
    GetFileAttributesExA(self, GetFileExInfoStandard, &a);
    GetTempPathA(MAX_PATH, tmp);
    _snprintf(g_cache, MAX_PATH, "%sToEEJP", tmp);
    CreateDirectoryA(g_cache, NULL);
    {
        char cur[64];
        _snprintf(cur, sizeof cur, "%08x%08x", a.ftLastWriteTime.dwLowDateTime, a.nFileSizeLow);
        strcpy(g_build, cur);
        strcat(g_cache, "\\");
        strcat(g_cache, cur);
        CreateDirectoryA(g_cache, NULL);
    }
}
