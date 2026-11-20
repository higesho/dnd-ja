/* ------------------------------------------------------------ 翻訳表 */
/* 'T' 節: u32 個数, 項目...
 *   u8 種類(0=完全一致, 1=書式), u8 旗(1=中央寄せし直す), u16 訳長, u16 英語長, u64 英語のハッシュ
 *   種類 1 はさらに u16 部品数, 部品 {u8 型(0=固定,1=指定); 固定: u16 長さ, u64 ハッシュ / 指定: u8 変換, u8 幅, u8 左寄せ}
 *   続いて訳。訳の中の "%1".."%9" は引数（英語側の書式指定の順番）, "%%" は % そのもの。
 * 英語の文そのものは持たない（原作の文章を配布物に入れないため、ハッシュで照合する）。 */
typedef struct { uint8_t type, conv, width, left; uint16_t len; uint64_t h; } ptok;
typedef struct { uint64_t h; const uint8_t *ja; uint16_t enlen, jalen; uint8_t flags; ptok *tok; int ntok; } tentry;
static tentry *g_tr;
static uint32_t g_ntr;
static int32_t *g_hash;          /* 開番地法: 項目番号+1 */
static uint32_t g_hmask;
static int *g_pat;               /* 書式（%）を含む項目 */
static int g_npat;

static uint32_t fnv(const uint8_t *p, int n)
{
    uint32_t h = 2166136261u;
    while (n--) h = (h ^ *p++) * 16777619u;
    return h;
}
static uint64_t fnv64(const uint8_t *p, int n)
{
    uint64_t h = 14695981039346656037ull;
    while (n--) h = (h ^ *p++) * 1099511628211ull;
    return h;
}

static void load_table(const uint8_t *d)
{
    uint32_t n = *(const uint32_t*)d, i, sz = 1;
    const uint8_t *p = d + 4;
    g_tr = (tentry*)calloc(n ? n : 1, sizeof(tentry));
    while (sz < n * 2) sz <<= 1;
    g_hash = (int32_t*)calloc(sz, 4);
    g_hmask = sz - 1;
    g_pat = (int*)calloc(n ? n : 1, sizeof(int));
    for (i = 0; i < n; i++) {
        tentry *e = &g_tr[i];
        uint8_t kind = p[0];
        uint32_t h;
        e->flags = p[1];
        e->jalen = *(const uint16_t*)(p + 2);
        e->enlen = *(const uint16_t*)(p + 4);
        memcpy(&e->h, p + 6, 8);
        p += 14;
        if (kind == 1) {
            int k;
            e->ntok = *(const uint16_t*)p;
            p += 2;
            e->tok = (ptok*)calloc(e->ntok ? e->ntok : 1, sizeof(ptok));
            for (k = 0; k < e->ntok; k++) {
                ptok *t = &e->tok[k];
                t->type = *p++;
                if (t->type == 0) { t->len = *(const uint16_t*)p; memcpy(&t->h, p + 2, 8); p += 10; }
                else { t->conv = p[0]; t->width = p[1]; t->left = p[2]; p += 3; }
            }
            g_pat[g_npat++] = (int)i;
        }
        e->ja = p;
        p += e->jalen;
        h = (uint32_t)e->h & g_hmask;
        while (g_hash[h]) h = (h + 1) & g_hmask;
        g_hash[h] = (int32_t)i + 1;
    }
    g_ntr = n;
    {   /* 具体的な（長い）書式から先に試す */
        int a, b2;
        for (a = 1; a < g_npat; a++) {
            int v = g_pat[a], lv = g_tr[v].enlen;
            for (b2 = a; b2 > 0; b2--) {
                int u = g_pat[b2 - 1];
                if ((int)g_tr[u].enlen >= lv) break;
                g_pat[b2] = u;
            }
            g_pat[b2] = v;
        }
    }
}

static const tentry *tr_exact(const uint8_t *s, int n)
{
    uint64_t hh;
    uint32_t h;
    if (!g_hash) return NULL;
    hh = fnv64(s, n);
    h = (uint32_t)hh & g_hmask;
    while (g_hash[h]) {
        const tentry *e = &g_tr[g_hash[h] - 1];
        if (e->h == hh && e->enlen == n) return e;
        h = (h + 1) & g_hmask;
    }
    return NULL;
}

/* 書式の k 番目の指定の幅（%-18s なら 18, 左寄せ=1） */
static int spec_width(const tentry *e, int k, int *left)
{
    int i, idx = 0;
    for (i = 0; i < e->ntok; i++) {
        if (e->tok[i].type != 1) continue;
        if (idx++ == k) { *left = e->tok[i].left; return e->tok[i].width; }
    }
    *left = 0;
    return 0;
}

