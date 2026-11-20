/*
 * Analysis-only winmm.dll for DOSBox 0.74: finds strings in guest memory and
 * logs the guest call chain (BP frames) whenever the game reads them.
 * Config: <gamedir>\_jp\trace\needles.txt (one needle per line). Log: trace.log
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

void proxy_init(void);

#define A_REGS          0x74d1e0u
#define A_SEGVAL        0x74d100u
#define A_SEGPHYS       0x74d110u
#define A_TLB_READ      0x74d6d0u
#define A_TLB_RH        0xf4d6d0u

static char g_dir[MAX_PATH];
static FILE *g_log;
static CRITICAL_SECTION g_cs;

static void logf(const char *fmt, ...)
{
    va_list ap;
    if (!g_log) return;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);
}

#define REG32(i) (((uint32_t*)A_REGS)[i])
#define SEGV(i) (((uint16_t*)A_SEGVAL)[i])
#define SEGP(i) (((uint32_t*)A_SEGPHYS)[i])

static uint8_t **TLBR = (uint8_t**)A_TLB_READ;
static void **TLBRH = (void**)A_TLB_RH;

/* direct host pointer of guest linear address, or NULL */
static uint8_t *hp(uint32_t a)
{
    uint8_t *p = TLBR[a >> 12];
    return p ? p + a : NULL;
}

/* ---- traps ---- */
typedef struct { uint32_t start, end; char name[48]; } region;
static region g_reg[64];
static int g_nreg;

typedef struct { uint8_t *orig; void *orig_h; int on; } trappage;
static trappage g_pg[256]; /* pages 0..0x9f (conventional mem) + some */

static uint32_t g_seen[4096];
static int g_nseen;

static uint8_t safe_rb(uint32_t a)
{
    uint32_t pg = a >> 12;
    if (pg < 256 && g_pg[pg].on) return g_pg[pg].orig[a];
    { uint8_t *p = hp(a); return p ? *p : 0; }
}
static uint16_t safe_rw(uint32_t a) { return (uint16_t)(safe_rb(a) | (safe_rb(a + 1) << 8)); }

static void on_read(uint32_t a, int size)
{
    int i, k;
    uint32_t chain[12], h = 2166136261u;
    int n = 0;
    uint16_t bp, ss = (uint16_t)SEGV(2);
    for (i = 0; i < g_nreg; i++) if (a >= g_reg[i].start && a < g_reg[i].end) break;
    if (i == g_nreg) return;
    bp = (uint16_t)REG32(5);
    chain[n++] = (SEGV(1) << 16) | (uint16_t)REG32(8);
    for (k = 0; k < 10 && bp; k++) {
        uint32_t f = ((uint32_t)ss << 4) + bp;
        uint16_t nbp = safe_rw(f), ip = safe_rw(f + 2), cs = safe_rw(f + 4);
        chain[n++] = ((uint32_t)cs << 16) | ip;
        if (nbp <= bp) break;
        bp = nbp;
    }
    for (k = 0; k < n; k++) h = (h ^ chain[k]) * 16777619u;
    h ^= (uint32_t)i * 7919u;
    EnterCriticalSection(&g_cs);
    for (k = 0; k < g_nseen; k++) if (g_seen[k] == h) { LeaveCriticalSection(&g_cs); return; }
    if (g_nseen < 4096) g_seen[g_nseen++] = h;
    logf("READ%d %s+%x  DS=%04x ES=%04x SS=%04x SP=%04x BP=%04x AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x DI=%04x\n",
         size, g_reg[i].name, a - g_reg[i].start, SEGV(3), SEGV(0), ss, (uint16_t)REG32(4), (uint16_t)REG32(5),
         (uint16_t)REG32(0), (uint16_t)REG32(3), (uint16_t)REG32(1), (uint16_t)REG32(2), (uint16_t)REG32(6), (uint16_t)REG32(7));
    for (k = 0; k < n; k++) {
        uint32_t cs = chain[k] >> 16, ip = chain[k] & 0xffff, lin = (cs << 4) + ip, j;
        logf("   %04x:%04x  ", cs, ip);
        for (j = 0; j < 24; j++) logf("%02x", safe_rb(lin - 16 + j));
        logf("\n");
    }
    LeaveCriticalSection(&g_cs);
}

