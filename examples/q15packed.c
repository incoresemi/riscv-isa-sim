/*
 * Q.15 Packed SIMD32 Benchmark — Packed Memory Layout (RV32 + P ext)
 *
 * This version stores data pre-packed in memory: two Q.15 values per 32-bit
 * word, matching how real DSP firmware would lay out buffers.
 *
 * SIMD path:  lw -> lw -> khm16 -> sw   (4 insns per 2 elements)
 * Scalar path: lh -> lh -> mul -> srai -> sat -> sh  (per element)
 *
 * This eliminates the artificial pack/unpack overhead and gives a realistic
 * picture of the speedup from packed SIMD32.
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
        return INT16_MAX;
    return (int16_t)(((int32_t)a * (int32_t)b) >> 15);
}

/* ---- Read cycle counter ---- */
static inline uint32_t rdcycle(void)
{
    uint32_t c;
    __asm__ volatile ("rdcycle %0" : "=r"(c));
    return c;
}

/* ---- P-extension: khm16 ---- */

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

#define N 256       /* number of Q.15 elements (multiple of 2) */
#define NPACKED (N / 2)  /* number of packed 32-bit words */
#define ITERS 100

/*
 * Packed layout: each uint32_t holds two Q.15 values.
 *   word[i] = { elem[2i+1] << 16 | elem[2i] }
 *
 * This is how a real DSP buffer would be organized — interleaved
 * sample pairs, filter tap pairs, or I/Q signal pairs.
 */
static uint32_t packed_a[NPACKED];
static uint32_t packed_b[NPACKED];
static uint32_t packed_result_simd[NPACKED];
static uint32_t packed_result_scalar[NPACKED];

/* Unpacked arrays for scalar reference */
static int16_t vec_a[N];
static int16_t vec_b[N];
static int16_t result_scalar[N];

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

    /* Fill unpacked arrays */
    for (i = 0; i < N; i++) {
        vec_a[i] = float_to_q15(pattern[i % 16]);
        vec_b[i] = float_to_q15(coeffs[i % 16]);
    }

    /* Pre-pack into 32-bit words: [hi:lo] = [elem[2i+1] : elem[2i]] */
    for (i = 0; i < NPACKED; i++) {
        packed_a[i] = ((uint32_t)(uint16_t)vec_a[2*i+1] << 16)
                    | (uint16_t)vec_a[2*i];
        packed_b[i] = ((uint32_t)(uint16_t)vec_b[2*i+1] << 16)
                    | (uint16_t)vec_b[2*i];
    }
}

