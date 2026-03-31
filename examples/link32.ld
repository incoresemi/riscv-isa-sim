/* Bare-metal RV32 linker script for Spike */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 16M
}

SECTIONS
{
    . = 0x80000000;

    .text : { *(.text._start) *(.text .text.*) } > RAM
    .rodata : { *(.rodata .rodata.*) } > RAM
    .data : { *(.data .data.*) } > RAM

    .htif ALIGN(8) : { *(.htif) } > RAM

    __bss_start = .;
    .bss : { *(.bss .bss.* COMMON) } > RAM
    __bss_end = .;

    /DISCARD/ : { *(.comment) *(.eh_frame) }
}
