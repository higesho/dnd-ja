# 一度きりの書き換え: コピープロテクト（冒険日誌の単語）の答えを入力欄の右に出す
p = 'src/dq_jp.c'
s = open(p, encoding='utf-8').read()
def rep(a, b):
    global s
    assert a in s, a[:60]
    s = s.replace(a, b, 1)
rep('''static int put_inplace(restore_t *r, uint32_t addr, const uint8_t *en, int n, const uint8_t *gs, int m)
{
    int k;
    if (m > n + 1) return 0;                /* 入らない: 英語のまま */
    memcpy(r->save, en, n); r->save[n] = 0;
    r->addr = addr; r->n = n + 1;''', '''static int put_inplace(restore_t *r, uint32_t addr, const uint8_t *en, int n, const uint8_t *gs, int m, int over)
{
    int k;
    if (m > n + 1 && !over) return 0;       /* 入らない: 英語のまま */
    if (m > n + 1) { for (k = 0; k < m && k < 1000; k++) r->save[k] = rb(addr + k); r->n = m; }
    else { memcpy(r->save, en, n); r->save[n] = 0; r->n = n + 1; }
    r->addr = addr;''')
rep('''if (!put_inplace(&g_rs_fmt, src, en, n, gs, m)) log_miss("F>", en, n);''',
    '''if (!put_inplace(&g_rs_fmt, src, en, n, gs, m, 0)) log_miss("F>", en, n);''')
rep('''if (!put_inplace(&g_rs_wrap, src, en, n, gs, m)) log_miss("W>", en, n);''',
    '''if (!put_inplace(&g_rs_wrap, src, en, n, gs, m, 0)) log_miss("W>", en, n);''')
# 答え: 「Enter 」の表示（出題中の印 DS:bec0=1、入力済みの印 DS:bec1=0）
rep('''    n = read_cstr(src, en, 500);
    e = lookup(en, n);''', '''    n = read_cstr(src, en, 500);
    e = lookup(en, n);
    if (n == 6 && !memcmp(en, "Enter ", 6) && rb(PHYS_DS + 0xbec0) == 1 && rb(PHYS_DS + 0xbec1) == 0) {
        uint8_t ans[24], q[80];
        int al = read_cstr(PHYS_DS + 0xc6f9, ans, 20), k, ok = al > 0, qn = 0, m;
        for (k = 0; k < al; k++) if (!(ans[k] >= 'A' && ans[k] <= 'Z') && !(ans[k] >= 'a' && ans[k] <= 'z') && ans[k] != '-' && ans[k] != '\\'') ok = 0;
        if (ok) {
            const tentry *a = lookup((const uint8_t*)"@ANSWER", 7);
            memcpy(q, "Enter               ", 20); qn = 20;      /* 入力欄（14 字）を空けて右に出す */
            if (a) {
                for (k = 0; k < a->jalen && qn < 60; k++) {
                    if (a->ja[k] == '%' && k + 1 < a->jalen && a->ja[k + 1] == 's') { memcpy(q + qn, ans, al); qn += al; k++; }
                    else q[qn++] = a->ja[k];
                }
            } else { q[qn++] = '('; memcpy(q + qn, ans, al); qn += al; q[qn++] = ')'; }
            m = to_game(q, qn, gs, 80);
            put_inplace(&g_rs_fmt, src, en, n, gs, m, 1);
            dbg("copy protection answer %.*s\\n", al, ans);
            push16(R_BP); SET_BP(R_SP); SET_SP(R_SP - 0x1c);
            return 0;
        }
    }''')
open(p, 'w', encoding='utf-8').write(s)
print('ok')