/* 可変長の文字列 */
typedef struct { uint8_t *p; int n, cap; } sbuf;
static void sb_put(sbuf *b, const uint8_t *s, int n)
{
    if (b->n + n + 1 > b->cap) {
        int c = b->cap ? b->cap : 256;
        while (c < b->n + n + 1) c *= 2;
        b->p = (uint8_t*)realloc(b->p, c);
        b->cap = c;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}
static void sb_c(sbuf *b, uint8_t c) { sb_put(b, &c, 1); }

#define MAXARG 9
typedef struct { const uint8_t *p; int n; } span;

/* 訳の雛形に引数をはめる */
static void render(sbuf *out, const uint8_t *t, int tn, const span *a, int na)
{
    int i;
    for (i = 0; i < tn; i++) {
        if (t[i] == '%' && i + 1 < tn) {
            if (t[i + 1] == '%') { sb_c(out, '%'); i++; continue; }
            if (t[i + 1] >= '1' && t[i + 1] <= '9') {
                int k = t[i + 1] - '1';
                if (k < na) sb_put(out, a[k].p, a[k].n);
                i++;
                continue;
            }
        }
        sb_c(out, t[i]);
    }
}

/* 書式の部品と、組み立て済みの英文を突き合わせて引数を取り出す（%s は最短一致から） */
static int pmatch(const ptok *t, int nt, const uint8_t *s, int sn, span *a, int na)
{
    int j;
    if (nt == 0) return sn == 0 ? na + 1 : 0;     /* 引数の個数 + 1 */
    if (t->type == 0) {
        if (sn < t->len || fnv64(s, t->len) != t->h) return 0;
        return pmatch(t + 1, nt - 1, s + t->len, sn - t->len, a, na);
    }
    if (na >= MAXARG) return 0;
    if (t->conv == 's' || t->conv == 'c') {
        int lo = t->conv == 'c' ? 1 : t->width, hi = t->conv == 'c' ? 1 : sn;
        for (j = lo; j <= hi && j <= sn; j++) {
            int r;
            a[na].p = s; a[na].n = j;
            r = pmatch(t + 1, nt - 1, s + j, sn - j, a, na + 1);
            if (r) return r;
        }
        return 0;
    }
    if (strchr("duxi", t->conv)) {
        j = 0;
        while (j < sn && s[j] == ' ') j++;
        if (j < sn && (s[j] == '-' || s[j] == '+')) j++;
        if (j >= sn || s[j] < '0' || s[j] > '9') return 0;
        while (j < sn && ((s[j] >= '0' && s[j] <= '9') || (t->conv == 'x' && strchr("abcdefABCDEF", s[j])))) j++;
        while (j < sn && j < t->width && s[j] == ' ') j++;   /* %-3d の右側の空白 */
        a[na].p = s; a[na].n = j;
        return pmatch(t + 1, nt - 1, s + j, sn - j, a, na + 1);
    }
    return 0;
}

/* 幅指定どおりに空白で詰め直す（バイト数で数える。日本語 1 字 = 2 バイト = 半角 2 字分） */
static void repad(sbuf *b, int w, int left)
{
    int a = 0, z = b->n, pad;
    uint8_t *t;
    if (w <= 0) return;
    while (a < z && b->p[a] == ' ') a++;
    while (z > a && b->p[z - 1] == ' ') z--;
    pad = w - (z - a);
    if (pad < 0) pad = 0;
    t = (uint8_t*)malloc(w + (z - a) + 1);
    if (left) { memcpy(t, b->p + a, z - a); memset(t + (z - a), ' ', pad); }
    else { memset(t, ' ', pad); memcpy(t + pad, b->p + a, z - a); }
    free(b->p);
    b->p = t; b->n = (z - a) + pad; b->cap = b->n + 1;
    b->p[b->n] = 0;
}

static int is_space_ctl(uint8_t c) { return c == ' ' || c == 0x0d || c == 0x0a; }

/* 英文 s を訳す。訳が見つかれば out に入れて 1 */
static int tr_str(sbuf *out, const uint8_t *s, int n, int depth)
{
    const tentry *e;
    int a0 = 0, b0 = n, i;
    if (n <= 0 || depth > 3) return 0;
    for (i = 0; i < n; i++) if (s[i] >= 0x80) return 0;   /* 既に日本語 */
    e = tr_exact(s, n);
    if (e) { render(out, e->ja, e->jalen, NULL, 0); return 1; }
    /* 前後の空白・改行・色指定を外して再挑戦 */
    for (;;) {
        if (a0 < b0 && is_space_ctl(s[a0])) { a0++; continue; }
        if (a0 + 1 < b0 && (s[a0] == 0x02 || s[a0] == 0x06)) { a0 += 2; continue; }
        break;
    }
    while (b0 > a0 && is_space_ctl(s[b0 - 1])) b0--;
    if (a0 > 0 || b0 < n) {
        sbuf t = {0};
        if (b0 > a0 && tr_str(&t, s + a0, b0 - a0, depth + 1)) {
            sb_put(out, s, a0);
            sb_put(out, t.p, t.n);
            sb_put(out, s + b0, n - b0);
            free(t.p);
            return 1;
        }
        free(t.p);
        return 0;
    }
    /* 書式つきの項目と突き合わせる */
    for (i = 0; i < g_npat; i++) {
        span a[MAXARG];
        int k, na;
        e = &g_tr[g_pat[i]];
        na = pmatch(e->tok, e->ntok, s, n, a, 0);
        if (na > 0) {
            sbuf args[MAXARG];
            span ta[MAXARG];
            na--;
            memset(args, 0, sizeof args);
            for (k = 0; k < na; k++) {
                int left, w = spec_width(e, k, &left);
                if (tr_str(&args[k], a[k].p, a[k].n, depth + 1)) {
                    if (w) repad(&args[k], w, left);
                    ta[k].p = args[k].p; ta[k].n = args[k].n;
                } else ta[k] = a[k];
            }
            {
                int start = out->n;
                render(out, e->ja, e->jalen, ta, na);
                for (k = 0; k < na; k++) free(args[k].p);
                if (out->n - start == n && !memcmp(out->p + start, s, n)) {   /* 何も訳せていない */
                    out->n = start;
                    if (out->p) out->p[start] = 0;
                    continue;
                }
            }
            return 1;
        }
    }
    return 0;
}
