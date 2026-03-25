/*
 * Q.15 Fixed-Point Packed SIMD Demo
 *
 * Demonstrates elementwise multiply of two Q.15 vectors using:
 *   1. Scalar C code (baseline)
 *   2. RISC-V P-extension packed SIMD (khm16 instruction)
 *
 * Q.15 format: signed 16-bit, 1 sign bit + 15 fractional bits
 *   Range: [-1.0, +0.999969] mapped to [-32768, +32767]
 *   Multiply: (a * b) >> 15, with saturation at -1.0 * -1.0
 *
 * khm16 instruction: operates on two packed 16-bit pairs in a 32-bit register
 *   khm16 rd, rs1, rs2
 *   rd[15:0]  = sat_q15(rs1[15:0]  * rs2[15:0]  >> 15)
 *   rd[31:16] = sat_q15(rs1[31:16] * rs2[31:16] >> 15)
 *
 * On RV64, khm16 processes FOUR 16-bit elements per instruction:
 *   rd[15:0]  = sat_q15(rs1[15:0]  * rs2[15:0]  >> 15)
 *   rd[31:16] = sat_q15(rs1[31:16] * rs2[31:16] >> 15)
 *   rd[47:32] = sat_q15(rs1[47:32] * rs2[47:32] >> 15)
 *   rd[63:48] = sat_q15(rs1[63:48] * rs2[63:48] >> 15)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ---- Q.15 helpers ---- */

/* Convert float to Q.15 */
static inline int16_t float_to_q15(float f)
{
    int32_t val = (int32_t)(f * 32768.0f);
    if (val > 32767) val = 32767;
    if (val < -32768) val = -32768;
    return (int16_t)val;
}

/* Convert Q.15 to float */
static inline float q15_to_float(int16_t q)
{
    return (float)q / 32768.0f;
}

/* Scalar Q.15 multiply with saturation */
static inline int16_t q15_mul(int16_t a, int16_t b)
{
    if (a == INT16_MIN && b == INT16_MIN)
        return INT16_MAX;  /* saturate -1.0 * -1.0 */
    return (int16_t)(((int32_t)a * (int32_t)b) >> 15);
}

/* ---- Read cycle counter ---- */
static inline uint64_t rdcycle(void)
{
    uint64_t c;
    __asm__ volatile ("rdcycle %0" : "=r"(c));
    return c;
}

/* ---- P-extension: khm16 via .insn encoding ---- */
/*
 * khm16 rd, rs1, rs2
 * Encoding: funct7=1000011 | rs2 | rs1 | funct3=000 | rd | opcode=1110111
 * MATCH_KHM16 = 0x86000077
 *
 * On RV64: processes 4 x Q.15 elements packed in a 64-bit register
 */

/* Pack two Q.15 values into a 32-bit word */
static inline uint32_t pack_2x16(int16_t lo, int16_t hi)
{
    return ((uint32_t)(uint16_t)hi << 16) | (uint16_t)lo;
}

/* Pack four Q.15 values into a 64-bit word (RV64) */
static inline uint64_t pack_4x16(int16_t a, int16_t b, int16_t c, int16_t d)
{
    return ((uint64_t)(uint16_t)d << 48) |
           ((uint64_t)(uint16_t)c << 32) |
           ((uint64_t)(uint16_t)b << 16) |
           (uint16_t)a;
}

/* Unpack element i (0..3) from packed 64-bit */
static inline int16_t unpack_16(uint64_t packed, int i)
{
    return (int16_t)(packed >> (i * 16));
}

/* khm16 instruction: 4 parallel Q.15 multiplies on RV64 */
static inline uint64_t p_khm16(uint64_t a, uint64_t b)
{
    uint64_t result;
    /* .insn r opcode, funct3, funct7, rd, rs1, rs2 */
    __asm__ volatile (
        ".insn r 0x77, 0, 0x43, %0, %1, %2"
        : "=r"(result)
        : "r"(a), "r"(b)
    );
    return result;
}

/* kadd16 instruction: 4 parallel saturating adds on RV64 */
static inline uint64_t p_kadd16(uint64_t a, uint64_t b)
{
    uint64_t result;
    __asm__ volatile (
        ".insn r 0x77, 0, 0x08, %0, %1, %2"
        : "=r"(result)
        : "r"(a), "r"(b)
    );
    return result;
}

/* ---- Test vectors ---- */

#define N 16  /* number of Q.15 elements (must be multiple of 4 for SIMD) */

/* Input signals: sine-like test pattern */
static const float vec_a_f[N] = {
     0.0f,  0.25f,  0.5f,   0.707f,
     0.9f,  0.707f, 0.5f,   0.25f,
     0.0f, -0.25f, -0.5f,  -0.707f,
    -0.9f, -0.707f, -0.5f, -0.25f
};

/* Coefficients: window/filter taps */
static const float vec_b_f[N] = {
    0.1f,  0.2f,  0.4f,  0.6f,
    0.8f,  0.95f, 0.95f, 0.8f,
    0.6f,  0.4f,  0.2f,  0.1f,
    0.05f, 0.02f, 0.01f, 0.005f
};

