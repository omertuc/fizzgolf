/*
 * Author: Paolo Bonzini
 * gcc fb.c -o fb -O2 -g -mavx -mavx2 -flax-vector-conversions
 */
#define _GNU_SOURCE

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include <avxintrin.h>

#define F_SETPIPE_SZ 1031

#define ZERO    15

typedef uint8_t v32qi __attribute__((vector_size(32), aligned(32)));
typedef uint8_t v32qi_u __attribute__((vector_size(32), aligned(1)));

v32qi lo_bytes = {
    '1', '0', '0', '0', '0', '0', '0', '0',     /* 0 */
    '0', '0', '0', '0', '0', '0', '0', '\0',    /* 8 */
    '1', '0', '0', '0', '0', '0', '0', '0',     /* 0 */
    '0', '0', '0', '0', '0', '0', '0', '\0',    /* 8 */
};

uint8_t hi_bytes[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7',     /* 16 */
    '8', '9', '\n', 'z', 'u', 'B', 'i', 'F',    /* 24 */
};

static v32qi biased_zero = {
    246, 246, 246, 246, 246, 246, 246, 246,
    246, 246, 246, 246, 246, 246, 246, 246,
    246, 246, 246, 246, 246, 246, 246, 246,
    246, 246, 246, 246, 246, 246, 246, 246,
};

static v32qi biased_line = {
    247, 246, 246, 246, 246, 246, 246, 246,
    246, 246, 246, 246, 246, 246, 246, 246,
    247, 246, 246, 246, 246, 246, 246, 246,
    246, 246, 246, 246, 246, 246, 246, 246,
};

