/*
 * Unlimited Adventures 日本語入力
 *
 * DOSBox の中では Windows の IME が使えない。そこで、DOSBox のウインドウで F12（または 半角/全角）を押すと
 * Windows の小さな入力欄を出す。そこで普段の IME で日本語を打って OK を押すと、その文を UA の保存形式
 * （uajp.h: 記号 2〜3 個で日本語 1 字）に直し、その記号をキー入力としてゲームに送る。
 * ゲームのエディタから見ると、記号が順にキーボードから打たれただけなので、文字数の上限や保存はゲームの処理のまま働く。
 *
 * しくみ: DOSBox.exe が呼ぶ SDL_PollEvent を横取りする。
 *   - F12 / 半角全角 のキーはゲームに渡さず、入力欄を開く
 *   - 送るキーは待ち行列に入れ、SDL_PollEvent が呼ばれるたびに少しずつ（時間をあけて）返す
 */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "uajp.h"

#ifdef DS_TEST
void ua_log(const char *fmt, ...);
#else
#define ua_log(...) ((void)0)   /* 配布版: ログは書かない */
#endif

/* ---------------------------------------------------------------- キーの待ち行列 */
unsigned ua_ed_acks(void);     /* ua_jp.c: 入力欄のカーソルが描かれた回数（キーが 1 つ処理されるたびに増える） */
int ua_kbd_empty(void);        /* ua_jp.c: BIOS のキーバッファが空か */

typedef struct { uint8_t type, sc, first, last; int sym; } qev;      /* first / last: 1 字ぶんの最初 / 最後のイベント */
#define QMAX 8192
static qev g_q[QMAX];
static volatile LONG g_qh, g_qt;
static DWORD g_qnext;
#define KEY_MS 14          /* 時間で送るときの間隔（ミリ秒）。短すぎるとゲームが取りこぼす */
/* 送り方: 文章の入力欄では、ゲームが 1 字を処理してカーソルを描き直したのを確かめてから次の字を送る（取りこぼすと、
 * 残りのキーがボタンのホットキーに当たってしまう）。カーソルの描き直しが見えない欄（名前の入力など）では時間で送る。 */
static int g_mode;         /* 0 = まだわからない, 1 = 確かめながら送る, 2 = 時間で送る */
static int g_wait;         /* 1 字送り終えて、処理されるのを待っている */
static unsigned g_ack0;
static DWORD g_tsent;

static void q_put(int type, int sc, int sym)
{
    LONG t = g_qt;
    if (((t + 1) % QMAX) == g_qh) return;
    g_q[t].type = (uint8_t)type; g_q[t].sc = (uint8_t)sc; g_q[t].sym = sym; g_q[t].first = g_q[t].last = 0;
    g_qt = (t + 1) % QMAX;
}
/* 1 字（UA の 64 種類の記号のどれか）を US 配列のキーにして待ち行列へ */
static void q_char(char c)
{
    static const char row1[] = "1234567890", row2[] = "qwertyuiop", row3[] = "asdfghjkl", row4[] = "zxcvbnm";
    static const struct { char c; uint8_t sc, shift; } sy[] = {
        {' ', 0x39, 0}, {'!', 0x02, 1}, {'"', 0x28, 1}, {'#', 0x04, 1}, {'$', 0x05, 1}, {'%', 0x06, 1}, {'&', 0x08, 1},
        {'\'', 0x28, 0}, {'(', 0x0a, 1}, {')', 0x0b, 1}, {'*', 0x09, 1}, {'+', 0x0d, 1}, {',', 0x33, 0}, {'-', 0x0c, 0},
        {'.', 0x34, 0}, {'/', 0x35, 0}, {':', 0x27, 1}, {';', 0x27, 0}, {'<', 0x33, 1}, {'=', 0x0d, 0}, {'>', 0x34, 1},
        {'?', 0x35, 1}, {'[', 0x1a, 0}, {']', 0x1b, 0}, {'^', 0x07, 1}, {0, 0, 0}};
    int sc = 0, shift = 0, sym = (uint8_t)c, k;
    const char *p;
    if (c >= 'A' && c <= 'Z') c = (char)(c + 0x20);
    if (c >= 'a' && c <= 'z') {
        sym = c;
        if ((p = strchr(row2, c)) != NULL) sc = 0x10 + (int)(p - row2);
        else if ((p = strchr(row3, c)) != NULL) sc = 0x1e + (int)(p - row3);
        else if ((p = strchr(row4, c)) != NULL) sc = 0x2c + (int)(p - row4);
    } else if (c >= '0' && c <= '9') sc = 0x02 + (int)(strchr(row1, c) - row1);
    else for (k = 0; sy[k].c; k++) if (sy[k].c == c) { sc = sy[k].sc; shift = sy[k].shift; }
    if (!sc) return;
    k = g_qt;
    if (shift) q_put(2, 0x2a, 304);
    q_put(2, sc, sym); q_put(3, sc, sym);
    if (shift) q_put(3, 0x2a, 304);
    if (g_qt != k) { g_q[k].first = 1; g_q[(g_qt + QMAX - 1) % QMAX].last = 1; }
}

