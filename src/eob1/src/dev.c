/*
 * 開発用：テストのときだけ（環境変数 EOBJP_DEV=1）、<game>\_jp\test\cmd.txt を見張り、
 * 書かれたキーを SDL のイベント列へ直接入れる。ウインドウが前面でなくても効く。
 *   1行1コマンド： key NAME [NAME..] / text ABC / wait MS
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef int (__cdecl *push_t)(void *ev);
static push_t g_push;
static char g_cmd[MAX_PATH];

#pragma pack(push, 1)
typedef struct {
    uint8_t type, which, state, pad0;
    uint8_t scancode, pad1[3];
    int32_t sym;
    int32_t mod;
    uint16_t unicode, pad2;
    uint8_t rest[8];
} sdl_key_event;
#pragma pack(pop)

static const struct { const char *n; int sym; } g_keys[] = {
    {"enter", 13}, {"esc", 27}, {"space", 32}, {"back", 8}, {"tab", 9},
    {"up", 273}, {"down", 274}, {"right", 275}, {"left", 276},
    {"ins", 277}, {"home", 278}, {"end", 279}, {"pgup", 280}, {"pgdn", 281},
    {"kp0", 256}, {"kp1", 257}, {"kp2", 258}, {"kp3", 259}, {"kp4", 260}, {"kp5", 261},
    {"kp6", 262}, {"kp7", 263}, {"kp8", 264}, {"kp9", 265},
    {"f1", 282}, {"f2", 283}, {"f3", 284}, {"f4", 285}, {"f5", 286}, {"f6", 287},
    {"f7", 288}, {"f8", 289}, {"f9", 290}, {"f10", 291},
    {"lshift", 304}, {"lctrl", 306}, {"lalt", 308},
    {NULL, 0}
};

static void send_sym(int sym, int down)
{
    sdl_key_event e;
    memset(&e, 0, sizeof e);
    e.type = down ? 2 : 3;
    e.state = down ? 1 : 0;
    e.sym = sym;
    g_push(&e);
}

static void tap(int sym)
{
    send_sym(sym, 1);
    Sleep(60);
    send_sym(sym, 0);
    Sleep(60);
}

static int sym_of(const char *n)
{
    int i;
    if (!n[1]) {
        char c = n[0];
        if (c >= 'A' && c <= 'Z') c += 32;
        return (uint8_t)c;
    }
    for (i = 0; g_keys[i].n; i++) if (!_stricmp(g_keys[i].n, n)) return g_keys[i].sym;
    return 0;
}

/* 画面（SDL の画面サーフェス）を BMP に書き出す: shot PATH。ウィンドウの描画に頼らないので、窓が白いままでも撮れる */
typedef struct { uint8_t r, g, b, unused; } sdl_color;
typedef struct { int ncolors; sdl_color *colors; } sdl_palette;
typedef struct {
    sdl_palette *palette; uint8_t bpp, Bpp, Rloss, Gloss, Bloss, Aloss, Rshift, Gshift, Bshift, Ashift;
    uint32_t Rmask, Gmask, Bmask, Amask;
} sdl_format;
typedef struct { uint32_t flags; sdl_format *format; int w, h; uint16_t pitch; void *pixels; } sdl_surface;

