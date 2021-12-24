#define _GNU_SOURCE
#include <stdbool.h>
#include <inttypes.h>
#include <stdalign.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <tmmintrin.h>

#define BUF_SIZE 0x100000
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
    *(uint64_t *)b = 0x0a7a7a6946;
    return 5;
}

static int writeFizzBuzz(char *b) {
    *(uint64_t *)b = 0x7a7a75427a7a6946;
    b[8] = 0x0a;
    return 9;
}

static int writeFizzAndBuzz(char *b) {
    *(uint64_t *)b = 0x7a75420a7a7a6946;
    ((uint16_t *)b)[4] = 0x0a7a;
    return 10;
}

static int writeBuzzAndFizz(char *b) {
    *(uint64_t *)b = 0x7a69460a7a7a7542;
    ((uint16_t *)b)[4] = 0x0a7a;
    return 10;
}

static __m128i fat(__m128i d) {
    return _mm_or_si128(d, _mm_and_si128(_mm_set1_epi64x(Z),
                        _mm_cmpeq_epi8(d, _mm_setzero_si128())));
}

__attribute__((always_inline)) inline
static void out(char *b, int n) {
    struct iovec iov = {b, n};
    do {
        ssize_t n = vmsplice(1, &iov, 1, 0);
        iov.iov_base = (char *)iov.iov_base + n;
        iov.iov_len -= n;
    } while (iov.iov_len);
}

#define I d = inc(d)
#define IC d = inc_carry(d)
#define FAT d = fat(d)
#define D I; p += writeDigits(p, d, i)
#define F I; p += writeFizz(p)
#define U I; p += writeFizzBuzz(p)
#define FNB I; I; p += writeFizzAndBuzz(p);
#define BNF I; I; p += writeBuzzAndFizz(p);
#define FST_ D; D; F; D; BNF;  D; D; I; p += writeFizzAndBuzz(p);
#define SND_ D; F; D; D; U; D; D; F; D; p += writeBuzzAndFizz(p);
#define TRD_ I; D; D; FNB;  D; F; D; D; p += writeFizzBuzz(p)
#define OUT do {\
    if (p - b + (17 * 16 + 5 * 12 + 9 * 2) > BUF_SIZE) {\
        out(b, p - b);\
        p = b;\
    }\
} while (false)
#define FST FST_; I; FAT
#define SND SND_; I; FAT
#define TRD TRD_; I; FAT; OUT
#define FSTC FST_; IC; FAT
#define SNDC SND_; IC; FAT
#define TRDC TRD_; IC; FAT; OUT
#define GROW do {\
    if (j == k) {\
        k *= 10;\
        --i;\
    }\
} while (false)

int main() {
    fcntl(1, F_SETPIPE_SZ, BUF_SIZE);
    __m128i d = _mm_set1_epi64x(Z);
    int i = sizeof(d) - 1;
    alignas(0x1000) static char b[BUF_SIZE];
    char *p = b;
    long long j = 10, k = 10;
    for (; j < 100000000 - 30; j += 30) {
        FST;
        GROW;
        SND;
        TRD;
    }
    int l = 0;
    for (; j < 10000000000000000; j += 100000000 / 30 * 30) {
        switch (l) {
            case 0: {
                l = 2;
                FST;
                SND;
                TRD;
                j += 30;
                FSTC;
                GROW;
                SND;
                TRD;
                break;
            } case 1: {
                --l;
                FST;
                SND;
                TRDC;
                break;
            } case 2: {
                --l;
                FST;
                SNDC;
                TRD;
                break;
            } default: {
                __builtin_unreachable();
            }
        }
        for (int k = 0; k < 100000000 / 30 - 1; ++k) {
            FST;
            SND;
            TRD;
        }
    }
    return 0;
}