/* ---------------------------------------------------------------- 文 → 記号 */
/* UTF-16 の文を UA の記号の並びに直す。入れられない字は ? にする。返り値は記号の数 */
static int encode_text(const WCHAR *w, char *out, int max)
{
    int o = 0;
    for (; *w && o < max - 4; w++) {
        WCHAR c = *w;
        char mb[4];
        BOOL bad = FALSE;
        int n, ku, ten, r;
        uint8_t s1, s2;
        if (c == L'\r') continue;
        if (c == L'\n' || c == L'\t' || c == 0x3000) c = L' ';
        if (c >= 0xff10 && c <= 0xff19) c = (WCHAR)(c - 0xfee0);                 /* 全角の数字 → 半角 */
        if ((c >= 0xff21 && c <= 0xff3a) || (c >= 0xff41 && c <= 0xff5a)) c = (WCHAR)(c - 0xfee0);   /* 全角の英字 → 半角 */
        if (c >= L'a' && c <= L'z') c = (WCHAR)(c - 0x20);
        if (c < 0x80) {
            /* 半角のまま入れられる字（日本語の符号の先頭に使う記号 # $ < = > [ ] と % などは全角にして入れる） */
            if (c == L' ' || (c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'Z') || (c && strchr("!\"&'()*+,-./:;?^", (char)c))) {
                out[o++] = (char)c; continue;
            }
            if (c > 0x20 && c < 0x7f) c = (WCHAR)(c + 0xfee0); else continue;
        }
        n = WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, &c, 1, mb, 4, NULL, &bad);
        if (n != 2 || bad) { out[o++] = '?'; continue; }
        s1 = (uint8_t)mb[0]; s2 = (uint8_t)mb[1];
        if (s1 >= 0x81 && s1 <= 0x9f) r = s1 - 0x81; else if (s1 >= 0xe0 && s1 <= 0xef) r = s1 - 0xc1; else { out[o++] = '?'; continue; }
        if (s2 >= 0x9f) { ku = r * 2 + 2; ten = s2 - 0x9e; }
        else { ku = r * 2 + 1; ten = s2 - 0x3f - (s2 >= 0x80 ? 1 : 0); }
        n = (ku >= 1 && ku <= 84 && ten >= 1 && ten <= 94) ? uajp_encode_kuten(ku, ten, out + o) : 0;
        if (n) o += n; else out[o++] = '?';
    }
    out[o] = 0;
    return o;
}

/* ---------------------------------------------------------------- 入力欄 */
#define IDC_EDIT 100
#define IDC_INFO 101
#ifdef DS_TEST
static int g_test;                 /* テスト中（DS_TEST=1）: 入力欄を前面に出さない */
#else
#define g_test 0                   /* 配布版: いつもふつうに入力欄を前面に出す */
#endif
static int g_dlg_done;
static WCHAR g_text[1024];
static HFONT g_font;

