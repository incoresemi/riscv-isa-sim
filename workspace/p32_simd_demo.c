/*
 * P32 SIMD Demo — RISC-V P-extension packed SIMD32 intrinsics
 *
 * Demonstrates common DSP operations using the __rv_* intrinsic API
 * on RV32 with the P extension.  Each operation processes 2 x int16
 * elements per instruction, packed in a 32-bit register.
 *
 * Build:   riscv64-unknown-elf-gcc -O2 -march=rv32gc -mabi=ilp32d -o p32_simd_demo p32_simd_demo.c
 * Run:     spike --isa=RV32IMAFDCP_zicntr_zihpm -m0x80000000:0x10000000 pk32 p32_simd_demo
 *
 * Or via:  ./spike-demo.sh make run-p32demo
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rvp_intrinsic.h"

/* ---- Q.15 helpers ---- */

static inline int16_t to_q15(float f)
{
    int32_t v = (int32_t)(f * 32768.0f);
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}

static inline float from_q15(int16_t q)
{
    return (float)q / 32768.0f;
}

/* ---- Print 2 packed Q.15 values ---- */
static void print_packed(const char *label, uint32_t p)
{
    int16_t lo = __rv_lo16(p);
    int16_t hi = __rv_hi16(p);
    printf("  %-20s = [%7d, %7d]  (%7.4f, %7.4f)\n",
           label, lo, hi, from_q15(lo), from_q15(hi));
}

/* ---- Demo 1: Basic SIMD arithmetic ---- */
static void demo_arithmetic(void)
{
    printf("=== Demo 1: SIMD16 Arithmetic ===\n\n");

    uint32_t a = __rv_pack16(to_q15(0.5f),  to_q15(-0.3f));
    uint32_t b = __rv_pack16(to_q15(0.25f), to_q15(0.7f));

    print_packed("a", a);
    print_packed("b", b);
    printf("\n");

    /* SIMD add: each 16-bit lane added independently */
    print_packed("add16(a, b)",    __rv_add16(a, b));

    /* SIMD subtract */
    print_packed("sub16(a, b)",    __rv_sub16(a, b));

    /* Saturating add — clamps to [-1.0, +1.0) in Q.15 */
    print_packed("kadd16(a, b)",   __rv_kadd16(a, b));

    /* Rounding average: (a + b + 1) >> 1 */
    print_packed("radd16(a, b)",   __rv_radd16(a, b));

    printf("\n");
}

/* ---- Demo 2: Q.15 Multiply ---- */
static void demo_multiply(void)
{
    printf("=== Demo 2: Q.15 Multiply (khm16) ===\n\n");

    uint32_t signal = __rv_pack16(to_q15(0.9f),  to_q15(-0.8f));
    uint32_t gain   = __rv_pack16(to_q15(0.5f),  to_q15(0.5f));

    print_packed("signal", signal);
    print_packed("gain",   gain);
    printf("\n");

    /* khm16: Q.15 saturating multiply — 2 multiplies in 1 instruction */
    uint32_t result = __rv_khm16(signal, gain);
    print_packed("khm16(sig, gain)", result);

    /* Saturation test: Q.15(-1.0) * Q.15(-1.0) should saturate to +0.99997 */
    uint32_t neg1 = __rv_pack16(to_q15(-1.0f), to_q15(-1.0f));
    print_packed("-1.0 * -1.0",  __rv_khm16(neg1, neg1));

    printf("\n");
}

/* ---- Demo 3: Min / Max clipping ---- */
static void demo_clip(void)
{
    printf("=== Demo 3: SIMD Clip (smin16 / smax16) ===\n\n");

    /* Clip signal to [-0.5, +0.5] range */
    uint32_t signal  = __rv_pack16(to_q15(0.9f),  to_q15(-0.8f));
    uint32_t lo_clip = __rv_pack16(to_q15(-0.5f), to_q15(-0.5f));
    uint32_t hi_clip = __rv_pack16(to_q15(0.5f),  to_q15(0.5f));

    print_packed("signal",     signal);
    print_packed("lo_clip",    lo_clip);
    print_packed("hi_clip",    hi_clip);
    printf("\n");

    /* Clip: max(lo, min(hi, signal)) — 2 instructions for 2 elements */
    uint32_t clipped = __rv_smax16(lo_clip, __rv_smin16(hi_clip, signal));
    print_packed("clipped",    clipped);

    printf("\n");
}