/* fake PageHandler (MinGW GCC 3.x ABI: this as first stack arg, 2 dtor slots) */
static uint32_t __cdecl h_readb(void *s, uint32_t a) { on_read(a, 1); return g_pg[a >> 12].orig[a]; }
static uint32_t __cdecl h_readw(void *s, uint32_t a) { on_read(a, 2); return *(uint16_t*)(g_pg[a >> 12].orig + a); }
static uint32_t __cdecl h_readd(void *s, uint32_t a) { on_read(a, 4); return *(uint32_t*)(g_pg[a >> 12].orig + a); }
static void __cdecl h_writeb(void *s, uint32_t a, uint32_t v) { g_pg[a >> 12].orig[a] = (uint8_t)v; }
static void __cdecl h_writew(void *s, uint32_t a, uint32_t v) { *(uint16_t*)(g_pg[a >> 12].orig + a) = (uint16_t)v; }
static void __cdecl h_writed(void *s, uint32_t a, uint32_t v) { *(uint32_t*)(g_pg[a >> 12].orig + a) = v; }
static uint8_t *__cdecl h_hostr(void *s, uint32_t pg) { return g_pg[pg].orig + pg * 4096; }
static int __cdecl h_rbc(void *s, uint32_t a, uint8_t *v) { *v = (uint8_t)h_readb(s, a); return 0; }
static int __cdecl h_rwc(void *s, uint32_t a, uint16_t *v) { *v = (uint16_t)h_readw(s, a); return 0; }
static int __cdecl h_rdc(void *s, uint32_t a, uint32_t *v) { *v = h_readd(s, a); return 0; }
static int __cdecl h_wbc(void *s, uint32_t a, uint32_t v) { h_writeb(s, a, v); return 0; }
static int __cdecl h_wwc(void *s, uint32_t a, uint32_t v) { h_writew(s, a, v); return 0; }
static int __cdecl h_wdc(void *s, uint32_t a, uint32_t v) { h_writed(s, a, v); return 0; }
static void __cdecl h_dtor(void *s) { }
static uint8_t *__cdecl h_hostw(void *s, uint32_t pg);
static void *g_vt[16] = { h_dtor, h_dtor, h_readb, h_readw, h_readd, h_writeb, h_writew, h_writed,
                          h_hostr, h_hostw, h_rbc, h_rwc, h_rdc, h_wbc, h_wwc, h_wdc };
static struct { void **vt; uint32_t flags; } g_handler = { g_vt, 2 };

static uint8_t *__cdecl h_hostw(void *s, uint32_t pg) { return g_pg[pg].orig + pg * 4096; }

/* DOSBox resets the TLB (EMS mapping etc.) to init_page_handler; wrap its vtable so
 * that pages we trap are re-trapped right after the original handler relinked them. */
