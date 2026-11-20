/* 折り返し: ゲーム（0x44a140）は空白・改行だけを区切りとして語ごとに行を分ける。
   区切りの判定（0x44a1cd: mov cx,[edx]; cmp cx,20h; je; cmp cx,0Ah; je）を差し替え、日本語の字も区切りにする。
   行頭禁則: 次の字が「、。」」などなら、この字では区切らない（2 字まとめて次の行へ送られる）。
   行末禁則: 「（ などでは区切らない。
   文の最後の 1 字だけが次の行に残るような区切りもしない。 */
#include "dst.h"

static const uint16_t NO_START[] = { 0x3001, 0x3002, 0xff0c, 0xff0e, 0x30fb, 0xff1a, 0xff1b, 0xff1f, 0xff01, 0x30fc, 0x2026, 0x2025,
    0x300d, 0x300f, 0xff09, 0xff3d, 0xff5d, 0x3015, 0x3009, 0x300b, 0x3011, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083,
    0x3085, 0x3087, 0x308e, 0x30a1, 0x30a3, 0x30a5, 0x30a7, 0x30a9, 0x30c3, 0x30e3, 0x30e5, 0x30e7, 0x30ee, 0x30f5, 0x30f6, 0x3005,
    0x301c, 0xff5e, '!', '?', '.', ',', ')', 0 };
static const uint16_t NO_END[] = { 0x300c, 0x300e, 0xff08, 0xff3b, 0xff5b, 0x3014, 0x3008, 0x300a, 0x3010, '(', 0 };

static int in(const uint16_t *set, uint16_t c) { for (; *set; set++) if (*set == c) return 1; return 0; }
static int wide(uint16_t c) { return c >= 0x2e80; }

static int __cdecl is_break(const uint16_t *p)
{
    uint16_t c = p[0], n = p[1];
    if (c == 0x20 || c == 0x0a) return 1;
    if (!wide(c)) return 0;
    if (n && in(NO_START, n)) return 0;
    if (n && (p[2] == 0 || p[2] == 0x0a || p[2] == 0x20)) return 0;   /* 最後の 1 字だけを次の行に送らない */
    if (in(NO_END, c)) return 0;
    return 1;
}

static uint32_t g_yes, g_no;
static __declspec(naked) void wrap_stub(void)
{
    __asm {
        mov cx, word ptr [edx]
        push eax
        push ecx
        push edx
        push edx
        call is_break
        add esp, 4
        test eax, eax
        pop edx
        pop ecx
        pop eax
        jz no
        jmp dword ptr [g_yes]
    no:
        jmp dword ptr [g_no]
    }
}

void wrap_init(void)
{
    static const uint8_t sig[] = { 0x66, 0x8b, 0x0a, 0x66, 0x83, 0xf9, 0x20, 0x74, 0x20, 0x66, 0x83, 0xf9, 0x0a, 0x74, 0x1a,
                                   0x8b, 0x4c, 0x24, 0x14, 0x0f, 0xb7, 0x49, 0x10, 0x03, 0xd9, 0x3b, 0x5f, 0x2c };
    static const uint8_t nops[10] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    uint32_t at = find_code(0x44a1cd, sig, "xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    if (!at) { dlog("wrap: unexpected code"); return; }
    g_yes = at + 0x29;       /* 0x44a1f6: 区切りの字 */
    g_no = at + 0x0f;        /* 0x44a1dc: ふつうの字 */
    patch_jmp(at, (void *)wrap_stub);
    patch_bytes(at + 5, nops, 10);
}
