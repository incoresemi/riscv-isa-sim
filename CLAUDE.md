# CLAUDE.md — Spike RISC-V ISA Simulator

## Project Overview

Fork of [riscv-isa-sim](https://github.com/riscv-software-src/riscv-isa-sim) (Spike) with Docker-based build infrastructure and workspace for RISC-V extension evaluation.

## Build Infrastructure

### Docker (builds everything from source)
```bash
./spike-run.sh build --prefix ~/.local   # Build Docker image with RV64+RV32 toolchains
./spike-run.sh install                   # Install to host at ~/.local
```

### Host tools (after install)
```bash
export PATH="$HOME/.local/bin:$PATH"
# Available: spike, riscv64-unknown-elf-gcc, riscv32-unknown-elf-gcc
# PK: ~/.local/riscv{32,64}-unknown-elf/bin/pk
```

### Workspace Makefile
```bash
./spike-run.sh make              # Build all workspace examples
./spike-run.sh make run-all      # Build and run all
./spike-run.sh make <target>     # Build/run specific target
```

## Configuration

- **spike.cfg** — Custom SoC memory map (ITCM, DTCM, SRAM1, Flash, PSRAM). For bare-metal only.
- **spike-pk.cfg** — RV32 + P extension, single 256MB region at 0x80000000. For pk runs.
- **spike-pk-nop.cfg** — Same as above but without P extension (baseline comparison).

**Important:** pk requires a simple memory layout. The custom SoC map in spike.cfg will hang pk.

## ISA Extensions

Enable/disable extensions by editing the ISA string in the config file:
- With P: `RV32IMAFDCP_zicntr_zihpm`
- Without P: `RV32IMAFDC_zicntr_zihpm`

## Workspace Examples

| File | Arch | Mode | Description |
|------|------|------|-------------|
| exitonly.S | RV64 | bare-metal | Minimal HTIF exit test |
| chartest.S | RV64 | bare-metal | HTIF character output |
| storetest.S | RV64 | bare-metal | Memory store test (hits unmapped regions) |
| memdemo.c + start.S + memdemo.ld | RV64 | bare-metal | Memory placement demo across SoC regions |
| q15demo.c | RV64 | pk | Q.15 packed SIMD demo (khm16, 4 elems/insn) |
| q15bench.c | RV32 | pk | Q.15 packed SIMD32 benchmark (khm16, 2 elems/insn) |
| q15packed.c | RV32 | pk | Q.15 packed-memory benchmark (realistic load amortization) |

## Key Branch

- `1-docker-infrastructure-to-build-and-test` — Active development branch with all infrastructure
