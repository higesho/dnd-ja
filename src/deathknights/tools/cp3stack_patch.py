# 一度きりの書き換え: 型 3 のコピープロテクトで、答えの番地が 0 のときは
# 出題関数の局所変数 [bp-105]（Pascal 文字列）を BP の鎖から探す（Death Knights of Krynn）
import sys
root = sys.argv[1]
p = root + '/src/pr_jp.c'
s = open(p, encoding='utf-8').read()
old = """        uint32_t aa = PHYS_DS + gp_cp3[0];
        int al = rb(aa), k;"""
new = """        uint32_t aa = PHYS_DS + gp_cp3[0];
        int al, k;
        if (!gp_cp3[0]) {                /* 答えはスタック上: 出題関数の [bp-105] */
            uint16_t f = R_BP;
            int lv;
            aa = 0;
            for (lv = 0; lv < 8 && f; lv++) {
                uint8_t len;
                f = rw(PHYS_SS + f);
                if (!f) break;
                len = rb(PHYS_SS + f - 0x105);
                if (len >= 1 && len <= 20) {
                    int ok = 1;
                    for (k = 0; k < len; k++) {
                        uint8_t c = rb(PHYS_SS + f - 0x105 + 1 + k);
                        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '\\'')) { ok = 0; break; }
                    }
                    if (ok) { aa = PHYS_SS + f - 0x105; break; }
                }
            }
        }
        al = aa ? rb(aa) : 0;"""
assert old in s
s = s.replace(old, new, 1)
open(p, 'w', encoding='utf-8').write(s)
print('ok')
