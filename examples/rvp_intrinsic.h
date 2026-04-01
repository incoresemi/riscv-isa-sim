/*
 * rvp_intrinsic.h — RISC-V P-extension (Packed SIMD) intrinsics
 *
 * Standalone header using inline assembly (.insn) — works with any
 * RISC-V GCC, no compiler builtin support required.
 *
 * API matches the InCore/GCC __rv_* naming convention so code is
 * portable between this header and a P-extension-aware GCC.
 *
 * On RV32: each intrinsic operates on 32-bit registers containing
 *   - 2 x int16 (16-bit SIMD) or
 *   - 4 x int8  (8-bit SIMD)
 *
 * Encoding reference: .insn r opcode, funct3, funct7, rd, rs1, rs2
 *   All P-extension packed SIMD use opcode=0x77
 */

#ifndef _RVP_INTRINSIC_H
#define _RVP_INTRINSIC_H

#include <stdint.h>

/* ========================================================================
 *  Helper macro: generates an inline function wrapping a .insn r encoding
 * ======================================================================== */

#define _RVP_INSN_RRR(name, funct7, funct3)                            \
    static inline uint32_t __rv_##name(uint32_t rs1, uint32_t rs2) {   \
        uint32_t rd;                                                    \
        __asm__ volatile (                                              \
            ".insn r 0x77, " #funct3 ", " #funct7 ", %0, %1, %2"      \
            : "=r"(rd) : "r"(rs1), "r"(rs2));                          \
        return rd;                                                      \
    }

/* 3-operand accumulate: rd = f(rs1, rs2) uses rd as input+output */
#define _RVP_INSN_RRR_ACC(name, funct7, funct3)                        \
    static inline uint32_t __rv_##name(uint32_t rd_in, uint32_t rs1,   \
                                       uint32_t rs2) {                  \
        uint32_t rd = rd_in;                                            \
        __asm__ volatile (                                              \
            ".insn r 0x77, " #funct3 ", " #funct7 ", %0, %1, %2"      \
            : "+r"(rd) : "r"(rs1), "r"(rs2));                          \
        return rd;                                                      \
    }

/* Unary (rs2 encoded in funct7, e.g., clrs16, clz16) */
#define _RVP_INSN_RR(name, match_hi, funct3)                           \
    static inline uint32_t __rv_##name(uint32_t rs1) {                 \
        uint32_t rd;                                                    \
        __asm__ volatile (                                              \
            ".insn i 0x77, " #funct3 ", %0, %1, " #match_hi           \
            : "=r"(rd) : "r"(rs1));                                    \
        return rd;                                                      \
    }

/* ========================================================================
 *  16-bit SIMD Arithmetic (2 x int16 packed in uint32)
 * ======================================================================== */

/* --- Addition / Subtraction --- */
_RVP_INSN_RRR(add16,    0x20, 0)  /* rd[15:0]  = rs1[15:0]  + rs2[15:0]   */
                                    /* rd[31:16] = rs1[31:16] + rs2[31:16]  */
_RVP_INSN_RRR(sub16,    0x21, 0)  /* SIMD 16-bit subtract                  */
_RVP_INSN_RRR(kadd16,   0x08, 0)  /* Saturating add (signed)               */
_RVP_INSN_RRR(ksub16,   0x09, 0)  /* Saturating subtract (signed)          */
_RVP_INSN_RRR(radd16,   0x00, 0)  /* Rounding average: (a + b) >> 1        */
_RVP_INSN_RRR(rsub16,   0x01, 0)  /* Rounding subtract: (a - b) >> 1       */

/* --- Unsigned saturating --- */
_RVP_INSN_RRR(ukadd16,  0x18, 0)  /* Unsigned saturating add               */
_RVP_INSN_RRR(uksub16,  0x19, 0)  /* Unsigned saturating subtract           */
_RVP_INSN_RRR(uradd16,  0x10, 0)  /* Unsigned rounding average              */
_RVP_INSN_RRR(ursub16,  0x11, 0)  /* Unsigned rounding subtract             */

