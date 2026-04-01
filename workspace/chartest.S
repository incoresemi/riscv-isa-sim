    .section .text._start
    .global _start
_start:
    /* Print 'H' via HTIF: device=1, cmd=1, payload=0x48 */
    la      t0, tohost
    la      t1, fromhost

    /* Wait for tohost == 0 */
1:  ld      t2, 0(t0)
    bnez    t2, 1b

    /* Write: (1 << 56) | (1 << 48) | 'H' */
    li      t2, 0x0101000000000048
    sd      t2, 0(t0)

    /* Wait for fromhost != 0 */
2:  ld      t2, 0(t1)
    beqz    t2, 2b

    /* Clear fromhost */
    sd      zero, 0(t1)

    /* Print newline */
3:  ld      t2, 0(t0)
    bnez    t2, 3b
    li      t2, 0x010100000000000a
    sd      t2, 0(t0)
4:  ld      t2, 0(t1)
    beqz    t2, 4b
    sd      zero, 0(t1)

    /* Exit */
5:  ld      t2, 0(t0)
    bnez    t2, 5b
    li      t2, 1
    sd      t2, 0(t0)
6:  j       6b

    .section .htif, "aw", @progbits
    .align 3
    .global tohost
    .global fromhost
tohost:   .dword 0
fromhost: .dword 0
