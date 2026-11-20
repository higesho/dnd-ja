/*
 * 開発用：テストのときだけ（環境変数 EOBJP_DEV=1）、<game>\_jp\test\cmd.txt を見張り、
 * 書かれたキーを SDL のイベント列へ直接入れる。ウインドウが前面でなくても効く。
 *   1行1コマンド： key NAME [NAME..] / text ABC / wait MS
 */
#include <windows.h>
#include <stdio.h>
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
