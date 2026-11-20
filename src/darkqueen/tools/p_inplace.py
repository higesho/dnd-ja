# 一度きりの書き換え: 訳文を VRAM ではなく元の文字列の場所に書き、次の呼び出しで元に戻す
p = 'src/dq_jp.c'
s = open(p, encoding='utf-8').read()
def rep(a, b):
    global s
    assert a in s, a[:60]
    s = s.replace(a, b, 1)
rep('''#define BUF_FMT  0xafa00u      /* 書式文字列用 512 バイト */
#define BUF_WRAP 0xafc00u      /* イベント文用 1024 バイト */''', '''/* 訳文は元の文字列の場所に書く（日本語は 1 字 1 バイトなので、ほとんどの場合は英文より短い）。
 * 元の英文は控えておき、同じフックが次に呼ばれたときに書き戻す（定数の文字列を何度でも訳し直せるように） */
typedef struct { uint32_t addr; int n; uint8_t save[1100]; } restore_t;
static restore_t g_rs_fmt, g_rs_wrap;
static void restore(restore_t *r)
{
    int k;
    if (!r->n) return;
    for (k = 0; k < r->n; k++) wb(r->addr + k, r->save[k]);
    r->n = 0;
}
static int put_inplace(restore_t *r, uint32_t addr, const uint8_t *en, int n, const uint8_t *gs, int m)
{
    int k;
    if (m > n + 1) return 0;                /* 入らない: 英語のまま */
    memcpy(r->save, en, n); r->save[n] = 0;
    r->addr = addr; r->n = n + 1;
    for (k = 0; k < m; k++) wb(addr + k, gs[k]);
    return 1;
}''')
rep('''    static uint8_t en[520], gs[520];
    int n = read_cstr(src, en, 500);
    const tentry *e = lookup(en, n);''', '''    static uint8_t en[520], gs[520];
    int n;
    const tentry *e;
    restore(&g_rs_fmt);
    n = read_cstr(src, en, 500);
    e = lookup(en, n);''')
rep('''        for (k = 0; k < m; k++) wb(BUF_FMT + k, gs[k]);
        ww(ss + sp + 4, 0xfa00);
        ww(ss + sp + 6, 0xa000);''', '''        (void)k;
        if (!put_inplace(&g_rs_fmt, src, en, n, gs, m)) log_miss("F>", en, n);''')
rep('''    static uint8_t en[1100], gs[1030];
    int n = read_cstr(src, en, 1090);
    const tentry *e = lookup(en, n);''', '''    static uint8_t en[1100], gs[1030];
    int n;
    const tentry *e;
    restore(&g_rs_wrap);
    n = read_cstr(src, en, 1090);
    e = lookup(en, n);''')
rep('''        int m = to_game(e->ja, e->jalen, gs, 1020), k;
        for (k = 0; k < m; k++) wb(BUF_WRAP + k, gs[k]);
        ww(ss + sp + 0x12, 0xfc00);
        ww(ss + sp + 0x14, 0xa000);''', '''        int m = to_game(e->ja, e->jalen, gs, 1020);
        if (!put_inplace(&g_rs_wrap, src, en, n, gs, m)) log_miss("W>", en, n);''')
open(p, 'w', encoding='utf-8').write(s)
print('ok')
