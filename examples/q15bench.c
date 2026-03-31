/*
 * Q.15 Packed SIMD32 Benchmark  (RV32 + P extension)
 *
 * On RV32, khm16 operates on 32-bit registers:
 *   khm16 rd, rs1, rs2
 *   rd[15:0]  = sat_q15(rs1[15:0]  * rs2[15:0]  >> 15)
 *   rd[31:16] = sat_q15(rs1[31:16] * rs2[31:16] >> 15)
 *
 * Each khm16 processes 2 x Q.15 elements packed in a 32-bit word.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ---- Q.15 helpers ---- */

static inline int16_t float_to_q15(float f)
{
    int32_t val = (int32_t)(f * 32768.0f);
    if (val > 32767) val = 32767;
    if (val < -32768) val = -32768;
    return (int16_t)val;
}

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
static inline uint32_t rdcycle(void)
{
    uint32_t c;
    __asm__ volatile ("rdcycle %0" : "=r"(c));
    return c;
}

/* ---- P-extension: khm16 packed SIMD32 ---- */

/* Pack two Q.15 values into a 32-bit word */
static inline uint32_t pack_2x16(int16_t lo, int16_t hi)
{
    return ((uint32_t)(uint16_t)hi << 16) | (uint16_t)lo;
}

/* Unpack low element from 32-bit packed */
static inline int16_t unpack_lo(uint32_t packed)
{
    return (int16_t)(packed & 0xFFFF);
}

/* Unpack high element from 32-bit packed */
static inline int16_t unpack_hi(uint32_t packed)
{
    return (int16_t)(packed >> 16);
}

/*
 * khm16 rd, rs1, rs2   (packed SIMD32: 2 x Q.15 multiply)
 * Encoding: funct7=1000011 | rs2 | rs1 | funct3=000 | rd | opcode=1110111
 * .insn r 0x77, 0, 0x43, rd, rs1, rs2
 */
static inline uint32_t p_khm16(uint32_t a, uint32_t b)
{
    uint32_t result;
    __asm__ volatile (
        ".insn r 0x77, 0, 0x43, %0, %1, %2"
        : "=r"(result)
        : "r"(a), "r"(b)
    );
    return result;
}

/* ---- Benchmark parameters ---- */

#define N 256  /* number of Q.15 elements (multiple of 2 for SIMD32) */
#define ITERS 100

static int16_t vec_a[N];
static int16_t vec_b[N];
static int16_t result_scalar[N];
static int16_t result_simd[N];

static void init_vectors(void)
{
    int i;
    const float pattern[] = {
         0.0f,  0.25f,  0.5f,   0.707f,
         0.9f,  0.707f, 0.5f,   0.25f,
         0.0f, -0.25f, -0.5f,  -0.707f,
        -0.9f, -0.707f, -0.5f, -0.25f
    };
    const float coeffs[] = {
        0.1f,  0.2f,  0.4f,  0.6f,
        0.8f,  0.95f, 0.95f, 0.8f,
        0.6f,  0.4f,  0.2f,  0.1f,
        0.05f, 0.02f, 0.01f, 0.005f
    };

    for (i = 0; i < N; i++) {
        vec_a[i] = float_to_q15(pattern[i % 16]);
        vec_b[i] = float_to_q15(coeffs[i % 16]);
    }
}

