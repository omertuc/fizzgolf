#include <inttypes.h>
#include <limits.h>
#include <stdalign.h>
#include <unistd.h>
#include <tmmintrin.h>

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

__attribute__((noinline))
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
    b[0] = 'F';
    b[1] = 'i';
    b[2] = 'z';
    b[3] = 'z';
    b[4] = '\n';
    return 5;
}

static int writeBuzz(char *b) {
    b[0] = 'B';
    b[1] = 'u';
    b[2] = 'z';
    b[3] = 'z';
    b[4] = '\n';
    return 5;
}

static int writeFizzBuzz(char *b) {
    b[0] = 'F';
    b[1] = 'i';
    b[2] = 'z';
    b[3] = 'z';
    b[4] = 'B';
    b[5] = 'u';
    b[6] = 'z';
    b[7] = 'z';
    b[8] = '\n';
    return 9;
}

static __m128i fat(__m128i d) {
    return _mm_or_si128(d, _mm_and_si128(_mm_set1_epi64x(Z),
           _mm_cmpeq_epi8(d, _mm_setzero_si128())));
}

#define I d = inc(d)
#define D I; p += writeDigits(p, d, i)
#define F I; p += writeFizz(p)
#define B_ p += writeBuzz(p)
#define U_ p += writeFizzBuzz(p)
#define B I; B_
#define U I; U_

int main() {
    __m128i d = _mm_set1_epi64x(Z);
    int i = sizeof(d) - 1;
    alignas(0x1000) char b[0x10000];
    char *p = b;
    long long j = 10, k = 10;
    for (; j < 100000000; j += 30) {
        D; D; F; D; B; F; D; D; F; B;
        if (j == k) {
            k *= 10;
            d = _mm_or_si128(d,
                _mm_set_epi64x(0, Z >> (i - sizeof(uint64_t)) * CHAR_BIT));
            --i;
        } else {
            d = fat(d);
        }
        D; F; D; D; U; D; D; F; D; B;
        d = fat(d);
        F; D; D; F; B; D; F; D; D; U;
        d = fat(d);
        if (p - b + (9 * 16 + 5 * 12 + 9 * 2) > sizeof(b)) {
            write(1, b, p - b);
            p = b;
        }
    }
    for (; j < 10000000000000000; j += 30) {
        D; D; F; D; B; F; D; D; F; B_;
        if (j == k) {
            d = inc_carry(d);
            k *= 10;
            d = _mm_or_si128(d, _mm_set_epi64x(Z >> (i - 1) * CHAR_BIT, Z));
            --i;
        } else {
            if (!(j % 100000000)) {
                d = inc_carry(d);
            } else {
                d = inc(d);
            }
            d = fat(d);
        }
        D; F; D; D; U; D; D; F; D; B_;
        if (!((j + 10) % 100000000)) {
            d = inc_carry(d);
        } else {
            d = inc(d);
        }
        d = fat(d);
        F; D; D; F; B; D; F; D; D; U_;
        if (!((j + 20) % 100000000)) {
            d = inc_carry(d);
        } else {
            d = inc(d);
        }
        d = fat(d);
        if (p - b + (17 * 16 + 5 * 12 + 9 * 2) > sizeof(b)) {
            write(1, b, p - b);
            p = b;
        }
    }
    return 0;
}