int main(void)
{
    int16_t a[N], b[N];
    int16_t result_scalar[N], result_simd[N];
    int i;

    /* Convert float test vectors to Q.15 */
    printf("=== Q.15 Packed SIMD Demo (P-extension khm16) ===\n\n");

    printf("Input vectors (float -> Q.15 hex):\n");
    for (i = 0; i < N; i++) {
        a[i] = float_to_q15(vec_a_f[i]);
        b[i] = float_to_q15(vec_b_f[i]);
    }

    printf("  A: ");
    for (i = 0; i < N; i++) printf("%6d ", a[i]);
    printf("\n  B: ");
    for (i = 0; i < N; i++) printf("%6d ", b[i]);
    printf("\n\n");

    /* ---- Scalar baseline ---- */
    uint64_t t0, t1;

    t0 = rdcycle();
    for (i = 0; i < N; i++) {
        result_scalar[i] = q15_mul(a[i], b[i]);
    }
    t1 = rdcycle();
    uint64_t cycles_scalar = t1 - t0;

    printf("Scalar results:  ");
    for (i = 0; i < N; i++) printf("%6d ", result_scalar[i]);
    printf("\n");
    printf("  (as float):    ");
    for (i = 0; i < N; i++) printf("%6.3f ", q15_to_float(result_scalar[i]));
    printf("\n");
    printf("  Cycles: %lu\n\n", (unsigned long)cycles_scalar);

    /* ---- SIMD P-extension (khm16, 4 elements per instruction on RV64) ---- */
    t0 = rdcycle();
    for (i = 0; i < N; i += 4) {
        uint64_t pa = pack_4x16(a[i], a[i+1], a[i+2], a[i+3]);
        uint64_t pb = pack_4x16(b[i], b[i+1], b[i+2], b[i+3]);
        uint64_t pr = p_khm16(pa, pb);
        result_simd[i+0] = unpack_16(pr, 0);
        result_simd[i+1] = unpack_16(pr, 1);
        result_simd[i+2] = unpack_16(pr, 2);
        result_simd[i+3] = unpack_16(pr, 3);
    }
    t1 = rdcycle();
    uint64_t cycles_simd = t1 - t0;

    printf("SIMD results:    ");
    for (i = 0; i < N; i++) printf("%6d ", result_simd[i]);
    printf("\n");
    printf("  (as float):    ");
    for (i = 0; i < N; i++) printf("%6.3f ", q15_to_float(result_simd[i]));
    printf("\n");
    printf("  Cycles: %lu\n\n", (unsigned long)cycles_simd);

    /* ---- Validate: compare scalar vs SIMD ---- */
    int match = 1;
    for (i = 0; i < N; i++) {
        if (result_scalar[i] != result_simd[i]) {
            printf("  MISMATCH at [%d]: scalar=%d, simd=%d\n",
                   i, result_scalar[i], result_simd[i]);
            match = 0;
        }
    }
    if (match)
        printf("Validation: PASS (scalar == SIMD for all %d elements)\n\n", N);
    else
        printf("Validation: FAIL\n\n");

    /* ---- Performance comparison ---- */
    printf("Performance:\n");
    printf("  Scalar:  %lu cycles for %d elements (%.1f cycles/elem)\n",
           (unsigned long)cycles_scalar, N, (float)cycles_scalar / N);
    printf("  SIMD:    %lu cycles for %d elements (%.1f cycles/elem)\n",
           (unsigned long)cycles_simd, N, (float)cycles_simd / N);
    if (cycles_simd > 0) {
        printf("  Speedup: %.2fx\n", (float)cycles_scalar / (float)cycles_simd);
    }

    /* ---- Test saturation: -1.0 * -1.0 should saturate to +0.999969 ---- */
    printf("\nSaturation test: Q.15(-1.0) * Q.15(-1.0)\n");
    int16_t sat_a = INT16_MIN;  /* -1.0 in Q.15 = -32768 */
    int16_t sat_b = INT16_MIN;
    int16_t sat_scalar = q15_mul(sat_a, sat_b);

    uint64_t pa_sat = pack_4x16(sat_a, sat_a, sat_a, sat_a);
    uint64_t pb_sat = pack_4x16(sat_b, sat_b, sat_b, sat_b);
    uint64_t pr_sat = p_khm16(pa_sat, pb_sat);
    int16_t sat_simd = unpack_16(pr_sat, 0);

    printf("  Scalar: %d (%.6f)\n", sat_scalar, q15_to_float(sat_scalar));
    printf("  SIMD:   %d (%.6f)\n", sat_simd, q15_to_float(sat_simd));
    printf("  Expected: %d (%.6f) — saturated to Q.15 max\n",
           INT16_MAX, q15_to_float(INT16_MAX));

    if (sat_scalar == INT16_MAX && sat_simd == INT16_MAX)
        printf("  Saturation: PASS\n");
    else
        printf("  Saturation: FAIL\n");

    printf("\nDone.\n");
    return 0;
}