int main(int argc, char *argv[])
{
    int i, iter;
    uint32_t t0, t1;

    init_vectors();

    int p_ext = 1;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-simd") == 0)
            p_ext = 0;
    }

    printf("=== Q.15 Packed SIMD32 Benchmark (RV32) ===\n");
    printf("P extension: %s\n", p_ext ? "available" : "NOT available (scalar only)");
    printf("Register width: 32-bit\n");
    printf("khm16: 2 x Q.15 elements per instruction\n");
    printf("Vector size: %d elements, Iterations: %d\n\n", N, ITERS);

    /* ---- Scalar baseline ---- */
    t0 = rdcycle();
    for (iter = 0; iter < ITERS; iter++) {
        for (i = 0; i < N; i++) {
            result_scalar[i] = q15_mul(vec_a[i], vec_b[i]);
        }
    }
    t1 = rdcycle();
    uint32_t cycles_scalar = t1 - t0;

    /* ---- Sample output (scalar) ---- */
    printf("Sample results (first 16 of %d):\n", N);
    printf("  Scalar: ");
    for (i = 0; i < 16; i++) printf("%6d ", result_scalar[i]);
    printf("\n  Float:  ");
    for (i = 0; i < 16; i++) printf("%6.3f ", q15_to_float(result_scalar[i]));
    printf("\n\n");

    unsigned long total_ops = (unsigned long)N * ITERS;
    uint32_t cycles_simd = 0;

    if (p_ext) {
        /* ---- SIMD P-extension: khm16, 2 elements per 32-bit register ---- */
        t0 = rdcycle();
        for (iter = 0; iter < ITERS; iter++) {
            for (i = 0; i < N; i += 2) {
                uint32_t pa = pack_2x16(vec_a[i], vec_a[i+1]);
                uint32_t pb = pack_2x16(vec_b[i], vec_b[i+1]);
                uint32_t pr = p_khm16(pa, pb);
                result_simd[i+0] = unpack_lo(pr);
                result_simd[i+1] = unpack_hi(pr);
            }
        }
        t1 = rdcycle();
        cycles_simd = t1 - t0;

        /* ---- Validate ---- */
        int match = 1;
        int mismatches = 0;
        for (i = 0; i < N; i++) {
            if (result_scalar[i] != result_simd[i]) {
                if (mismatches < 5)
                    printf("  MISMATCH [%d]: scalar=%d simd=%d\n",
                           i, result_scalar[i], result_simd[i]);
                match = 0;
                mismatches++;
            }
        }
        printf("Validation: %s", match ? "PASS" : "FAIL");
        if (mismatches > 0)
            printf(" (%d mismatches)", mismatches);
        printf("\n\n");

        /* ---- SIMD sample output ---- */
        printf("  SIMD:   ");
        for (i = 0; i < 16; i++) printf("%6d ", result_simd[i]);
        printf("\n\n");

        /* ---- Saturation test ---- */
        int16_t sat_a = INT16_MIN;  /* -1.0 in Q.15 = -32768 */
        uint32_t pa_sat = pack_2x16(sat_a, sat_a);
        uint32_t pr_sat = p_khm16(pa_sat, pa_sat);
        int16_t sat_lo = unpack_lo(pr_sat);
        int16_t sat_hi = unpack_hi(pr_sat);
        printf("Saturation test: Q.15(-1.0) * Q.15(-1.0)\n");
        printf("  khm16 lo: %d  hi: %d  (expect %d)\n", sat_lo, sat_hi, INT16_MAX);
        printf("  Result: %s\n\n", (sat_lo == INT16_MAX && sat_hi == INT16_MAX) ? "PASS" : "FAIL");
    }

    /* ---- Performance ---- */
    printf("=== Performance Results ===\n");
    printf("  Scalar:  %lu cycles  (%lu ops)\n",
           (unsigned long)cycles_scalar, total_ops);
    printf("  Scalar:  %.2f cycles/element\n", (float)cycles_scalar / total_ops);

    if (p_ext) {
        printf("  SIMD:    %lu cycles  (%lu ops)\n",
               (unsigned long)cycles_simd, total_ops);
        printf("  SIMD:    %.2f cycles/element\n", (float)cycles_simd / total_ops);
        printf("\n");
        if (cycles_simd > 0) {
            float speedup = (float)cycles_scalar / (float)cycles_simd;
            printf("  Speedup: %.2fx\n", speedup);
            printf("  Cycle savings: %ld cycles (%.1f%%)\n",
                   (long)((long)cycles_scalar - (long)cycles_simd),
                   (1.0f - (float)cycles_simd / (float)cycles_scalar) * 100.0f);
        }
    } else {
        printf("\n  (SIMD benchmark skipped — P extension not available)\n");
    }

    printf("\n=== SIMD32 Instruction Analysis ===\n");
    printf("  Scalar: 1 multiply per element (load, load, mul, shift, sat, store)\n");
    printf("  SIMD32: 1 khm16 per 2 elements (pack, pack, khm16, unpack, store)\n");
    printf("  Theoretical max speedup: ~2x (2 ops per khm16 on RV32)\n");

    printf("\nDone.\n");
    return 0;
}
