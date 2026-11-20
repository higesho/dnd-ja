# 一度きりの書き換え: コピープロテクトの型 4（Krynn: 「日誌の○番の△語目」）
# 出題関数の局所変数 [bp-105] に答えの単語（Pascal 文字列）が入っている。
# 3 行目「 (journal, page N)?」を表示するとき、BP の鎖をたどってその単語を見つけ、行の後ろに付ける
import sys
root = sys.argv[1]
p = root + '/src/pr_jp.c'
s = open(p, encoding='utf-8').read()
code = r'''
static int cp_answer4(const uint8_t *s, int n, sbuf *out)
{
    static const char key[] = "(journal, page";
    uint32_t ss = PHYS_SS;
    uint16_t f = R_BP;
    int lv, k, hit = 0;
    for (k = 0; k + (int)sizeof key - 1 <= n; k++)
        if (!memcmp(s + k, key, sizeof key - 1)) { hit = 1; break; }
    if (!hit) return 0;
    for (lv = 0; lv < 6; lv++) {
        uint8_t len;
        f = rw(ss + f);
        if (!f) break;
        len = rb(ss + f - 0x105);
        if (len >= 1 && len <= 20) {
            uint8_t w[24];
            int ok = 1;
            for (k = 0; k < len; k++) {
                w[k] = rb(ss + f - 0x105 + 1 + k);
                if (!((w[k] >= 'A' && w[k] <= 'Z') || (w[k] >= 'a' && w[k] <= 'z') || w[k] == '-' || w[k] == '\'')) { ok = 0; break; }
            }
            if (ok) {
                span sp;
                const tentry *e;
                int fl, m = n;
                const sbuf *tr;
                while (m > 0 && s[m - 1] == ' ') m--;          /* 行末の空白（英語の桁そろえ）を除いて訳す */
                tr = tr_cached(s, m, &fl);
                if (tr) sb_put(out, tr->p, tr->n); else sb_put(out, s, m);
                sp.p = w; sp.n = len;
                e = tr_exact((const uint8_t*)"@ANSWER", 7);
                if (e) render(out, e->ja, e->jalen, &sp, 1);
                else { sb_put(out, (const uint8_t*)" (", 2); sb_put(out, w, len); sb_c(out, ')'); }
                dbg("copy protection answer %.*s (level %d)\n", len, w, lv);
                return 1;
            }
        }
    }
    return 0;
}

'''
anchor = '/* --- 1 行表示 709:0626 の、文字列を局所変数 [bp-100] に写した直後'
assert anchor in s and 'cp_answer4' not in s
s = s.replace(anchor, code + anchor, 1)
old = "    if (gp_cp == 2 && n > 20) {"
new = """    if (gp_cp == 4 && n > 10) {
        sbuf q = {0};
        if (cp_answer4(buf, n, &q)) {
            int m = to_game(q.p, q.n, out, NULL);
            free(q.p);
            write_pstr(a, out, m);
            SET_AL(m);
            return 0;
        }
        free(q.p);
    }
    if (gp_cp == 2 && n > 20) {"""
assert old in s
s = s.replace(old, new, 1)
open(p, 'w', encoding='utf-8').write(s)
print('ok')
