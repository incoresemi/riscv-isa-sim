# ============================================================================
#  RISC-V Spike Examples Makefile
#
#  Works with the riscv-toolchain .deb package (installed to /opt/riscv)
#  or any prefix where spike, pk, and cross-compilers are installed.
#
#  Usage:
#    make                        # Build all examples
#    make run-all                # Build and run all
#    make PREFIX=/path/to/riscv  # Use a custom toolchain prefix
#
#  Targets are grouped by execution mode:
#    Bare-metal RV64  — run with: spike --isa=... <prog>.elf
#    PK RV64          — run with: spike pk <prog>
#    PK RV32 + P-ext  — run with: spike --isa=RV32...P... pk32 <prog>
# ============================================================================

# --- Toolchain (multilib: single compiler targets both RV64 and RV32) ---
# Auto-detect: if spike is in PATH, derive prefix; otherwise /opt/riscv
SPIKE_PATH := $(shell command -v spike 2>/dev/null)
ifdef SPIKE_PATH
  PREFIX ?= $(dir $(SPIKE_PATH))..
else
  PREFIX ?= /opt/riscv
endif

GCC         = $(PREFIX)/bin/riscv64-unknown-elf-gcc
SPIKE       = $(PREFIX)/bin/spike
PK64        = $(PREFIX)/riscv64-unknown-elf/bin/pk
PK32        = $(PREFIX)/riscv32-unknown-elf/bin/pk

# --- Flags ---
# -I. finds headers in the local examples dir; -I$(PREFIX)/include finds
# headers installed by the .deb package (e.g., rvp_intrinsic.h)
INCLUDES      = -I. -I$(PREFIX)/include
RV64_ASFLAGS  = -nostdlib -nostartfiles -Wl,-n,-Ttext,0x80000000
RV64_CFLAGS   = -O2 $(INCLUDES)
RV32_CFLAGS   = -O2 -march=rv32gc -mabi=ilp32d $(INCLUDES)

# --- Spike ISA strings ---
ISA_BAREMETAL = RV64IMAFDCP_zicntr_zihpm
ISA_RV32_P    = RV32IMAFDCP_zicntr_zihpm
ISA_RV32_NOP  = RV32IMAFDC_zicntr_zihpm

# --- Bare-metal memory map (from spike.cfg) ---
BAREMETAL_MEM = 0x20004000:0x00004000,0x20000000:0x00004000,0x20010000:0x00010000,0x00200000:0x00004000,0x80000000:0x00100000,0x60000000:0x00100000

# --- PK memory (single large region) ---
PK_MEM        = 0x80000000:0x10000000

# ============================================================================
#  Build targets
# ============================================================================

# -- Bare-metal RV64 (assembly, custom linker scripts) --
BAREMETAL_ASM  = exitonly.elf chartest.elf storetest.elf
BAREMETAL_C    = memdemo.elf

# -- PK RV64 --
PK64_PROGS     = q15demo hello

# -- PK RV32 (P-extension packed SIMD32 benchmarks) --
PK32_PROGS     = q15bench q15packed p32_simd_demo

ALL = $(BAREMETAL_ASM) $(BAREMETAL_C) $(PK64_PROGS) $(PK32_PROGS)

.PHONY: all clean help
.PHONY: run-baremetal run-pk64 run-pk32 run-all
.PHONY: run-exitonly run-chartest run-storetest run-memdemo
.PHONY: run-hello run-q15demo run-q15bench run-q15packed run-p32demo
.PHONY: run-q15bench-nop run-q15packed-nop run-p32demo-nop

all: $(ALL)

# ============================================================================
#  Bare-metal RV64: assembly programs
# ============================================================================

exitonly.elf: exitonly.S
	$(GCC) $(RV64_ASFLAGS) -o $@ $<

chartest.elf: chartest.S
	$(GCC) $(RV64_ASFLAGS) -o $@ $<

storetest.elf: storetest.S
	$(GCC) $(RV64_ASFLAGS) -o $@ $<

# ============================================================================
#  Bare-metal RV64: C with custom startup + linker script
# ============================================================================

memdemo.elf: memdemo.c start.S memdemo.ld
	$(GCC) $(RV64_CFLAGS) -mcmodel=medany -nostdlib -nostartfiles \
		-T memdemo.ld -o $@ start.S memdemo.c

# ============================================================================
#  PK RV64: C programs using proxy kernel (printf, etc.)
# ============================================================================

hello: hello.c
	$(GCC) $(RV64_CFLAGS) -o $@ $<

q15demo: q15demo.c
	$(GCC) $(RV64_CFLAGS) -o $@ $<

# ============================================================================
#  PK RV32 + P-extension: packed SIMD32 benchmarks
# ============================================================================

q15bench: q15bench.c
	$(GCC) $(RV32_CFLAGS) -o $@ $<

q15packed: q15packed.c
	$(GCC) $(RV32_CFLAGS) -o $@ $<

