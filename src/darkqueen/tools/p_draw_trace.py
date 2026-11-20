# 調査用: 1 字描画の関数の入口にもフックを入れ、呼び出し元ごとに文字を記録する
p = 'src/dq_jp.c'
s = open(p, encoding='utf-8').read()
add = r'''
/* ---- 1 字描画の入口（push bp / mov bp,sp / sub sp,4e の代わり） */
static uint32_t g_draw, g_last_draw;
static char g_dl[512];
static int g_dn;
static void flush_draw(void)
{
    if (g_dn) { g_dl[g_dn] = 0; dbg("D%05x %s\n", g_last_draw, g_dl); g_dn = 0; }
}
static uint32_t __cdecl cb_draw(void)
{
    uint32_t ss = PHYS_SS;
    uint16_t sp = R_SP;
    uint32_t ret = ((uint32_t)rw(ss + sp + 2) << 4) + rw(ss + sp);
    uint8_t ch = rb(ss + sp + 8);
    if (ret != g_last_draw) { flush_draw(); g_last_draw = ret; }
    if (g_dn < 500) g_dl[g_dn++] = (char)(ch >= 0x20 && ch < 0x7f ? ch : '?');
    if (g_dn >= 120) flush_draw();
    push16(R_BP);
    SET_BP(R_SP);
    SET_SP(R_SP - 0x4e);
    return 0;
}
'''
s = s.replace("static int g_cb_installed;", add + "\nstatic int g_cb_installed;", 1)
s = s.replace("    tab[b + 1] = (uint32_t)cb_fmt;", "    tab[b + 1] = (uint32_t)cb_fmt;\n    tab[b + 2] = (uint32_t)cb_draw;", 1)
s = s.replace("static const uint8_t SIG_PUTCH[]", "static const uint8_t SIG_DRAW[] = {0x55,0x8b,0xec,0x83,0xec,0x4e,0x56,0x57,0x16,0x8d,0x46,0x08,0x50,0x16,0x8d,0x46};\nstatic const uint8_t SIG_PUTCH[]", 1)
old = '        dbg("patched fmt %x\\n", p);\n    }'
assert old in s
s = s.replace(old, old + '''
    p = find(mb, SIG_DRAW, sizeof SIG_DRAW);
    if (p) {
        g_draw = p;
        wb(p, 0xfe); wb(p + 1, 0x38); wb(p + 2, (uint8_t)(g_cb_base + 2)); wb(p + 3, (uint8_t)((g_cb_base + 2) >> 8));
        wb(p + 4, 0x90); wb(p + 5, 0x90);
        dbg("patched draw %x\\n", p);
    }''', 1)
s = s.replace("try_patch(); flush_line();", "try_patch(); flush_line(); flush_draw();", 1)
open(p, 'w', encoding='utf-8').write(s)
print('ok')
