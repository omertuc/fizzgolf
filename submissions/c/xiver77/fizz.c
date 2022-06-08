#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>
#include <fcntl.h>
#include <smmintrin.h>

static const __m128i shiftMask[] = {
  {0x0706050403020100, 0x0f0e0d0c0b0a0908},
  {0x0807060504030201, 0xff0f0e0d0c0b0a09},
  {0x0908070605040302, 0xffff0f0e0d0c0b0a},
  {0x0a09080706050403, 0xffffff0f0e0d0c0b},
  {0x0b0a090807060504, 0xffffffff0f0e0d0c},
  {0x0c0b0a0908070605, 0xffffffffff0f0e0d},
  {0x0d0c0b0a09080706, 0xffffffffffff0f0e},
  {0x0e0d0c0b0a090807, 0xffffffffffffff0f},
  {0x0f0e0d0c0b0a0908, 0xffffffffffffffff},
  {0xff0f0e0d0c0b0a09, 0xffffffffffffffff},
  {0xffff0f0e0d0c0b0a, 0xffffffffffffffff},
  {0xffffff0f0e0d0c0b, 0xffffffffffffffff},
  {0xffffffff0f0e0d0c, 0xffffffffffffffff},
  {0xffffffffff0f0e0d, 0xffffffffffffffff},
  {0xffffffffffff0f0e, 0xffffffffffffffff},
  {0xffffffffffffff0f, 0xffffffffffffffff}
};

static __m128i inc(__m128i d) {
  return _mm_sub_epi64(d, _mm_set_epi64x(0, -1));
}

static __m128i carry(__m128i d) {
  d = _mm_sub_epi64(d,
    _mm_bslli_si128(_mm_cmpeq_epi64(d, _mm_setzero_si128()), 8));
  return _mm_or_si128(d,
    _mm_and_si128(_mm_cmpeq_epi8(d, _mm_setzero_si128()), _mm_set1_epi8(0xf6)));
}

static int writeDigits(char *b, __m128i d, int i) {
  _mm_storeu_si128((__m128i *)b,
    _mm_shuffle_epi8(
      _mm_shuffle_epi8(_mm_sub_epi64(d, _mm_set1_epi8(0xc6)),
        _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)),
      shiftMask[i]));
  b[16 - i] = '\n';
  return 17 - i;
}

static int writeFizz(void *b) {
  memcpy(b, &(int64_t){0x0a7a7a6946}, 8);
  return 5;
}

static int writeFizzBuzz(void *b) {
  memcpy(b, &(int64_t){0x7a7a75427a7a6946}, 8);
  ((char *)b)[8] = '\n';
  return 9;
}

static int writeFizzAndBuzz(void *b) {
  memcpy(b, &(int64_t){0x7a75420a7a7a6946}, 8);
  memcpy(b + 8, &(int16_t){0x0a7a}, 2);
  return 10;
}

static int writeBuzzAndFizz(void *b) {
  memcpy(b, &(int64_t){0x7a69460a7a7a7542}, 8);
  memcpy(b + 8, &(int16_t){0x0a7a}, 2);
  return 10;
}

static void memcpy_simple(void *d, void *s, int n) {
  int i = 0;
  do {
    _mm_store_si128((void *)((char *)d + i),
      _mm_load_si128((void *)((char *)s + i)));
  } while ((i += 16) < n);
}

#define ALIGN 0x1000
#define SIZE (ALIGN << 8)
#define I d = inc(d)
#define IC d = carry(inc(d))
#define D I; p += writeDigits(p, d, i)
#define F I; p += writeFizz(p)
#define FB I; p += writeFizzBuzz(p)
#define FNB I; I; p += writeFizzAndBuzz(p)
#define BNF I; I; p += writeBuzzAndFizz(p)

int main() {
  if (fcntl(1, F_SETPIPE_SZ, SIZE) != SIZE) {
    abort();
  }
  alignas(ALIGN) char b[2][SIZE + ALIGN];
  int f = 0;
  char *p = b[f];
  __m128i d = _mm_set1_epi8(0xf6);
  int i = 15;
  for (int64_t j = 10, k = 10;; j += 30) {
    D; D; F; D; BNF; D; D; I; IC; p += writeFizzAndBuzz(p);
    if (j == k) {
      k *= 10;
      --i;
    }
    D; F; D; D; FB; D; D; F; D; IC; p += writeBuzzAndFizz(p);
    I; D; D; FNB; D; F; D; D; IC; p += writeFizzBuzz(p);
    int n = p - b[f] - SIZE;
    if (n >= 0) {
      struct iovec v = {b[f], SIZE};
      do {
        register long rax __asm__ ("rax") = 278;
        register long rdi __asm__ ("rdi") = 1;
        register long rsi __asm__ ("rsi") = (long)&v;
        register long rdx __asm__ ("rdx") = 1;
        register long r10 __asm__ ("r10") = 0;
        __asm__ ("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10)
        : "rcx", "r11");
        if (rax < 0) {
          abort();
        }
        v.iov_base = (char *)v.iov_base + rax;
        v.iov_len -= rax;
      } while (v.iov_len);
      f = !f;
      memcpy_simple(b[f], b[!f] + SIZE, n);
      p = b[f] + n;
    }
  }
  return 0;
}