static void save_bmp(const char *path)
{
    typedef sdl_surface *(__cdecl *gvs_t)(void);
    HMODULE sdl = GetModuleHandleA("SDL.dll");
    gvs_t gvs = sdl ? (gvs_t)GetProcAddress(sdl, "SDL_GetVideoSurface") : NULL;
    sdl_surface *s = gvs ? gvs() : NULL;
    typedef int (__cdecl *lock_t)(sdl_surface *);
    typedef void (__cdecl *unlock_t)(sdl_surface *);
    lock_t lock = sdl ? (lock_t)GetProcAddress(sdl, "SDL_LockSurface") : NULL;
    unlock_t unlock = sdl ? (unlock_t)GetProcAddress(sdl, "SDL_UnlockSurface") : NULL;
    int locked = 0;
    int w, h, x, y, stride;
    const sdl_format *f;
    BITMAPFILEHEADER fh; BITMAPINFOHEADER ih;
    uint8_t *row;
    FILE *o;
    char tmp[MAX_PATH];
    if (s && !s->pixels && lock && unlock && lock(s) == 0) locked = 1;
    if (!s || !s->pixels) {
        char e[MAX_PATH];
        FILE *ef;
        wsprintfA(e, "%s.err", path);
        ef = fopen(e, "w");
        if (ef) { fprintf(ef, "sdl %p gvs %p surface %p pixels %p flags %08x\n", (void*)sdl, (void*)gvs, (void*)s, s ? s->pixels : NULL, s ? s->flags : 0); fclose(ef); }
        return;
    }
    w = s->w; h = s->h; stride = (w * 3 + 3) & ~3; f = s->format;
    wsprintfA(tmp, "%s.tmp", path);
    row = (uint8_t*)malloc(stride);
    o = fopen(tmp, "wb");
    if (!o || !row) { if (o) fclose(o); free(row); if (locked) unlock(s); return; }
    memset(&fh, 0, sizeof fh); memset(&ih, 0, sizeof ih);
    fh.bfType = 0x4d42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + stride * h;
    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 24;
    fwrite(&fh, sizeof fh, 1, o); fwrite(&ih, sizeof ih, 1, o);
    for (y = 0; y < h; y++) {
        const uint8_t *p = (const uint8_t*)s->pixels + y * s->pitch;
        memset(row, 0, stride);
        for (x = 0; x < w; x++) {
            uint32_t v; uint8_t r, g, b;
            if (f->Bpp == 1) {
                sdl_color c = f->palette->colors[p[x]]; r = c.r; g = c.g; b = c.b;
            } else {
                if (f->Bpp == 2) v = ((const uint16_t*)p)[x];
                else if (f->Bpp == 3) v = p[x * 3] | (p[x * 3 + 1] << 8) | (p[x * 3 + 2] << 16);
                else v = ((const uint32_t*)p)[x];
                r = (uint8_t)(((v & f->Rmask) >> f->Rshift) << f->Rloss);
                g = (uint8_t)(((v & f->Gmask) >> f->Gshift) << f->Gloss);
                b = (uint8_t)(((v & f->Bmask) >> f->Bshift) << f->Bloss);
            }
            row[x * 3] = b; row[x * 3 + 1] = g; row[x * 3 + 2] = r;
        }
        fwrite(row, 1, stride, o);
    }
    fclose(o); free(row);
    if (locked) unlock(s);
    DeleteFileA(path);
    MoveFileA(tmp, path);
}

static void run_line(char *line)
{
    char *tok, *ctx = NULL;
    size_t L = strlen(line);
    while (L && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;
    if (!strncmp(line, "text ", 5)) {
        const char *p;
        char one[2] = {0, 0};
        for (p = line + 5; *p; p++) { one[0] = *p; tap(*p == ' ' ? 32 : sym_of(one)); }
        return;
    }
    if (!strncmp(line, "shot ", 5)) { save_bmp(line + 5); return; }
    tok = strtok_s(line, " ", &ctx);
    if (!tok) return;
    if (!strcmp(tok, "wait")) { tok = strtok_s(NULL, " ", &ctx); if (tok) Sleep(atoi(tok)); return; }
    if (!strcmp(tok, "key")) {
        while ((tok = strtok_s(NULL, " ", &ctx)) != NULL) {
            int s = sym_of(tok);
            if (s) tap(s);
        }
    }
}

static DWORD WINAPI dev_thread(LPVOID u)
{
    char line[512];
    (void)u;
    for (;;) {
        FILE *f;
        Sleep(50);
        if (!g_push) {
            HMODULE sdl = GetModuleHandleA("SDL.dll");
            if (sdl) g_push = (push_t)GetProcAddress(sdl, "SDL_PushEvent");
            continue;
        }
        f = fopen(g_cmd, "r");
        if (!f) continue;
        while (fgets(line, sizeof line, f)) run_line(line);
        fclose(f);
        DeleteFileA(g_cmd);
    }
    return 0;
}

void dev_init(const char *gamedir)
{
    char v[8];
    if (!GetEnvironmentVariableA("EOBJP_DEV", v, sizeof v) || v[0] != '1') return;
    wsprintfA(g_cmd, "%s\\_jp\\test\\cmd.txt", gamedir);
    DeleteFileA(g_cmd);
    CreateThread(NULL, 0, dev_thread, NULL, 0, NULL);
}