/* --- Multiply --- */
_RVP_INSN_RRR(khm16,    0x43, 0)  /* Q.15 multiply: sat((a*b) >> 15)       */
_RVP_INSN_RRR(khmx16,   0x4b, 0)  /* Q.15 cross multiply                   */

/* --- Compare / Min / Max --- */
_RVP_INSN_RRR(smax16,   0x41, 0)  /* Signed max of each 16-bit element     */
_RVP_INSN_RRR(smin16,   0x40, 0)  /* Signed min of each 16-bit element     */
_RVP_INSN_RRR(umax16,   0x49, 0)  /* Unsigned max                          */
_RVP_INSN_RRR(umin16,   0x48, 0)  /* Unsigned min                          */
_RVP_INSN_RRR(scmpeq16, 0x26, 0)  /* Signed compare equal (0xFFFF / 0x0000)*/

/* --- Shift --- */
_RVP_INSN_RRR(sra16,    0x28, 0)  /* Arithmetic shift right                */
_RVP_INSN_RRR(sll16,    0x2a, 0)  /* Logical shift left                    */
_RVP_INSN_RRR(srl16,    0x29, 0)  /* Logical shift right                   */
_RVP_INSN_RRR(ksll16,   0x2c, 0)  /* Saturating shift left                 */

/* --- Pack --- */
_RVP_INSN_RRR(pkbb16,   0x07, 1)  /* Pack bottom-bottom: {rs2[15:0], rs1[15:0]}  */
_RVP_INSN_RRR(pkbt16,   0x0f, 1)  /* Pack bottom-top:    {rs2[31:16], rs1[15:0]} */
_RVP_INSN_RRR(pktb16,   0x1f, 1)  /* Pack top-bottom:    {rs2[15:0], rs1[31:16]} */
_RVP_INSN_RRR(pktt16,   0x17, 1)  /* Pack top-top:       {rs2[31:16], rs1[31:16]}*/

/* ========================================================================
 *  8-bit SIMD Arithmetic (4 x int8 packed in uint32)
 * ======================================================================== */

_RVP_INSN_RRR(add8,     0x24, 0)  /* SIMD 8-bit add                        */
_RVP_INSN_RRR(sub8,     0x25, 0)  /* SIMD 8-bit subtract                   */
_RVP_INSN_RRR(kadd8,    0x0c, 0)  /* Saturating 8-bit add                  */
_RVP_INSN_RRR(ksub8,    0x0d, 0)  /* Saturating 8-bit subtract             */
_RVP_INSN_RRR(smax8,    0x45, 0)  /* Signed 8-bit max                      */
_RVP_INSN_RRR(smin8,    0x44, 0)  /* Signed 8-bit min                      */
_RVP_INSN_RRR(umax8,    0x4d, 0)  /* Unsigned 8-bit max                    */
_RVP_INSN_RRR(umin8,    0x4c, 0)  /* Unsigned 8-bit min                    */

/* ========================================================================
 *  Convenience: pack / unpack helpers (pure C, no asm needed)
 * ======================================================================== */

static inline uint32_t __rv_pack16(int16_t lo, int16_t hi) {
    return ((uint32_t)(uint16_t)hi << 16) | (uint16_t)lo;
}

static inline int16_t __rv_lo16(uint32_t packed) {
    return (int16_t)(packed & 0xFFFF);
}

static inline int16_t __rv_hi16(uint32_t packed) {
    return (int16_t)(packed >> 16);
}

static inline uint32_t __rv_pack8(int8_t b0, int8_t b1, int8_t b2, int8_t b3) {
    return ((uint32_t)(uint8_t)b3 << 24) | ((uint32_t)(uint8_t)b2 << 16) |
           ((uint32_t)(uint8_t)b1 << 8)  | (uint8_t)b0;
}

#endif /* _RVP_INTRINSIC_H */
