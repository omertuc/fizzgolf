#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <tmmintrin.h>

#define ALIGN 0x1000
#define SIZE (ALIGN << 8)
#define Z 0xf6f6f6f6f6f6f6f6u
static const union {uint64_t _[2]; __m128i m;} S[] = {
    {{0x0706050403020100, 0x0f0e0d0c0b0a0908}},
    {{0x0807060504030201, 0xff0f0e0d0c0b0a09}},
    {{0x0908070605040302, 0xffff0f0e0d0c0b0a}},
    {{0x0a09080706050403, 0xffffff0f0e0d0c0b}},
    {{0x0b0a090807060504, 0xffffffff0f0e0d0c}},
    {{0x0c0b0a0908070605, 0xffffffffff0f0e0d}},
    {{0x0d0c0b0a09080706, 0xffffffffffff0f0e}},
    {{0x0e0d0c0b0a090807, 0xffffffffffffff0f}},
    {{0x0f0e0d0c0b0a0908, 0xffffffffffffffff}},
    {{0xff0f0e0d0c0b0a09, 0xffffffffffffffff}},
    {{0xffff0f0e0d0c0b0a, 0xffffffffffffffff}},
    {{0xffffff0f0e0d0c0b, 0xffffffffffffffff}},
    {{0xffffffff0f0e0d0c, 0xffffffffffffffff}},
    {{0xffffffffff0f0e0d, 0xffffffffffffffff}},
    {{0xffffffffffff0f0e, 0xffffffffffffffff}},
    {{0xffffffffffffff0f, 0xffffffffffffffff}}
};

static __m128i inc(__m128i d) {
    return _mm_add_epi64(d, _mm_set_epi64x(0, 1));
}

static __m128i inc_carry(__m128i d) {
    return _mm_add_epi64(d, _mm_set_epi64x(1, 1));
}

static int writeDigits(char *b, __m128i d, int i) {
    d = _mm_sub_epi64(d, _mm_set1_epi64x(0xc6c6c6c6c6c6c6c6));
    d = _mm_shuffle_epi8(d, _mm_set_epi8(0, 1,  2,  3,  4,  5,  6,  7,
                                         8, 9, 10, 11, 12, 13, 14, 15));
    d = _mm_shuffle_epi8(d, S[i].m);
    _mm_storeu_si128((__m128i *)b, d);
    b[sizeof(d) - i] = '\n';
    return sizeof(d) - i + 1;
}

static int writeFizz(char *b) {
    uint64_t fizz = 0x0a7a7a6946;
    memcpy(b, &fizz, sizeof(fizz));
    return 5;
}

static int writeFizzBuzz(char *b) {
    uint64_t fizzbuzz = 0x7a7a75427a7a6946;
    memcpy(b, &fizzbuzz, sizeof(fizzbuzz));
    b[8] = 0x0a;
    return 9;
}

static int writeFizzAndBuzz(char *b) {
    uint64_t fizznbuz = 0x7a75420a7a7a6946;
    memcpy(b, &fizznbuz, sizeof(fizznbuz));
    uint16_t zn = 0x0a7a;
    memcpy(b + 8, &zn, sizeof(zn));
    return 10;
}

static int writeBuzzAndFizz(char *b) {
    uint64_t buzznfiz = 0x7a69460a7a7a7542;
    memcpy(b, &buzznfiz, sizeof(buzznfiz));
    uint16_t zn = 0x0a7a;
    memcpy(b + 8, &zn, sizeof(zn));
    return 10;
}

static __m128i fat(__m128i d) {
    return _mm_or_si128(d, _mm_and_si128(_mm_set1_epi64x(Z),
                        _mm_cmpeq_epi8(d, _mm_setzero_si128())));
}

static void memcpy_fast(char *a, char *b, unsigned n) {
    for (int i = 0; i < n; i += sizeof(__m128i)) {
        __m128i r = _mm_load_si128((__m128i *)(b + i));
        _mm_store_si128((__m128i *)(a + i), r);
    }
}

#define I d = inc(d)
#define IC d = inc_carry(d)
#define D I; p += writeDigits(p, d, i)
#define F I; p += writeFizz(p)
#define U I; p += writeFizzBuzz(p)
#define FNB I; I; p += writeFizzAndBuzz(p)
#define BNF I; I; p += writeBuzzAndFizz(p)
#define FIRST  D; D; F; D; BNF;  D; D; I; p += writeFizzAndBuzz(p)
#define SECOND D; F; D; D; U; D; D; F; D; p += writeBuzzAndFizz(p)
#define THIRD  I; D; D; FNB;  D; F; D; D; p += writeFizzBuzz(p)
#define CARRY(n) if (!((j + (n)) % 100000000)) IC; else I; d = fat(d)

int main() {
    if (fcntl(1, F_SETPIPE_SZ, SIZE) != SIZE) abort();
    _Alignas(ALIGN) char b[2][SIZE + ALIGN];
    int f = 0;
    char *p = b[f];
    __m128i d = _mm_set1_epi64x(Z);
    int i = sizeof(d) - 1;
    long long j = 10, k = 10;
    for (; j < 10000000000000000; j += 30) {
        FIRST; CARRY(0);
        if (j == k) {
            k *= 10;
            --i;
        }
        SECOND; CARRY(10);
        THIRD; CARRY(20);
        int n = p - b[f] - SIZE;
        if (n >= 0) {
            struct iovec v = {b[f], SIZE};
            do {
                int n = vmsplice(1, &v, 1, 0);
                if (n < 0) abort();
                v.iov_base = (char *)v.iov_base + n;
                v.iov_len -= n;
            } while (v.iov_len);
            f = !f;
            memcpy_fast(b[f], b[!f] + SIZE, n);
            p = b[f] + n;
        }
    }
    return 0;
}