/* Extract lo/hi from packed word for display */
static inline int16_t lo16(uint32_t w) { return (int16_t)(w & 0xFFFF); }
static inline int16_t hi16(uint32_t w) { return (int16_t)(w >> 16); }

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

    printf("=== Q.15 Packed-Memory SIMD32 Benchmark (RV32) ===\n");
    printf("P extension: %s\n", p_ext ? "available" : "NOT available (scalar only)");
    printf("Data layout: 2 x Q.15 pre-packed per 32-bit word\n");
    printf("Vector size: %d elements (%d packed words), Iterations: %d\n\n",
           N, NPACKED, ITERS);

    /* ================================================================
     * SCALAR PATH: operates on unpacked int16_t arrays
     *   Inner loop: lh, lh, mul, srai, (branch for sat), sh
     *   ~6 instructions per element
     * ================================================================ */
    t0 = rdcycle();
    for (iter = 0; iter < ITERS; iter++) {
        for (i = 0; i < N; i++) {
            result_scalar[i] = q15_mul(vec_a[i], vec_b[i]);
        }
    }
    t1 = rdcycle();
    uint32_t cycles_scalar = t1 - t0;

    /* Pack scalar results for comparison */
    for (i = 0; i < NPACKED; i++) {
        packed_result_scalar[i] =
            ((uint32_t)(uint16_t)result_scalar[2*i+1] << 16)
          | (uint16_t)result_scalar[2*i];
    }

    /* ================================================================
     * SCALAR-ON-PACKED PATH: scalar code working on packed arrays
     *   This is what you'd write without P extension on the same
     *   packed memory layout — extract, multiply, repack.
     *   Inner loop: lw, lw, extract×2, mul×2, shift×2, sat×2, repack, sw
     *   ~14 instructions per 2 elements = 7 insns/element
     * ================================================================ */
    uint32_t packed_result_scalar_on_packed[NPACKED];

    t0 = rdcycle();
    for (iter = 0; iter < ITERS; iter++) {
        for (i = 0; i < NPACKED; i++) {
            uint32_t wa = packed_a[i];
            uint32_t wb = packed_b[i];

            /* Extract lo elements */
            int16_t a_lo = (int16_t)(wa & 0xFFFF);
            int16_t b_lo = (int16_t)(wb & 0xFFFF);
            /* Extract hi elements */
            int16_t a_hi = (int16_t)(wa >> 16);
            int16_t b_hi = (int16_t)(wb >> 16);

            /* Scalar multiply each */
            int16_t r_lo = q15_mul(a_lo, b_lo);
            int16_t r_hi = q15_mul(a_hi, b_hi);

            /* Repack */
            packed_result_scalar_on_packed[i] =
                ((uint32_t)(uint16_t)r_hi << 16) | (uint16_t)r_lo;
        }
    }
    t1 = rdcycle();
    uint32_t cycles_scalar_packed = t1 - t0;

    uint32_t cycles_simd = 0;

    if (p_ext) {
        /* ================================================================
         * SIMD PATH: operates on pre-packed uint32_t arrays
         *   Inner loop: lw, lw, khm16, sw
         *   4 instructions per 2 elements = 2 insns/element
         * ================================================================ */
        t0 = rdcycle();
        for (iter = 0; iter < ITERS; iter++) {
            for (i = 0; i < NPACKED; i++) {
                packed_result_simd[i] = p_khm16(packed_a[i], packed_b[i]);
            }
        }
        t1 = rdcycle();
        cycles_simd = t1 - t0;

        /* ---- Validate all three paths ---- */
        int match_simd = 1, match_sop = 1;
        int mismatch_simd = 0, mismatch_sop = 0;

        for (i = 0; i < NPACKED; i++) {
            if (packed_result_scalar[i] != packed_result_simd[i]) {
                if (mismatch_simd < 3)
                    printf("  SIMD MISMATCH [%d]: expect=0x%08lx got=0x%08lx\n",
                           i, (unsigned long)packed_result_scalar[i],
                           (unsigned long)packed_result_simd[i]);
                match_simd = 0;
                mismatch_simd++;
            }
            if (packed_result_scalar[i] != packed_result_scalar_on_packed[i]) {
                if (mismatch_sop < 3)
                    printf("  SoP  MISMATCH [%d]: expect=0x%08lx got=0x%08lx\n",
                           i, (unsigned long)packed_result_scalar[i],
                           (unsigned long)packed_result_scalar_on_packed[i]);
                match_sop = 0;
                mismatch_sop++;
            }
        }

        printf("Validation:\n");
        printf("  SIMD vs Scalar:             %s\n", match_simd ? "PASS" : "FAIL");
        printf("  Scalar-on-Packed vs Scalar: %s\n\n", match_sop ? "PASS" : "FAIL");

        /* ---- Sample output ---- */
        printf("Sample results (first 16 elements):\n");
        printf("  Scalar:   ");
        for (i = 0; i < 8; i++)
            printf(" %6d %6d", lo16(packed_result_scalar[i]), hi16(packed_result_scalar[i]));
        printf("\n  SIMD:     ");
        for (i = 0; i < 8; i++)
            printf(" %6d %6d", lo16(packed_result_simd[i]), hi16(packed_result_simd[i]));
        printf("\n  Float:    ");
        for (i = 0; i < 8; i++)
            printf(" %6.3f %6.3f",
                   q15_to_float(lo16(packed_result_scalar[i])),
                   q15_to_float(hi16(packed_result_scalar[i])));
        printf("\n\n");

        /* ---- Saturation test ---- */
        uint32_t sat_in = 0x80008000u;  /* two Q.15(-1.0) packed */
        uint32_t sat_out = p_khm16(sat_in, sat_in);
        printf("Saturation test: packed(-1.0, -1.0) * packed(-1.0, -1.0)\n");
        printf("  Result: lo=%d hi=%d  (expect %d, %d)\n",
               lo16(sat_out), hi16(sat_out), INT16_MAX, INT16_MAX);
        printf("  %s\n\n", (sat_out == 0x7FFF7FFFu) ? "PASS" : "FAIL");
    } else {
        /* Validate scalar-on-packed vs scalar (no SIMD to compare) */
        int match_sop = 1;
        for (i = 0; i < NPACKED; i++) {
            if (packed_result_scalar[i] != packed_result_scalar_on_packed[i]) {
                match_sop = 0;
                break;
            }
        }
        printf("Validation:\n");
        printf("  Scalar-on-Packed vs Scalar: %s\n\n", match_sop ? "PASS" : "FAIL");

        printf("Sample results (first 16 elements):\n");
        printf("  Scalar:   ");
        for (i = 0; i < 8; i++)
            printf(" %6d %6d", lo16(packed_result_scalar[i]), hi16(packed_result_scalar[i]));
        printf("\n  Float:    ");
        for (i = 0; i < 8; i++)
            printf(" %6.3f %6.3f",
                   q15_to_float(lo16(packed_result_scalar[i])),
                   q15_to_float(hi16(packed_result_scalar[i])));
        printf("\n\n");
    }

    /* ---- Performance ---- */
    unsigned long total_ops = (unsigned long)N * ITERS;

    printf("=== Performance Results ===\n\n");

    printf("  %-28s %10s %10s %10s\n",
           "Path", "Cycles", "Cyc/Elem", "Insns/Elem(est)");
    printf("  %-28s %10s %10s %10s\n",
           "----------------------------", "----------", "----------", "---------------");
    printf("  %-28s %10lu %10.2f %10s\n",
           "Scalar (unpacked int16)",
           (unsigned long)cycles_scalar,
           (float)cycles_scalar / total_ops, "~6");
    printf("  %-28s %10lu %10.2f %10s\n",
           "Scalar-on-Packed (extract)",
           (unsigned long)cycles_scalar_packed,
           (float)cycles_scalar_packed / total_ops, "~7");

    if (p_ext) {
        printf("  %-28s %10lu %10.2f %10s\n",
               "SIMD32 khm16 (packed lw/sw)",
               (unsigned long)cycles_simd,
               (float)cycles_simd / total_ops, "~2");

        printf("\n  Speedup vs Scalar:            %.2fx\n",
               (float)cycles_scalar / (float)cycles_simd);
        printf("  Speedup vs Scalar-on-Packed:  %.2fx\n",
               (float)cycles_scalar_packed / (float)cycles_simd);

        if (cycles_simd > 0) {
            printf("\n  Cycle savings vs Scalar:           %ld (%.1f%%)\n",
                   (long)((long)cycles_scalar - (long)cycles_simd),
                   (1.0f - (float)cycles_simd / (float)cycles_scalar) * 100.0f);
            printf("  Cycle savings vs Scalar-on-Packed: %ld (%.1f%%)\n",
                   (long)((long)cycles_scalar_packed - (long)cycles_simd),
                   (1.0f - (float)cycles_simd / (float)cycles_scalar_packed) * 100.0f);
        }
    } else {
        printf("\n  (SIMD benchmark skipped — P extension not available)\n");
    }

    printf("\n=== Memory Access Analysis (per 2 elements) ===\n\n");
    printf("  %-28s %6s %6s %6s\n", "Path", "Loads", "Stores", "Total");
    printf("  %-28s %6s %6s %6s\n", "----------------------------", "------", "------", "------");
    printf("  %-28s %6s %6s %6s\n", "Scalar (unpacked int16)",   "4 lh",  "2 sh",  "6");
    printf("  %-28s %6s %6s %6s\n", "Scalar-on-Packed",          "2 lw",  "1 sw",  "3+extract");
    printf("  %-28s %6s %6s %6s\n", "SIMD32 khm16 (packed)",     "2 lw",  "1 sw",  "3");
    printf("\n  SIMD32 halves memory transactions vs unpacked scalar.\n");
    printf("  On real silicon this reduces cache pressure and bus stalls.\n");

    printf("\nDone.\n");
    return 0;
}
