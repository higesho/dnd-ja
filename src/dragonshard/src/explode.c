/*
 * Altered version based on zlib contrib/blast by Mark Adler (zlib license).
 * Original: Copyright (C) 2003, 2012, 2013 Mark Adler
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not claim that you wrote
 *    the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be misrepresented as
 *    being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */
/* PKWARE Data Compression Library "explode" (the format of Liquid Entertainment's H2O archives).
   Written after the description in zlib's contrib/blast (Mark Adler). */
#include "dsjp.h"

#define MAXBITS 13

typedef struct { short count[MAXBITS + 1]; short symbol[256]; } Huff;

typedef struct {
    const uint8_t *in; uint32_t inlen, pos;
    uint32_t bitbuf; int bitcnt;
    int err;
} St;

static int bits(St *s, int need)
{
    uint32_t val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->pos >= s->inlen) { s->err = 1; return 0; }
        val |= (uint32_t)s->in[s->pos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = val >> need;
    s->bitcnt -= need;
    return (int)(val & ((1u << need) - 1));
}

static int decode(St *s, const Huff *h)
{
    int len = 1, code = 0, first = 0, index = 0, count, left;
    uint32_t bitbuf = s->bitbuf;
    left = s->bitcnt;
    for (;;) {
        while (left--) {
            code |= (bitbuf & 1) ^ 1;
            bitbuf >>= 1;
            count = h->count[len];
            if (code < first + count) {
                s->bitbuf = bitbuf;
                s->bitcnt = (s->bitcnt - len) & 7;
                return h->symbol[index + (code - first)];
            }
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
            len++;
        }
        left = (MAXBITS + 1) - len;
        if (left == 0) { s->err = 2; return 0; }
        if (s->pos >= s->inlen) { s->err = 1; return 0; }
        bitbuf = s->in[s->pos++];
        if (left > 8) left = 8;
    }
}

static void construct(Huff *h, const uint8_t *rep, int n)
{
    short length[256], offs[MAXBITS + 1];
    int sym = 0, len, i;
    do {
        int l = *rep & 15, cnt = (*rep >> 4) + 1;
        rep++;
        while (cnt--) length[sym++] = (short)l;
    } while (--n);
    n = sym;
    for (len = 0; len <= MAXBITS; len++) h->count[len] = 0;
    for (i = 0; i < n; i++) h->count[length[i]]++;
    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++) offs[len + 1] = offs[len] + h->count[len];
    for (i = 0; i < n; i++)
        if (length[i]) h->symbol[offs[length[i]]++] = (short)i;
}

int explode(const uint8_t *src, uint32_t srclen, uint8_t *dst, uint32_t dstlen)
{
    static const uint8_t litlen[] = {
        11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8, 9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
        7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12, 8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
        44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45, 44, 173 };
    static const uint8_t lenlen[] = { 2, 35, 36, 53, 38, 23 };
    static const uint8_t distlen[] = { 2, 20, 53, 230, 247, 151, 248 };
    static const short base[16] = { 3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264 };
    static const char extra[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    static Huff hlit, hlen, hdist;
    static int built;
    St s = { 0 };
    uint32_t out = 0;
    int lit, dict;
    if (!built) {
        construct(&hlit, litlen, sizeof litlen);
        construct(&hlen, lenlen, sizeof lenlen);
        construct(&hdist, distlen, sizeof distlen);
        built = 1;
    }
    s.in = src; s.inlen = srclen;
    lit = bits(&s, 8);
    dict = bits(&s, 8);
    if (s.err || lit > 1 || dict < 4 || dict > 6) return -1;
    for (;;) {
        if (bits(&s, 1)) {
            int sym = decode(&s, &hlen), len, dist;
            len = base[sym] + bits(&s, extra[sym]);
            if (len == 519) break;
            sym = len == 2 ? 2 : dict;
            dist = (decode(&s, &hdist) << sym) + bits(&s, sym) + 1;
            if (s.err || (uint32_t)dist > out || out + len > dstlen) return -1;
            while (len--) { dst[out] = dst[out - dist]; out++; }
        } else {
            int c = lit ? decode(&s, &hlit) : bits(&s, 8);
            if (s.err || out >= dstlen) return -1;
            dst[out++] = (uint8_t)c;
        }
        if (s.err) return -1;
    }
    return (int)out;
}
