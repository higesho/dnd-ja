# 一度きりの書き換え: pr_jp.c の作品ごとの値を 'P' 節から読む形にする
import re
p = 'src/pr_jp.c'
s = open(p, encoding='utf-8').read()

def rep(old, new, count=1):
    global s
    assert old in s, old[:60]
    s = s.replace(old, new) if count == 0 else s.replace(old, new, count)

rep("static int g_cb_base = 0x70;", r'''static int g_cb_base = 0x70;
/* 作品ごとの値（'P' 節）。既定値は Pool of Radiance */
static uint32_t gp_font = 0x5e20;     /* フォントの far ポインタ（DS 内） */
static uint32_t gp_cursor = 0x5e24;   /* ワードラップ表示のカーソル列（DS 内） */
static uint32_t gp_wx1 = 0x14, gp_wx2 = 0x10;   /* ワードラップ表示の左端・右端の引数（bp+） */
static uint32_t gp_px = 0x0e;         /* 1 行表示の x の引数（bp+） */
static uint32_t gp_menuvar = 0x6b2c;  /* メニューのフックで置き換えた mov byte [xxxx],0 */
static uint32_t gp_cp = 1;            /* コードホイールの答えを出す */
static char gp_dirname[32] = "PoolRad_JP";''')
rep('''            g_cb_base = (int)v[0];
            g_logon = (int)v[1];''', '''            g_cb_base = (int)v[0];
            g_logon = (int)v[1];
            if (s->len >= 9 * 4 + 32) {
                gp_font = v[2]; gp_cursor = v[3]; gp_wx1 = v[4]; gp_wx2 = v[5]; gp_px = v[6]; gp_menuvar = v[7]; gp_cp = v[8];
                memcpy(gp_dirname, d + 36, 31);
            }''')
rep("rw(ds + 0x5e20) | ((uint32_t)rw(ds + 0x5e22) << 16)", "rw(ds + gp_font) | ((uint32_t)rw(ds + gp_font + 2) << 16)", 0)
rep("ww(ds + 0x5e20,", "ww(ds + gp_font,", 0)
rep("ww(ds + 0x5e22,", "ww(ds + gp_font + 2,", 0)
rep("uint32_t xa = PHYS_SS + R_BP + 0x0e;", "uint32_t xa = PHYS_SS + R_BP + gp_px;")
rep("int x1 = rb(bp + 0x14), x2 = rb(bp + 0x10), cur = rb(ds + 0x5e24);", "int x1 = rb(bp + gp_wx1), x2 = rb(bp + gp_wx2), cur = rb(ds + gp_cursor);")
rep("wb(PHYS_DS + 0x6b2c, 0);", "wb(PHYS_DS + gp_menuvar, 0);")
rep("    if (n >= 9 && n < 20) {\n", "    if (gp_cp && n >= 9 && n < 20) {\n")
rep('''    wsprintfA(g_dir, "%s\\\\PoolRad_JP", la);
    CreateDirectoryA(g_dir, NULL);
    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;''', '''    if (!verify_dosbox()) return;
    if (!load_blob(self)) return;
    wsprintfA(g_dir, "%s\\\\%s", la, gp_dirname);
    CreateDirectoryA(g_dir, NULL);''')
open(p, 'w', encoding='utf-8').write(s)
print('left:', re.findall(r'0x5e2[024]|0x6b2c', s.split('gp_dirname[32]')[1]))