static INT_PTR CALLBACK ime_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
    case WM_INITDIALOG:
        if (!g_font) g_font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, 0, 0, 0, 0, L"Yu Gothic UI");
        SendDlgItemMessageW(h, IDC_EDIT, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendDlgItemMessageW(h, IDC_EDIT, EM_SETLIMITTEXT, 500, 0);
        if (!g_test) SetFocus(GetDlgItem(h, IDC_EDIT));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(w) == IDC_EDIT && HIWORD(w) == EN_CHANGE) {      /* 打った文が記号いくつ分になるかを見せる */
            static WCHAR t[1024], msg[100];
            static char sym[2200];
            GetDlgItemTextW(h, IDC_EDIT, t, 1000);
            wsprintfW(msg, L"ゲームの文字数にして %d 字ぶん", encode_text(t, sym, sizeof sym));
            SetDlgItemTextW(h, IDC_INFO, msg);
            return TRUE;
        }
        if (LOWORD(w) == IDOK) { GetDlgItemTextW(h, IDC_EDIT, g_text, 1000); g_dlg_done = 1; return TRUE; }
        if (LOWORD(w) == IDCANCEL) { g_dlg_done = 2; return TRUE; }
        break;
    case WM_CLOSE:
        g_dlg_done = 2; return TRUE;
    }
    return FALSE;
}

/* 入力欄のひな形（リソースを使わずにメモリ上で作る） */
static uint8_t *tmpl_align(uint8_t *p) { return (uint8_t*)(((uintptr_t)p + 3) & ~(uintptr_t)3); }
static uint8_t *tmpl_str(uint8_t *p, const WCHAR *s) { int n = (lstrlenW(s) + 1) * 2; memcpy(p, s, n); return p + n; }
static uint8_t *tmpl_item(uint8_t *p, DWORD style, int x, int y, int cx, int cy, int id, WORD cls, const WCHAR *text)
{
    DLGITEMTEMPLATE *it;
    p = tmpl_align(p);
    it = (DLGITEMTEMPLATE*)p;
    it->style = style | WS_CHILD | WS_VISIBLE; it->dwExtendedStyle = 0;
    it->x = (short)x; it->y = (short)y; it->cx = (short)cx; it->cy = (short)cy; it->id = (WORD)id;
    p += sizeof(DLGITEMTEMPLATE);
    *(WORD*)p = 0xffff; p += 2; *(WORD*)p = cls; p += 2;
    p = tmpl_str(p, text);
    *(WORD*)p = 0; p += 2;
    return p;
}
static BOOL CALLBACK find_main(HWND h, LPARAM l)
{
    DWORD pid;
    char cls[32];
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(h)) return TRUE;
    GetClassNameA(h, cls, sizeof cls);
    if (lstrcmpA(cls, "SDL_app")) return TRUE;
    *(HWND*)l = h;
    return FALSE;
}