static v32qi incr_low_mask = {
    1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static v32qi incr_high_mask = {
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0,
};

#define OUTPUT_SIZE(n)      (94+16*n)
#define TEMPLATE_SIZE(n)    ((OUTPUT_SIZE(n) + 31) & ~31)
#define MAX         15

#define OUTBUF_SIZE     1048576

static uint8_t template[TEMPLATE_SIZE(MAX)];
static uint8_t *output1;
static uint8_t *output2;
static uint8_t *output;
#define BOUNDARY (output + OUTBUF_SIZE)

static v32qi mask[26];
static v32qi *mask_ptr;

static uint8_t code_buffer[64];
static uint8_t *code_ptr;

static uint8_t *jit_buffer;
static uint8_t *jit_ptr;
typedef uint8_t *jit_fn(uint8_t *, int);

/*
 * Bytecode language:
 *   0 = mov 32 bytes into temp buffer from the next mask
 *   1 = or 32 bytes into temp buffer from the next mask
 *   2 = increment the line
 *   3 = store 32 bytes from temp buffer
 *   -32..-1 = add n to the output pointer
 */
static void gen_prolog(void)
{
    code_ptr = code_buffer;
    mask_ptr = mask;
}

static void gen_epilog(int template_size)
{
    *code_ptr++ = 3;
    *code_ptr++ = (template_size & 31) - 32;
}

static void do_gen_or_code(int from)
{
    assert(mask_ptr - mask < sizeof(mask) / sizeof(mask[0]));

    // o[i++] |= out_bytes[template[from + i]];
    for (int i = 0; i < 32; i++) {
        uint8_t m = template[from + i];
        if (m < 16) {
            mask_ptr[0][i] = m;
            mask_ptr[1][i] = 0;
        } else {
            mask_ptr[0][i] = -1;
            mask_ptr[1][i] = hi_bytes[m - 16];
        }
    }
    *code_ptr++ = 1;
    mask_ptr += 2;
}

static void do_gen_mov_code(int from, int to)
{
    assert(mask_ptr - mask < sizeof(mask) / sizeof(mask[0]));

    // o[i++] = out_bytes[template[from + i]];
    for (int i = 0; i < 32; i++) {
        uint8_t m = (from + i > to) ? ZERO : template[from + i];
        if (m < 16) {
            mask_ptr[0][i] = m;
            mask_ptr[1][i] = 0;
        } else {
            mask_ptr[0][i] = -1;
            mask_ptr[1][i] = hi_bytes[m - 16];
        }
    }
    *code_ptr++ = 0;
    mask_ptr += 2;
}

static void gen_inc_code(void)
{
    *code_ptr++ = 2;
}

static void gen_out_code(int from, int to)
{
    int offset = from & ~31;
    if (offset < from) {
        assert(to >= offset + 32);
        do_gen_or_code(offset);
        offset += 32;
        *code_ptr++ = 3;
    }
    while (offset < to) {
        do_gen_mov_code(offset, to);
        offset += 32;
        if (offset <= to)
            *code_ptr++ = 3;
    }
    memset(template + from, ZERO, to - from);
}

static void inc_line(v32qi incr_mask)
{
    v32qi old = biased_line;
    v32qi incr = _mm256_add_epi64((__m256i)incr_mask, (__m256i)old);
    biased_line = _mm256_max_epu8(incr, biased_zero);
    lo_bytes += biased_line - old;
}

static v32qi do_shuffle(v32qi *mask)
{
    v32qi digits = __builtin_ia32_pshufb256(lo_bytes, mask[0]);
    return digits | mask[1];
}

static uint8_t *maybe_output(uint8_t *o)
{
    if (o > output + OUTBUF_SIZE) {
#if 1
        struct iovec iov = {output, OUTBUF_SIZE};
        do {
            ssize_t r = vmsplice(1, &iov, 1, 0);
            if (r < 0) {
                perror("vmsplice");
                exit(1);
            }
            iov.iov_base += r;
            iov.iov_len -= r;
        } while (iov.iov_len);
#else
        write(1, output, OUTBUF_SIZE);
#endif
        if (output == output1) {
            memcpy(output2, BOUNDARY, o - BOUNDARY);
            o = output2 + (o - BOUNDARY);
            output = output2;
        } else {
            memcpy(output1, BOUNDARY, o - BOUNDARY);
            o = output1 + (o - BOUNDARY);
            output = output1;
        }
    }
    return o;
}

static uint8_t *slow_run(uint8_t *o, int carry)
{
    const uint8_t *p;
    v32qi *m = mask;
    v32qi temp;
    for (p = code_buffer; p < code_ptr; p++) {
        uint8_t c = *p;
        if (c == 0) {
            temp = do_shuffle(m);
            m += 2;
        } else if (c == 1) {
            temp |= do_shuffle(m);
            m += 2;
        } else if (c == 3) {
            *(v32qi_u *)o = temp;
            o += 32;
        } else if (c == 2) {
            inc_line(incr_low_mask);
            if (--carry == 0)
                inc_line(incr_high_mask);
        } else {
            o += (int8_t) c;
        }
    }
    return maybe_output(o);
}

#define o(b) (*jit_ptr++ = (0x##b))
#define s(p) jit_ptr += ({ uint32_t x = (uintptr_t)p - (uintptr_t)(jit_ptr + 4); memcpy(jit_ptr, &x, 4); 4; })
#define d(i) jit_ptr += ({ uint32_t x = (i); memcpy(jit_ptr, &x, 4); 4; })

void compile(void)
{
    const uint8_t *p, *label;
    v32qi *m = mask;
    int ofs = 0;

    jit_ptr = jit_buffer;
    o(C5),o(FD),o(6F),o(05),s(&lo_bytes);      // vmovdqa ymm0, lo_bytes
    o(C5),o(FD),o(6F),o(15),s(&biased_line);   // vmovdqa ymm2, biased_line
    o(C5),o(FD),o(6F),o(1D),s(&biased_zero);   // vmovdqa ymm3, biased_zero
    o(C5),o(FD),o(6F),o(25),s(&incr_low_mask); // vmovdqa ymm4, incr_low_mask

    /* in inc_line, lo_bytes - old is always the same.  Put it in ymm1.  */
    o(C5),o(FD),o(F8),o(CA);           // vpsubb ymm1, ymm0, ymm2

    label = jit_ptr;
    for (p = code_buffer; p < code_ptr; p++) {
        uint8_t c = *p;
        if (c == 0) {
            o(C5),o(FD),o(6F),o(35),s(m);  // vmovdqa ymm6, MASK
            m++;
            o(C5),o(FD),o(6F),o(2D),s(m);  // vmovdqa ymm5, MASK
            m++;
            o(C4),o(E2),o(7D),o(00),o(F6); // vpshufb ymm6, ymm0, ymm6
            o(C5),o(D5),o(EB),o(EE);       // vpor ymm5, ymm5, ymm6
        } else if (c == 1) {
            o(C5),o(FD),o(6F),o(35),s(m);  // vmovdqa ymm6, MASK
            m++;
            o(C5),o(FD),o(6F),o(3D),s(m);  // vmovdqa ymm7, MASK
            m++;
            o(C4),o(E2),o(7D),o(00),o(F6); // vpshufb ymm6, ymm0, ymm6
            o(C5),o(D5),o(EB),o(EF);       // vpor ymm5, ymm5, ymm7
            o(C5),o(D5),o(EB),o(EE);       // vpor ymm5, ymm5, ymm6
        } else if (c == 3) {
            o(C5),o(FE),o(7F),o(AF),d(ofs); // vmovdqu [rdi+NNN], ymm5
            ofs += 32;
        } else if (c == 2) {
            o(C5),o(ED),o(D4),o(D4);      // vpaddq ymm2, ymm2, ymm4
            o(C5),o(ED),o(DE),o(D3);      // vpmaxub ymm2, ymm2, ymm3
            o(C5),o(F5),o(FC),o(C2);      // vpaddb ymm0, ymm1, ymm2
        } else {
            ofs += (int8_t) c;
        }
    }
    o(48),o(81),o(C7),d(ofs);   // add rdi, ofs
    o(FF),o(CE);                // dec esi
    o(0F),o(85),s(label);       // jnz label
    o(48),o(89),o(F8);          // mov rax, rdi
    o(C5),o(FD),o(7F),o(05),s(&lo_bytes);     // vmovdqa lo_bytes, ymm0
    o(C5),o(FD),o(7F),o(15),s(&biased_line);  // vmovdqa biased_line, ymm2
    o(C3);                // ret
}

#define INITIAL "1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nFizz\n"
#define TENS_FOR_VPADDQ (10000 * 10000)

int main()
{
    uint8_t shuffle[] = { 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 16, 26 };
    const uint8_t fizz[] = { 31, 30, 27, 27, 26 };
    const uint8_t fizzbuzz[] = { 31, 30, 27, 27, 29, 28, 27, 27, 26 };
    const uint8_t *buzz = fizzbuzz + 4;
    
    int l;
    uint64_t n;
    uint32_t tens_till_carry = TENS_FOR_VPADDQ - 1;

    output1 = mmap(NULL, OUTBUF_SIZE + 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    output2 = mmap(NULL, OUTBUF_SIZE + 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    output = output1;
    uint8_t *o = mempcpy(output, INITIAL, strlen(INITIAL));

    fcntl(1, F_SETPIPE_SZ, OUTBUF_SIZE);
    memset(template, ZERO, sizeof(template));

    jit_buffer = mmap(NULL, 16384, PROT_READ|PROT_WRITE|PROT_EXEC,
              MAP_32BIT|MAP_PRIVATE|MAP_ANON, -1, 0);
    assert((uintptr_t)mask <= 0x7FFFFFFF);
    assert((uintptr_t)jit_buffer <= 0x7FFFFFFF);

    for (l = 2, n = 3; l <= MAX; l++, n = n * 10) {
        int output_size = OUTPUT_SIZE(l);
        int template_size = TEMPLATE_SIZE(l);

        uint8_t *s = shuffle + sizeof(shuffle) - l - 1;
        uint8_t *p = template;

#define ZERO_UNITS s[l - 1] = 16;
#define INC_UNITS s[l - 1]++;

        ZERO_UNITS; p = mempcpy(p, buzz, 5); // 10
        INC_UNITS; p = mempcpy(p, s, l + 1); // 11
        INC_UNITS; p = mempcpy(p, fizz, 5); // 12
        INC_UNITS; p = mempcpy(p, s, l + 1); // 13
        INC_UNITS; p = mempcpy(p, s, l + 1); // 14
        INC_UNITS; p = mempcpy(p, fizzbuzz, 9); // 15
        INC_UNITS; p = mempcpy(p, s, l + 1); // 16
        INC_UNITS; p = mempcpy(p, s, l + 1); // 17
        INC_UNITS; p = mempcpy(p, fizz, 5); // 18
        INC_UNITS; p = mempcpy(p, s, l + 1); // 19
        ZERO_UNITS; p = mempcpy(p, buzz, 5); // 20
        INC_UNITS; p = mempcpy(p, fizz, 5); // 21
        INC_UNITS; p = mempcpy(p, s, l + 1); // 22
        INC_UNITS; p = mempcpy(p, s, l + 1); // 23
        INC_UNITS; p = mempcpy(p, fizz, 5); // 24
        INC_UNITS; p = mempcpy(p, buzz, 5); // 25
        INC_UNITS; p = mempcpy(p, s, l + 1); // 26
        INC_UNITS; p = mempcpy(p, fizz, 5); // 27
        INC_UNITS; p = mempcpy(p, s, l + 1); // 28
        INC_UNITS; p = mempcpy(p, s, l + 1); // 29
        ZERO_UNITS; p = mempcpy(p, fizzbuzz, 9); // 30
        INC_UNITS; p = mempcpy(p, s, l + 1); // 31
        INC_UNITS; p = mempcpy(p, s, l + 1); // 32
        INC_UNITS; p = mempcpy(p, fizz, 5); // 33
        INC_UNITS; p = mempcpy(p, s, l + 1); // 34
        INC_UNITS; p = mempcpy(p, buzz, 5); // 35
        INC_UNITS; p = mempcpy(p, fizz, 5); // 36
        INC_UNITS; p = mempcpy(p, s, l + 1); // 37
        INC_UNITS; p = mempcpy(p, s, l + 1); // 38
        INC_UNITS; p = mempcpy(p, fizz, 5); // 39
        memset(p, ZERO, template + template_size - p);

        gen_prolog();
        gen_out_code(0, 30+6*l);
        gen_inc_code();
        gen_out_code(30+6*l, 60+11*l);
        gen_inc_code();
        gen_out_code(60+11*l, 94+16*l);
        gen_inc_code();
        gen_epilog(94+16*l);

        compile();

        uint64_t left = n;
        do {
            int runs;
            if (tens_till_carry <= 3) {
                if (tens_till_carry == 0) {
                    inc_line(incr_high_mask);
                    runs = 0;
                } else {
                    o = slow_run(o, tens_till_carry);
                    runs = 1;
                }
                tens_till_carry += TENS_FOR_VPADDQ;
            } else {
                runs = (BOUNDARY - o) / output_size + 1;
                if (runs > left)
                    runs = left;
                if (runs * 3 > tens_till_carry)
                    runs = tens_till_carry / 3;

                o = ((jit_fn *)jit_buffer) (o, runs);
                o = maybe_output(o);
            }
            left -= runs;
            tens_till_carry -= runs * 3;
        } while (left);
    }
    write(1, output, o - output);
}
