/*
 * Bare-metal RV32 startup with HTIF character output for Spike.
 */
    .section .text._start
    .global _start
_start:
    /* Set stack pointer (top of memory at 0x80000000 + 16MB) */
    li      sp, 0x81000000

    /* Zero .bss */
    la      a0, __bss_start
    la      a1, __bss_end
    beq     a0, a1, .Lskip_bss
.Lzero_bss:
    sw      zero, 0(a0)
    addi    a0, a0, 4
    blt     a0, a1, .Lzero_bss
.Lskip_bss:

    /* Call main */
    call    main

    /* Exit spike: write 1 to tohost */
    la      t0, tohost
    li      t1, 1
    sw      t1, 0(t0)
    sw      zero, 4(t0)
.Lhang:
    j       .Lhang

/*
 * _putchar: write a single character via HTIF.
 * HTIF tohost device=1, cmd=1 (console putchar):
 *   tohost = (1 << 56) | (1 << 48) | char
 * On RV32, tohost is 64-bit at a known address.
 * We write the low word first, then the high word.
 *
 * a0 = character to output
 */
    .global _putchar
_putchar:
    la      t0, tohost

    /* Wait for tohost to be 0 (previous command consumed) */
.Lwait:
    lw      t1, 0(t0)
    lw      t2, 4(t0)
    or      t1, t1, t2
    bnez    t1, .Lwait

    /* Write: low word = char, high word = 0x01010000 (device=1, cmd=1) */
    /* high word: (1 << 24) | (1 << 16) = 0x01010000 */
    sw      a0, 0(t0)          /* low 32 bits: the character */
    li      t1, 0x01010000
    sw      t1, 4(t0)          /* high 32 bits: device=1, cmd=1 */

    /* Wait for fromhost acknowledgement */
    la      t0, fromhost
.Lwait_ack:
    lw      t1, 0(t0)
    lw      t2, 4(t0)
    or      t1, t1, t2
    beqz    t1, .Lwait_ack

    /* Clear fromhost */
    sw      zero, 0(t0)
    sw      zero, 4(t0)

    ret

    /* HTIF interface */
    .section .htif, "aw", @progbits
    .align 3
    .global tohost
    .global fromhost
tohost:   .word 0; .word 0
fromhost: .word 0; .word 0