/* 入力欄を開いて、OK なら文を記号にしてキーの待ち行列に入れる */
static void open_ime(void)
{
    static uint8_t buf[1024];
    static char sym[2200];
    DLGTEMPLATE *dt = (DLGTEMPLATE*)buf;
    uint8_t *p;
    HWND owner = NULL, dlg;
    MSG msg;
    int n, k;
    memset(buf, 0, sizeof buf);
    EnumWindows(find_main, (LPARAM)&owner);
    dt->style = DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dt->cdit = 5; dt->x = 0; dt->y = 0; dt->cx = 300; dt->cy = 112;
    p = buf + sizeof(DLGTEMPLATE);
    *(WORD*)p = 0; p += 2;                      /* メニューなし */
    *(WORD*)p = 0; p += 2;                      /* 既定のクラス */
    p = tmpl_str(p, L"Unlimited Adventures 日本語入力");
    *(WORD*)p = 9; p += 2;
    p = tmpl_str(p, L"Yu Gothic UI");
    p = tmpl_item(p, SS_LEFT, 8, 6, 284, 10, -1, 0x0082, L"入れたい文を日本語で打って、OK（Enter）を押してください。");
    p = tmpl_item(p, ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | WS_TABSTOP | WS_VSCROLL, 8, 20, 284, 56, IDC_EDIT, 0x0081, L"");
    p = tmpl_item(p, SS_LEFT, 8, 80, 150, 10, IDC_INFO, 0x0082, L"ゲームの文字数にして 0 字ぶん");
    p = tmpl_item(p, BS_DEFPUSHBUTTON | WS_TABSTOP, 184, 92, 50, 14, IDOK, 0x0080, L"OK");
    p = tmpl_item(p, BS_PUSHBUTTON | WS_TABSTOP, 242, 92, 50, 14, IDCANCEL, 0x0080, L"キャンセル");
    g_dlg_done = 0; g_text[0] = 0;
    dlg = CreateDialogIndirectParamW(GetModuleHandleW(NULL), dt, g_test ? NULL : owner, ime_proc, 0);
    if (!dlg) { ua_log("ime: dialog failed %d\n", (int)GetLastError()); return; }
    if (g_test) ShowWindow(dlg, SW_SHOWNOACTIVATE);
    else {
        ClipCursor(NULL);
        while (ShowCursor(TRUE) < 0) {}
        if (owner) EnableWindow(owner, FALSE);
        ShowWindow(dlg, SW_SHOW);
        SetForegroundWindow(dlg);
        SetFocus(GetDlgItem(dlg, IDC_EDIT));
    }
    while (!g_dlg_done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(dlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (!g_test) {
        if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    }
    DestroyWindow(dlg);
    if (g_dlg_done != 1) return;
    n = encode_text(g_text, sym, sizeof sym);
    ua_log("ime: %d symbols [%s]\n", n, sym);
    g_mode = 0; g_wait = 0;
    for (k = 0; k < n; k++) q_char(sym[k]);
    g_qnext = GetTickCount() + 150;
}

/* ---------------------------------------------------------------- SDL_PollEvent の横取り */
typedef int (__cdecl *poll_t)(void *);
static poll_t real_poll;
static int g_busy;
uint32_t ua_ed_struct(void);       /* ua_jp.c: 最後にカーソルを描いた入力欄 */
int ua_ed_charlen(int where);      /* ua_jp.c: カーソルの前（0）／位置（1）の 1 字が記号いくつ分か */
static struct { int extra, sym, sc, acked, up; uint32_t S0; unsigned ack0; DWORD t0; } g_rep;
static int __cdecl hook_poll(void *ev)
{
    int r;
    if (ev && g_qh != g_qt && (int)(GetTickCount() - g_qnext) >= 0) {
        DWORD now = GetTickCount();
        if (g_wait) {              /* 前の 1 字が処理されるのを待つ */
            if (ua_ed_acks() != g_ack0) { g_wait = 0; g_mode = 1; }
            else if (g_mode != 1 && now - g_tsent > 400) { g_wait = 0; g_mode = 2; }        /* カーソルの描き直しが見えない欄 */
            else if (g_mode == 1 && now - g_tsent > 2500) {      /* 入力欄が受け付けなくなった（いっぱい、欄を出た）: 残りは送らない */
                ua_log("ime: input stopped, %d events dropped\n", (int)((g_qt + QMAX - g_qh) % QMAX));
                g_qh = g_qt; g_wait = 0;
            }
        }
        if (!g_wait && g_qh != g_qt && ua_kbd_empty()) {
            uint8_t *b = (uint8_t*)ev;
            qev *q = &g_q[g_qh];
            if (q->first) g_ack0 = ua_ed_acks();
            memset(b, 0, 20);
            b[0] = q->type; b[2] = q->type == 2 ? 1 : 0; b[4] = q->sc; *(int32_t*)(b + 8) = q->sym;
            if (q->last) {
                g_tsent = now;
                if (g_mode != 2) g_wait = 1;
            }
            g_qh = (g_qh + 1) % QMAX;
            g_qnext = now + (g_mode == 2 ? KEY_MS : 4);
            return 1;
        }
    }
    /* Backspace・Delete・左右の矢印: エディタは記号 1 つ分しか動かさない。日本語 1 字は記号 2〜3 個なので、
     * 1 回目のキーを入力欄が処理した（カーソルを描き直した）のを確かめてから、残りの回数ぶん同じキーを足す */
    if (g_rep.extra > 0 && g_qh == g_qt) {
        DWORD now = GetTickCount();
        if (!g_rep.acked) {
            if (ua_ed_acks() != g_rep.ack0) {
                if (ua_ed_struct() == g_rep.S0) g_rep.acked = 1; else g_rep.extra = 0;
            } else if (now - g_rep.t0 > 400) g_rep.extra = 0;      /* 入力欄ではなかった */
        }
        /* 元のキーが離されてから送る（押されたままのキーをもう一度「押す」と無視される） */
        if (g_rep.extra > 0 && g_rep.acked && g_rep.up) {
            int k;
            for (k = 0; k < g_rep.extra; k++) {
                LONG t = g_qt;
                q_put(2, g_rep.sc, g_rep.sym); q_put(3, g_rep.sc, g_rep.sym);
                if (g_qt != t) { g_q[t].first = 1; g_q[(g_qt + QMAX - 1) % QMAX].last = 1; }
            }
            g_mode = 1; g_wait = 0; g_qnext = now + 20;
            g_rep.extra = 0;
        } else if (g_rep.extra > 0 && now - g_rep.t0 > 3000) g_rep.extra = 0;
    }
    for (;;) {
        uint8_t *b = (uint8_t*)ev;
        r = real_poll(ev);
        if (!r || !ev || (b[0] != 2 && b[0] != 3)) return r;
        if (b[0] == 2 && g_qh == g_qt && !g_rep.extra) {
            int sym = *(int32_t*)(b + 8), n = 0;
            if (sym == 8 || sym == 276) n = ua_ed_charlen(0);            /* Backspace, 左: カーソルの前の字 */
            else if (sym == 127 || sym == 275) n = ua_ed_charlen(1);     /* Delete, 右: カーソルの位置の字 */
            if (n > 1) {
                g_rep.extra = n - 1; g_rep.sym = sym; g_rep.sc = b[4]; g_rep.acked = 0; g_rep.up = 0;
                g_rep.S0 = ua_ed_struct(); g_rep.ack0 = ua_ed_acks(); g_rep.t0 = GetTickCount();
            }
        }
        if (b[0] == 3 && g_rep.extra > 0 && *(int32_t*)(b + 8) == g_rep.sym) g_rep.up = 1;
        if (b[4] != 0x29 && *(int32_t*)(b + 8) != 293) return r;      /* 半角/全角（スキャンコード 29h）か F12 */
        if (b[0] == 2 && !g_busy && g_qh == g_qt) { g_busy = 1; open_ime(); g_busy = 0; }
        /* このキーはゲームに渡さない: 次のイベントを見る */
    }
}

void ime_init(void)
{
    uint8_t *b = (uint8_t*)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS*)(b + ((IMAGE_DOS_HEADER*)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR*)(b + dd.VirtualAddress);
#ifdef DS_TEST
    char v[8];
    g_test = GetEnvironmentVariableA("DS_TEST", v, sizeof v) && v[0] == '1';
#endif
    for (; d->Name; d++) {
        IMAGE_THUNK_DATA *ot, *ft;
        if (lstrcmpiA((const char*)(b + d->Name), "SDL.dll")) continue;
        ot = (IMAGE_THUNK_DATA*)(b + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        ft = (IMAGE_THUNK_DATA*)(b + d->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;
            if (ot->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME*)(b + ot->u1.AddressOfData);
            if (strcmp((const char*)ibn->Name, "SDL_PollEvent")) continue;
            real_poll = (poll_t)ft->u1.Function;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
            ft->u1.Function = (DWORD)(uintptr_t)hook_poll;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
        }
    }
    ua_log("ime: SDL_PollEvent hook %s\n", real_poll ? "ok" : "not found");
}
