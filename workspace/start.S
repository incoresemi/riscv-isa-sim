/*
 * Bare-metal startup for Spike memory placement demo.
 */
    .section .text._start
    .global _start
_start:
    /* Set up global pointer (not strictly needed but good practice) */
    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop

    /* Set up stack pointer at top of SRAM1 */
    la      sp, __stack_top

    /* Copy .fast_code from FLASH (LMA) to ITCM (VMA) */
    la      a0, __fast_code_start
    la      a1, __fast_code_end
    la      a2, __fast_code_load
    beq     a0, a1, .Lskip_fast
.Lcopy_fast:
    ld      t0, 0(a2)
    sd      t0, 0(a0)
    addi    a0, a0, 8
    addi    a2, a2, 8
    blt     a0, a1, .Lcopy_fast
.Lskip_fast:

    /* Copy .data from FLASH (LMA) to DTCM (VMA) */
    la      a0, __data_start
    la      a1, __data_end
    la      a2, __data_load
    beq     a0, a1, .Lskip_data
.Lcopy_data:
    ld      t0, 0(a2)
    sd      t0, 0(a0)
    addi    a0, a0, 8
    addi    a2, a2, 8
    blt     a0, a1, .Lcopy_data
.Lskip_data:

    /* Zero .bss in SRAM1 */
    la      a0, __bss_start
    la      a1, __bss_end
    beq     a0, a1, .Lskip_bss
.Lzero_bss:
    sd      zero, 0(a0)
    addi    a0, a0, 8
    blt     a0, a1, .Lzero_bss
.Lskip_bss:

    /* Call main */
    call    main

    /* Exit spike: write 1 to tohost (encodes "exit code 0") */
    la      t0, tohost
    li      t1, 1
    sd      t1, 0(t0)
.Lhang:
    j       .Lhang

    /* HTIF interface — spike finds these symbols */
    .section .htif, "aw", @progbits
    .align 3
    .global tohost
    .global fromhost
tohost:   .dword 0
fromhost: .dword 0
