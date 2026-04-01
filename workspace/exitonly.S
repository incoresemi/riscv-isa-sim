    .section .text._start
    .global _start
_start:
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