typedef uint32_t (__cdecl *rd_fn)(void *, uint32_t);
typedef void (__cdecl *wr_fn)(void *, uint32_t, uint32_t);
typedef int (__cdecl *rdc_fn)(void *, uint32_t, void *);
typedef int (__cdecl *wrc_fn)(void *, uint32_t, uint32_t);
static void *g_init_orig[16];
static void retrap(void)
{
    int pg;
    for (pg = 0; pg < 256; pg++) {
        if (!g_pg[pg].on || TLBRH[pg] == &g_handler) continue;
        if (TLBR[pg]) g_pg[pg].orig = TLBR[pg];
        TLBR[pg] = NULL;
        TLBRH[pg] = &g_handler;
    }
}
#define IN_TRAP(a) ((a) >> 12 < 256 && g_pg[(a) >> 12].on)
static uint32_t __cdecl i_readb(void *s, uint32_t a) { uint32_t r = ((rd_fn)g_init_orig[2])(s, a); if (IN_TRAP(a)) { on_read(a, 1); retrap(); } return r; }
static uint32_t __cdecl i_readw(void *s, uint32_t a) { uint32_t r = ((rd_fn)g_init_orig[3])(s, a); if (IN_TRAP(a)) { on_read(a, 2); retrap(); } return r; }
static uint32_t __cdecl i_readd(void *s, uint32_t a) { uint32_t r = ((rd_fn)g_init_orig[4])(s, a); if (IN_TRAP(a)) { on_read(a, 4); retrap(); } return r; }
static void __cdecl i_writeb(void *s, uint32_t a, uint32_t v) { ((wr_fn)g_init_orig[5])(s, a, v); if (IN_TRAP(a)) retrap(); }
static void __cdecl i_writew(void *s, uint32_t a, uint32_t v) { ((wr_fn)g_init_orig[6])(s, a, v); if (IN_TRAP(a)) retrap(); }
static void __cdecl i_writed(void *s, uint32_t a, uint32_t v) { ((wr_fn)g_init_orig[7])(s, a, v); if (IN_TRAP(a)) retrap(); }
static int __cdecl i_rbc(void *s, uint32_t a, void *v) { int r = ((rdc_fn)g_init_orig[10])(s, a, v); if (IN_TRAP(a)) { on_read(a, 1); retrap(); } return r; }
static int __cdecl i_rwc(void *s, uint32_t a, void *v) { int r = ((rdc_fn)g_init_orig[11])(s, a, v); if (IN_TRAP(a)) { on_read(a, 2); retrap(); } return r; }
static int __cdecl i_rdc(void *s, uint32_t a, void *v) { int r = ((rdc_fn)g_init_orig[12])(s, a, v); if (IN_TRAP(a)) { on_read(a, 4); retrap(); } return r; }
static int __cdecl i_wbc(void *s, uint32_t a, uint32_t v) { int r = ((wrc_fn)g_init_orig[13])(s, a, v); if (IN_TRAP(a)) retrap(); return r; }
static int __cdecl i_wwc(void *s, uint32_t a, uint32_t v) { int r = ((wrc_fn)g_init_orig[14])(s, a, v); if (IN_TRAP(a)) retrap(); return r; }
static int __cdecl i_wdc(void *s, uint32_t a, uint32_t v) { int r = ((wrc_fn)g_init_orig[15])(s, a, v); if (IN_TRAP(a)) retrap(); return r; }

static int g_hooked;
static int hook_init_handler(void)
{
    int pg;
    void **vt;
    DWORD old;
    for (pg = 0; pg < 0x100; pg++) {
        void *h = TLBRH[pg];
        if (h && h != &g_handler && (((uint32_t*)h)[1] & 0x20)) break;
    }
    if (pg == 0x100) return 0;
    vt = *(void***)TLBRH[pg];
    memcpy(g_init_orig, vt, sizeof g_init_orig);
    VirtualProtect(vt, 64, PAGE_EXECUTE_READWRITE, &old);
    vt[2] = i_readb; vt[3] = i_readw; vt[4] = i_readd; vt[5] = i_writeb; vt[6] = i_writew; vt[7] = i_writed;
    vt[10] = i_rbc; vt[11] = i_rwc; vt[12] = i_rdc; vt[13] = i_wbc; vt[14] = i_wwc; vt[15] = i_wdc;
    VirtualProtect(vt, 64, old, &old);
    logf("init handler %p vt %p hooked\n", TLBRH[pg], vt);
    g_hooked = 1;
    return 1;
}

static void trap_page(uint32_t pg)
{
    if (pg >= 256 || g_pg[pg].on) return;
    if (!TLBR[pg]) return;
    g_pg[pg].orig = TLBR[pg];
    g_pg[pg].on = 1;
    TLBRH[pg] = &g_handler;
    TLBR[pg] = NULL;
}