/* ---- Demo 4: 4-tap FIR filter on packed data ---- */
static void demo_fir(void)
{
    printf("=== Demo 4: 4-tap FIR Filter (khm16 + kadd16) ===\n\n");

    /* Q.15 filter coefficients: simple low-pass */
    int16_t h[4] = {
        to_q15(0.1f), to_q15(0.4f), to_q15(0.4f), to_q15(0.1f)
    };

    /* Input signal: step function */
    int16_t x[8] = {
        to_q15(0.0f), to_q15(0.0f), to_q15(1.0f), to_q15(1.0f),
        to_q15(1.0f), to_q15(1.0f), to_q15(0.0f), to_q15(0.0f)
    };

    printf("  Coefficients: ");
    for (int i = 0; i < 4; i++) printf("%.2f ", from_q15(h[i]));
    printf("\n  Input:        ");
    for (int i = 0; i < 8; i++) printf("%.2f ", from_q15(x[i]));
    printf("\n  Output:       ");

    /* Pack coefficient pairs */
    uint32_t h01 = __rv_pack16(h[0], h[1]);
    uint32_t h23 = __rv_pack16(h[2], h[3]);

    /* FIR: y[n] = sum(h[k] * x[n-k]) for k=0..3
     *
     * Using khm16 we compute 2 products per instruction, then
     * kadd16 to accumulate. Final result extracted from lo lane.
     */
    for (int n = 3; n < 8; n++) {
        /* Pack input pairs matching coefficient layout */
        uint32_t x01 = __rv_pack16(x[n], x[n-1]);
        uint32_t x23 = __rv_pack16(x[n-2], x[n-3]);

        /* 2 multiplies per khm16 */
        uint32_t p01 = __rv_khm16(h01, x01);   /* h0*x[n], h1*x[n-1] */
        uint32_t p23 = __rv_khm16(h23, x23);   /* h2*x[n-2], h3*x[n-3] */

        /* Sum all 4 products: add pairs, then add across */
        uint32_t sum_pairs = __rv_kadd16(p01, p23);  /* lo: p0+p2, hi: p1+p3 */
        int16_t lo = __rv_lo16(sum_pairs);
        int16_t hi = __rv_hi16(sum_pairs);
        int32_t y = (int32_t)lo + (int32_t)hi;
        if (y > 32767) y = 32767;
        if (y < -32768) y = -32768;

        printf("%.2f ", from_q15((int16_t)y));
    }
    printf("\n\n");
}

/* ---- Demo 5: 8-bit SIMD (4 elements per register) ---- */
static void demo_8bit(void)
{
    printf("=== Demo 5: SIMD8 Arithmetic (4 x int8) ===\n\n");

    /* Pack 4 x int8 into a 32-bit register */
    uint32_t a = __rv_pack8(10, 20, -30, 100);
    uint32_t b = __rv_pack8(5,  -10, 40,  50);

    printf("  a = [%4d, %4d, %4d, %4d]\n",
           (int8_t)(a & 0xFF), (int8_t)((a>>8) & 0xFF),
           (int8_t)((a>>16) & 0xFF), (int8_t)((a>>24) & 0xFF));
    printf("  b = [%4d, %4d, %4d, %4d]\n",
           (int8_t)(b & 0xFF), (int8_t)((b>>8) & 0xFF),
           (int8_t)((b>>16) & 0xFF), (int8_t)((b>>24) & 0xFF));

    uint32_t sum = __rv_add8(a, b);
    printf("\n  add8(a, b)  = [%4d, %4d, %4d, %4d]\n",
           (int8_t)(sum & 0xFF), (int8_t)((sum>>8) & 0xFF),
           (int8_t)((sum>>16) & 0xFF), (int8_t)((sum>>24) & 0xFF));

    uint32_t sat = __rv_kadd8(a, b);
    printf("  kadd8(a, b) = [%4d, %4d, %4d, %4d]  (saturated)\n",
           (int8_t)(sat & 0xFF), (int8_t)((sat>>8) & 0xFF),
           (int8_t)((sat>>16) & 0xFF), (int8_t)((sat>>24) & 0xFF));

    uint32_t mx = __rv_smax8(a, b);
    printf("  smax8(a, b) = [%4d, %4d, %4d, %4d]\n",
           (int8_t)(mx & 0xFF), (int8_t)((mx>>8) & 0xFF),
           (int8_t)((mx>>16) & 0xFF), (int8_t)((mx>>24) & 0xFF));

    printf("\n");
}

int main(int argc, char *argv[])
{
    int has_p = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-simd") == 0)
            has_p = 0;
    }

    printf("=== RISC-V P32 SIMD Intrinsics Demo ===\n");
    printf("Using rvp_intrinsic.h (inline asm, works with any GCC)\n\n");

    if (!has_p) {
        printf("P extension not available (--no-simd). Skipping demos.\n");
        return 0;
    }

    demo_arithmetic();
    demo_multiply();
    demo_clip();
    demo_fir();
    demo_8bit();

    printf("=== All demos complete ===\n");
    return 0;
}