p32_simd_demo: p32_simd_demo.c rvp_intrinsic.h
	$(GCC) $(RV32_CFLAGS) -o $@ $<

# ============================================================================
#  Run targets — bare-metal (use spike.cfg memory map, no pk)
# ============================================================================

run-exitonly: exitonly.elf
	$(SPIKE) --isa=$(ISA_BAREMETAL) -m$(BAREMETAL_MEM) $<

run-chartest: chartest.elf
	$(SPIKE) --isa=$(ISA_BAREMETAL) -m$(BAREMETAL_MEM) $<

run-storetest: storetest.elf
	$(SPIKE) --isa=$(ISA_BAREMETAL) -m$(BAREMETAL_MEM) $<

run-memdemo: memdemo.elf
	$(SPIKE) --isa=$(ISA_BAREMETAL) -m$(BAREMETAL_MEM) $<

run-baremetal: run-exitonly
# Note: chartest/memdemo use HTIF putchar which hangs on exit; run separately.
# Note: storetest intentionally accesses unmapped memory; run separately.

# ============================================================================
#  Run targets — PK RV64
# ============================================================================

run-hello: hello
	$(SPIKE) $(PK64) $<

run-q15demo: q15demo
	$(SPIKE) --isa=$(ISA_BAREMETAL) $(PK64) $<

run-pk64: run-hello run-q15demo

# ============================================================================
#  Run targets — PK RV32 with P extension
# ============================================================================

run-q15bench: q15bench
	$(SPIKE) --isa=$(ISA_RV32_P) -m$(PK_MEM) $(PK32) $<

run-q15packed: q15packed
	$(SPIKE) --isa=$(ISA_RV32_P) -m$(PK_MEM) $(PK32) $<

run-pk32: run-q15bench run-q15packed

# ============================================================================
#  Run targets — PK RV32 WITHOUT P extension (scalar only)
# ============================================================================

run-q15bench-nop: q15bench
	@echo "=== Running WITHOUT P extension (scalar only) ==="
	$(SPIKE) --isa=$(ISA_RV32_NOP) -m$(PK_MEM) $(PK32) $< --no-simd

run-p32demo: p32_simd_demo
	$(SPIKE) --isa=$(ISA_RV32_P) -m$(PK_MEM) $(PK32) $<

run-q15packed-nop: q15packed
	@echo "=== Running WITHOUT P extension (scalar only) ==="
	$(SPIKE) --isa=$(ISA_RV32_NOP) -m$(PK_MEM) $(PK32) $< --no-simd

run-p32demo-nop: p32_simd_demo
	@echo "=== Running WITHOUT P extension ==="
	$(SPIKE) --isa=$(ISA_RV32_NOP) -m$(PK_MEM) $(PK32) $< --no-simd

# ============================================================================
#  Convenience: run everything
# ============================================================================

run-all: run-baremetal run-pk64 run-pk32

# ============================================================================
#  Clean
# ============================================================================

clean:
	rm -f $(ALL) *.map.html

# ============================================================================
#  Help
# ============================================================================

help:
	@echo "Build targets:"
	@echo "  all              Build everything"
	@echo "  hello            PK RV64: Hello World"
	@echo "  exitonly.elf     Bare-metal RV64: minimal exit test"
	@echo "  chartest.elf     Bare-metal RV64: HTIF character output"
	@echo "  storetest.elf    Bare-metal RV64: memory store test"
	@echo "  memdemo.elf      Bare-metal RV64: memory placement demo"
	@echo "  q15demo          PK RV64: Q.15 SIMD demo (khm16)"
	@echo "  q15bench         PK RV32: Q.15 packed SIMD32 benchmark"
	@echo "  q15packed        PK RV32: Q.15 packed-memory SIMD32 benchmark"
	@echo "  p32_simd_demo    PK RV32: P-extension intrinsics demo"
	@echo ""
	@echo "Run targets:"
	@echo "  run-hello        Run Hello World on RV64 with pk"
	@echo "  run-baremetal    Run all bare-metal programs"
	@echo "  run-q15demo      Run Q.15 demo on RV64 with pk"
	@echo "  run-pk64         Run all RV64 pk programs"
	@echo "  run-pk32         Run all RV32 P-ext benchmarks"
	@echo "  run-q15bench-nop Run q15bench WITHOUT P ext (scalar only)"
	@echo "  run-p32demo      Run P-extension intrinsics demo"
	@echo "  run-q15packed-nop Run q15packed WITHOUT P ext (scalar only)"
	@echo "  run-p32demo-nop  Run p32demo WITHOUT P ext"
	@echo "  run-all          Run everything"
	@echo ""
	@echo "  clean            Remove built binaries"
	@echo ""
	@echo "Configuration:"
	@echo "  PREFIX=$(PREFIX)"
	@echo "  SPIKE=$(SPIKE)"
	@echo "  PK64=$(PK64)"
	@echo "  PK32=$(PK32)"
