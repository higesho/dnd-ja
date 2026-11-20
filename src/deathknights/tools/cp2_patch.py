# 一度きりの書き換え: Curse 型のコピープロテクト（1 文字の答え）を表示する（gp_cp == 2）
p = 'src/pr_jp.c'
s = open(p, encoding='utf-8').read()
old = "/* --- 1 行表示 709:0626 の"
new = r'''/* Curse 型: 「type character and press return: 」を出すとき、出題の関数のフレームの [bp-3] にある答えの字を付ける。
 * Print の bp から呼び出し元のフレームを数段たどり、[bp-14]（道筋 0..2）・[bp-15]（升目 0..5）がもっともらしいものを使う */
static int cp_answer2(const uint8_t *s, int n, sbuf *out)
{
    static const char q[] = "type character and press return: ";
    uint32_t ss = PHYS_SS;
    uint16_t f = R_BP;
    int lv;
    const tentry *e;
    if (n != (int)sizeof q - 1 || memcmp(s, q, n)) return 0;
    for (lv = 0; lv < 5; lv++) {
        uint8_t ans, path, box;
        f = rw(ss + f);
        if (!f) break;
        ans = rb(ss + f - 3); path = rb(ss + f - 0x14); box = rb(ss + f - 0x15);
        if (path <= 2 && box <= 5 && ((ans >= 'A' && ans <= 'Z') || (ans >= '0' && ans <= '9'))) {
            span sp;
            sbuf t = {0};
            int fl;
            const sbuf *tr = tr_cached(s, n, &fl);
            if (tr) sb_put(out, tr->p, tr->n); else sb_put(out, s, n);
            sp.p = &ans; sp.n = 1;
            e = tr_exact((const uint8_t*)"@ANSWER", 7);
            if (e) render(&t, e->ja, e->jalen, &sp, 1);
            else { sb_put(&t, (const uint8_t*)"(", 1); sb_c(&t, ans); sb_c(&t, ')'); }
            sb_put(out, t.p, t.n);
            free(t.p);
            dbg("copy protection answer %c (level %d)\n", ans, lv);
            return 1;
        }
    }
    return 0;
}

/* --- 1 行表示 709:0626 の'''
assert old in s
s = s.replace(old, new, 1)
old = "    if (gp_cp && n >= 9 && n < 20) {"
new = '''    if (gp_cp == 2 && n > 20) {
        sbuf q = {0};
        if (cp_answer2(buf, n, &q)) {
            int m = to_game(q.p, q.n, out, NULL);
            free(q.p);
            write_pstr(a, out, m);
            SET_AL(m);
            return 0;
        }
        free(q.p);
    }
    if (gp_cp == 1 && n >= 9 && n < 20) {'''
assert old in s
s = s.replace(old, new, 1)
s = s.replace('dbg("Pool of Radiance JP patch active, blob %08X\\n", g_blob_crc);', 'dbg("Gold Box JP patch active (%s), blob %08X\\n", gp_dirname, g_blob_crc);')
open(p, 'w', encoding='utf-8').write(s)
print('ok')