/* ---- needle scanning thread ---- */
typedef struct { char s[128]; int len; int found; char name[48]; uint32_t span; } needle;
static needle g_nd[64];
static int g_nnd;

static void load_needles(void)
{
    char p[MAX_PATH], line[512];
    FILE *f;
    wsprintfA(p, "%s\\needles.txt", g_dir);
    f = fopen(p, "r");
    if (!f) return;
    /* format: name<TAB>span<TAB>needle */
    while (fgets(line, sizeof line, f) && g_nnd < 64) {
        char *t1 = strchr(line, '\t'), *t2;
        size_t L;
        if (!t1) continue;
        *t1++ = 0;
        t2 = strchr(t1, '\t');
        if (!t2) continue;
        *t2++ = 0;
        L = strlen(t2);
        while (L && (t2[L - 1] == '\n' || t2[L - 1] == '\r')) t2[--L] = 0;
        lstrcpynA(g_nd[g_nnd].name, line, 48);
        g_nd[g_nnd].span = strtoul(t1, NULL, 0);
        if (t2[0] == '0' && t2[1] == 'x') {   /* hex needle */
            int k = 0;
            const char *q = t2 + 2;
            while (q[0] && q[1] && k < 127) {
                char hx[3] = { q[0], q[1], 0 };
                g_nd[g_nnd].s[k++] = (char)strtoul(hx, NULL, 16);
                q += 2;
            }
            L = (size_t)k;
        } else
            lstrcpynA(g_nd[g_nnd].s, t2, 128);
        g_nd[g_nnd].len = (int)L;
        g_nnd++;
    }
    fclose(f);
}

static DWORD WINAPI scan_thread(LPVOID unused)
{
    uint32_t a;
    int i;
    Sleep(1500);
    for (;;) {
        Sleep(300);
        static int tries;
        if (!g_hooked && !hook_init_handler() && tries++ < 5) continue;
        for (i = 0; i < g_nnd; i++) {
            needle *n = &g_nd[i];
            if (n->found) continue;
            for (a = 0x1000; a < 0x9f000; a++) {
                uint8_t *p = hp(a);
                if (!p || *p != (uint8_t)n->s[0]) continue;
                if (((a + n->len) >> 12) != (a >> 12)) {
                    int j, ok = 1;
                    for (j = 0; j < n->len && ok; j++) { uint8_t *q = hp(a + j); if (!q || *q != (uint8_t)n->s[j]) ok = 0; }
                    if (!ok) continue;
                } else if (memcmp(p, n->s, n->len)) continue;
                {
                    uint32_t pg;
                    EnterCriticalSection(&g_cs);
                    if (g_nreg < 64) {
                        g_reg[g_nreg].start = a;
                        g_reg[g_nreg].end = a + (n->span ? n->span : (uint32_t)n->len);
                        lstrcpyA(g_reg[g_nreg].name, n->name);
                        g_nreg++;
                    }
                    logf("FOUND %s at %05x\n", n->name, a);
                    LeaveCriticalSection(&g_cs);
                    for (pg = a >> 12; pg <= (a + (n->span ? n->span : n->len)) >> 12; pg++) trap_page(pg);
                    n->found = 1;
                    break;
                }
            }
        }
    }
    return 0;
}

static void init(void)
{
    char p[MAX_PATH], *s;
    GetModuleFileNameA(NULL, p, MAX_PATH);
    s = strrchr(p, '\\');
    if (s) *s = 0;
    wsprintfA(g_dir, "%s\\..\\_jp\\trace", p);
    InitializeCriticalSection(&g_cs);
    if ((uintptr_t)GetModuleHandleA(NULL) != 0x400000) return;
    wsprintfA(p, "%s\\trace.log", g_dir);
    g_log = fopen(p, "w");
    load_needles();
    logf("trace start, %d needles\n", g_nnd);
    CreateThread(NULL, 0, scan_thread, NULL, 0, NULL);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        init();
    }
    return TRUE;
}
