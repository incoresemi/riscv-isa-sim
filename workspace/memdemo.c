/*
 * Memory placement demo — bare metal, no libc.
 *
 * Sections placed by memdemo.ld into regions from BlockDiagram_uArch.xlsx:
 *   .text         -> OFFCHIP_FLASH  (0x80000000)
 *   .rodata       -> OFFCHIP_FLASH  (0x80000000)
 *   .data         -> DTCM_RAM       (0x20000000)
 *   .bss          -> SRAM1          (0x20010000)
 *   .fast_code    -> ITCM_RAM       (0x00000000)
 *   .psram_data   -> OFFCHIP_PSRAM  (0x60000000)
 */

#include <stdint.h>

/* HTIF tohost/fromhost — defined in start.S */
extern volatile uint64_t tohost;
extern volatile uint64_t fromhost;

static void htif_putchar(char c)
{
    /* Wait for spike to consume previous command */
    while (tohost != 0)
        ;
    /* device=1 (terminal), cmd=1 (write), payload=char */
    tohost = ((uint64_t)1 << 56) | ((uint64_t)1 << 48) | (unsigned char)c;
    /* Write doesn't generate a fromhost response — just wait for tohost clear */
    while (tohost != 0)
        ;
}

static void print(const char *s)
{
    while (*s)
        htif_putchar(*s++);
}

static void print_hex(uintptr_t val)
{
    const char hex[] = "0123456789abcdef";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--)
        buf[2 + (15 - i)] = hex[(val >> (i * 4)) & 0xf];
    buf[18] = '\0';
    print(buf);
}

static void print_int(int val)
{
    if (val < 0) { htif_putchar('-'); val = -val; }
    char buf[12];
    int i = 0;
    do {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    } while (val > 0);
    while (i > 0)
        htif_putchar(buf[--i]);
}

/* ---------- .data -> DTCM_RAM ---------- */
int initialized_var = 0xDEAD;
int lookup_table[4] = {10, 20, 30, 40};

/* ---------- .rodata -> OFFCHIP_FLASH ---------- */
const char greeting[] = "=== Memory Placement Demo ===\n";
const int rom_constants[4] = {100, 200, 300, 400};

/* ---------- .bss -> SRAM1 ---------- */
int bss_counter;
int bss_array[8];

/* ---------- .fast_code -> ITCM_RAM ---------- */
__attribute__((section(".fast_code")))
int fast_multiply(int a, int b)
{
    return a * b;
}

__attribute__((section(".fast_code")))
int fast_accumulate(const int *data, int count)
{
    int sum = 0;
    for (int i = 0; i < count; i++)
        sum += data[i];
    return sum;
}

/* ---------- .psram_data -> OFFCHIP_PSRAM ---------- */
__attribute__((section(".psram_data")))
int psram_buffer[256];

/* ---------- main ---------- */
void main(void)
{
    print(greeting);
    print("Section addresses:\n");

    print("  main()           (FLASH) = "); print_hex((uintptr_t)&main); print("\n");
    print("  greeting[]       (FLASH) = "); print_hex((uintptr_t)greeting); print("\n");
    print("  initialized_var  (DTCM)  = "); print_hex((uintptr_t)&initialized_var); print("\n");
    print("  lookup_table[]   (DTCM)  = "); print_hex((uintptr_t)lookup_table); print("\n");
    print("  bss_counter      (SRAM1) = "); print_hex((uintptr_t)&bss_counter); print("\n");
    print("  bss_array[]      (SRAM1) = "); print_hex((uintptr_t)bss_array); print("\n");
    print("  fast_multiply()  (ITCM)  = "); print_hex((uintptr_t)&fast_multiply); print("\n");
    print("  fast_accumulate()(ITCM)  = "); print_hex((uintptr_t)&fast_accumulate); print("\n");
    print("  psram_buffer[]   (PSRAM) = "); print_hex((uintptr_t)psram_buffer); print("\n");

    /* Use everything to prevent optimization */
    bss_counter = fast_multiply(initialized_var, 2);
    int sum = fast_accumulate(lookup_table, 4);
    bss_array[0] = sum + rom_constants[0];
    psram_buffer[0] = bss_array[0] + bss_counter;

    print("\nComputed values:\n");
    print("  fast_multiply(0xDEAD, 2) = "); print_hex(bss_counter); print("\n");
    print("  accumulate(lookup)       = "); print_int(sum); print("\n");
    print("  psram_buffer[0]          = "); print_hex(psram_buffer[0]); print("\n");
    print("\nDone.\n");
}
