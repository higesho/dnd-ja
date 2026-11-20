/*
 * UA のデザインの文章に日本語を入れるための符号（保存形式）。tools/uajp.py と同じ決まり。
 *
 * UA の文章は 6 ビット（64 種類の記号）で保存される。日本語 1 字を、英文でほとんど使われない記号で始まる 2〜3 記号で表す。
 *   2 記号: ] < > = のどれか + 1 字   → 204 通り（ひらがな・カタカナ・主な約物）
 *   3 記号: [ # $ のどれか + 2 字     → 漢字（JIS 第 1・第 2 水準）とそのほかの記号
 * 後ろの字に使うのは 51 種類: ! " & ' ( ) * + , - . / 0-9 : ; ? A-Z
 * DLL の中の符号は (0x80 + 区, 0x80 + 点) の 2 バイト。
 */
#ifndef UAJP_H
#define UAJP_H
#include <stdint.h>

#define UAJP_NT 51
#define UAJP_NK 6486
static const char UAJP_T[] = "!\"&'()*+,-./0123456789:;?ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char UAJP_LEAD2[] = "]<>=";
static const char UAJP_LEAD3[] = "[#$";
static const uint8_t UAJP_P1[35] = {2, 3, 6, 9, 10, 28, 33, 36, 54, 55, 56, 57, 42, 43, 4, 5, 7, 8, 25, 29, 37, 38, 39, 40, 41,
                                    58, 59, 26, 27, 11, 12, 19, 20, 21, 22};

static int uajp_tval(uint8_t c)
{
    int i;
    if (c >= 'a' && c <= 'z') c -= 0x20;
    for (i = 0; i < UAJP_NT; i++) if ((uint8_t)UAJP_T[i] == c) return i;
    return -1;
}
static int uajp_lead2(uint8_t c) { int i; for (i = 0; i < 4; i++) if ((uint8_t)UAJP_LEAD2[i] == c) return i; return -1; }
static int uajp_lead3(uint8_t c) { int i; for (i = 0; i < 3; i++) if ((uint8_t)UAJP_LEAD3[i] == c) return i; return -1; }

/* 番号 → 内部符号（区点）。なければ 0 */
static uint16_t uajp_plane2(int i)
{
    int ku, ten;
    if (i < 83) { ku = 4; ten = i + 1; }
    else if (i < 169) { ku = 5; ten = i - 83 + 1; }
    else if (i < 204) { ku = 1; ten = UAJP_P1[i - 169]; }
    else return 0;
    return (uint16_t)(((0x80 + ku) << 8) | (0x80 + ten));
}
static uint16_t uajp_plane3(int i)
{
    int ku, ten;
    if (i < UAJP_NK) { ku = 16 + i / 94; ten = i % 94 + 1; }
    else { i -= UAJP_NK; if (i >= 8 * 94) return 0; ku = 1 + i / 94; ten = i % 94 + 1; }
    return (uint16_t)(((0x80 + ku) << 8) | (0x80 + ten));
}

/* 区点 → 記号の並び（out に 2〜3 字、長さを返す。入れられなければ 0） */
static int uajp_encode_kuten(int ku, int ten, char *out)
{
    int i = -1, k;
    if (ku == 4 && ten <= 83) i = ten - 1;
    else if (ku == 5 && ten <= 86) i = 83 + ten - 1;
    else if (ku == 1) { for (k = 0; k < 35; k++) if (UAJP_P1[k] == ten) i = 169 + k; }
    if (i >= 0) { out[0] = UAJP_LEAD2[i / UAJP_NT]; out[1] = UAJP_T[i % UAJP_NT]; return 2; }
    if (ku >= 16 && ku <= 84) i = (ku - 16) * 94 + ten - 1;
    else if (ku >= 1 && ku <= 8) i = UAJP_NK + (ku - 1) * 94 + ten - 1;
    else return 0;
    out[0] = UAJP_LEAD3[i / (UAJP_NT * UAJP_NT)]; out[1] = UAJP_T[i / UAJP_NT % UAJP_NT]; out[2] = UAJP_T[i % UAJP_NT];
    return 3;
}
#endif
