    .section .text._start
    .global _start
_start:
    /* Try to store to DTCM at 0x20000000 */
    li      t0, 0x20000000
    li      t1, 0x12345678
    sw      t1, 0(t0)

    /* Try to store to 0x10000000 */
    li      t0, 0x10000000
    li      t1, 0xAABBCCDD
    sw      t1, 0(t0)

    /* Exit via HTIF */
    la      t0, tohost
    li      t1, 1
    sd      t1, 0(t0)
1:  j       1b

    .section .htif, "aw", @progbits
    .align 3
    .global tohost
    .global fromhost
tohost:   .dword 0
fromhost: .dword 0
