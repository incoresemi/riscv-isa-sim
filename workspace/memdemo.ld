/*
 * Linker script for Spike memory placement demo.
 * Memory regions match spike.cfg (from BlockDiagram_uArch.xlsx MemoryMap).
 */

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    ITCM_RAM      (rwx) : ORIGIN = 0x20004000, LENGTH = 16K
    ONCHIP_ROM    (rx)  : ORIGIN = 0x00200000, LENGTH = 16K
    DTCM_RAM      (rw)  : ORIGIN = 0x20000000, LENGTH = 16K
    SRAM1         (rw)  : ORIGIN = 0x20010000, LENGTH = 64K
    OFFCHIP_PSRAM (rw)  : ORIGIN = 0x60000000, LENGTH = 1M
    OFFCHIP_FLASH (rwx) : ORIGIN = 0x80000000, LENGTH = 1M
}

SECTIONS
{
    /* ---- Code in FLASH ---- */
    .text : {
        *(.text._start)
        *(.text .text.*)
    } > OFFCHIP_FLASH

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } > OFFCHIP_FLASH

    /* ---- Fast code: stored in FLASH, copied to ITCM at boot ---- */
    .fast_code : ALIGN(8) {
        __fast_code_start = .;
        *(.fast_code)
        . = ALIGN(8);
        __fast_code_end = .;
    } > ITCM_RAM AT> OFFCHIP_FLASH

    __fast_code_load = LOADADDR(.fast_code);

    /* ---- Initialized data: stored in FLASH, copied to DTCM at boot ---- */
    .data : ALIGN(8) {
        __data_start = .;
        *(.data .data.*)
        *(.sdata .sdata.*)
        . = ALIGN(8);
        __data_end = .;
    } > DTCM_RAM AT> OFFCHIP_FLASH

    __data_load = LOADADDR(.data);

    /* Global pointer for linker relaxation (middle of DTCM) */
    PROVIDE(__global_pointer$ = ORIGIN(DTCM_RAM) + 0x800);

    /* ---- Zero-initialized data in SRAM1 ---- */
    .bss (NOLOAD) : ALIGN(8) {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > SRAM1

    /* ---- HTIF (spike host interface) in SRAM1 ---- */
    .htif (NOLOAD) : ALIGN(8) {
        *(.htif)
    } > SRAM1

    /* ---- Large buffers in PSRAM ---- */
    .psram_data (NOLOAD) : ALIGN(8) {
        *(.psram_data)
    } > OFFCHIP_PSRAM

    /* ---- Stack at top of SRAM1 ---- */
    __stack_top = ORIGIN(SRAM1) + LENGTH(SRAM1);
}
