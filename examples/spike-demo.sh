#!/usr/bin/env bash
set -euo pipefail

# ============================================================================
#  spike-demo.sh — Run RISC-V examples with the installed Spike toolchain
#
#  Works with the toolchain installed via the riscv-toolchain .deb package
#  or any installation where spike, pk, and cross-compilers are in PATH
#  or under a known prefix.
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CFG_PK32="$SCRIPT_DIR/spike-pk.cfg"
CFG_PK64="$SCRIPT_DIR/spike-pk64.cfg"
CFG_BAREMETAL="$SCRIPT_DIR/spike.cfg"
DEFAULT_CFG=""  # auto-detect based on ELF

# --- Locate toolchain prefix ---
# Try: 1) explicit --prefix, 2) RISCV env var, 3) /opt/riscv, 4) PATH lookup
find_prefix() {
    # If spike is already in PATH, derive prefix from its location
    local spike_path
    if spike_path="$(command -v spike 2>/dev/null)"; then
        # spike is at <prefix>/bin/spike
        dirname "$(dirname "$spike_path")"
        return
    fi

    # Try standard install locations
    for candidate in /opt/riscv "$HOME/.local"; do
        if [ -x "$candidate/bin/spike" ]; then
            echo "$candidate"
            return
        fi
    done

    echo ""
}

PREFIX=""

usage() {
    cat <<EOF
Usage: $0 <command> [args...]

Commands:
  gcc [--rv32] <source.c> [-o out] [flags...]
                             Compile a C program (--rv32 for 32-bit target)
  run [--cfg <file>] <prog>  Run an ELF binary on Spike (bare-metal, no pk)
  pk  [--cfg <file>] <prog> [args...]
                             Run a binary on Spike with proxy kernel
  make [target...]           Build and/or run examples via Makefile
  show-cmd [--cfg <file>]    Show the spike command that would be generated
  help                       Show this help message

Options:
  --prefix <path>            Override toolchain prefix (default: auto-detect)

Config file:
  The 'pk' command auto-detects RV32/RV64 from the ELF and selects the
  right config (spike-pk.cfg or spike-pk64.cfg). Override with --cfg.
  The 'run' command defaults to spike.cfg (bare-metal memory map).
  Format: key = value (one per line, # for comments)

Examples:
  $0 gcc hello.c                        # Compile for RV64
  $0 gcc --rv32 hello.c                 # Compile for RV32
  $0 pk hello                           # Run with default config (RV32+P)
  $0 pk --cfg spike-pk-nop.cfg hello    # Run without P extension
  $0 run --cfg spike.cfg prog.elf       # Run bare-metal ELF
  $0 make                               # Build all examples
  $0 make run-all                       # Build and run all
  $0 make run-q15bench                  # Run specific benchmark
  $0 show-cmd --cfg spike.cfg           # Preview the spike command line

Toolchain detection (in order):
  1. --prefix <path> argument
  2. RISCV environment variable
  3. 'spike' found in PATH
  4. /opt/riscv
  5. ~/.local
EOF
    exit 1
}

ensure_prefix() {
    if [ -z "$PREFIX" ]; then
        PREFIX="$(find_prefix)"
    fi
    if [ -z "$PREFIX" ] || [ ! -x "$PREFIX/bin/spike" ]; then
        echo "Error: Cannot find spike. Install the riscv-toolchain package or set --prefix."
        echo ""
        echo "  sudo dpkg -i riscv-toolchain_*.deb"
        echo "  # or"
        echo "  $0 --prefix /path/to/riscv <command>"
        exit 1
    fi
}

# Parse a config file into spike command-line arguments.
parse_config() {
    local cfg_file="$1"
    local -a args=()

    if [ ! -f "$cfg_file" ]; then
        echo "Error: Config file '$cfg_file' not found." >&2
        exit 1
    fi

    local -A config=()
    local -a mem_regions=()
    local -a extensions=()
    local -a extlibs=()
    local -a devices=()

    while IFS= read -r line; do
        # Strip leading/trailing whitespace
        line="${line#"${line%%[![:space:]]*}"}"
        line="${line%"${line##*[![:space:]]}"}"

        # Skip empty lines and comments
        [[ -z "$line" || "$line" == \#* ]] && continue

        # Split on first '='
        local key="${line%%=*}"
        local val="${line#*=}"

        # Trim whitespace from key and value
        key="${key#"${key%%[![:space:]]*}"}"
        key="${key%"${key##*[![:space:]]}"}"
        val="${val#"${val%%[![:space:]]*}"}"
        val="${val%"${val##*[![:space:]]}"}"

        # Strip inline comments
        if [[ "$val" =~ ^([^#]*[^[:space:]])[[:space:]]+#.*$ ]]; then
            val="${BASH_REMATCH[1]}"
        fi

        # Collect multi-value keys
        case "$key" in
            memory|memory\ *)
                mem_regions+=("$val")
                ;;
            extension) extensions+=("$val") ;;
            extlib)    extlibs+=("$val") ;;
            device)    devices+=("$val") ;;
            *)         config["$key"]="$val" ;;
        esac
    done < "$cfg_file"

    # --- Build spike arguments ---
    [[ -n "${config[isa]:-}" ]] && args+=(--isa="${config[isa]}")
    [[ -n "${config[priv]:-}" ]] && args+=(--priv="${config[priv]}")
    [[ -n "${config[varch]:-}" ]] && args+=(--varch="${config[varch]}")
    [[ -n "${config[processors]:-}" && "${config[processors]}" != "1" ]] && \
        args+=(-p"${config[processors]}")
    [[ -n "${config[hartids]:-}" ]] && args+=(--hartids="${config[hartids]}")
    [[ -n "${config[pc]:-}" ]] && args+=(--pc="${config[pc]}")

    if [[ ${#mem_regions[@]} -gt 0 ]]; then
        local mem_arg=""
        for region in "${mem_regions[@]}"; do
            [[ -n "$mem_arg" ]] && mem_arg+=","
            mem_arg+="$region"
        done
        args+=(-m"$mem_arg")
    fi

    [[ "${config[big_endian]:-false}" == "true" ]] && args+=(--big-endian)
    [[ "${config[misaligned]:-false}" == "true" ]] && args+=(--misaligned)
    [[ -n "${config[pmpregions]:-}" && "${config[pmpregions]}" != "16" ]] && \
        args+=(--pmpregions="${config[pmpregions]}")
    [[ -n "${config[pmpgranularity]:-}" && "${config[pmpgranularity]}" != "4" ]] && \
        args+=(--pmpgranularity="${config[pmpgranularity]}")
    [[ -n "${config[ic]:-}" ]] && args+=(--ic="${config[ic]}")
    [[ -n "${config[dc]:-}" ]] && args+=(--dc="${config[dc]}")
    [[ -n "${config[l2]:-}" ]] && args+=(--l2="${config[l2]}")
    [[ -n "${config[blocksz]:-}" && "${config[blocksz]}" != "64" ]] && \
        args+=(--blocksz="${config[blocksz]}")
    [[ -n "${config[triggers]:-}" && "${config[triggers]}" != "4" ]] && \
        args+=(--triggers="${config[triggers]}")
    [[ -n "${config[kernel]:-}" ]] && args+=(--kernel="${config[kernel]}")
    [[ -n "${config[initrd]:-}" ]] && args+=(--initrd="${config[initrd]}")
    [[ -n "${config[bootargs]:-}" ]] && args+=(--bootargs="${config[bootargs]}")
    [[ "${config[dtb_enabled]:-true}" == "false" ]] && args+=(--disable-dtb)
    [[ -n "${config[dtb]:-}" ]] && args+=(--dtb="${config[dtb]}")
    [[ -n "${config[dm_progsize]:-}" && "${config[dm_progsize]}" != "2" ]] && \
        args+=(--dm-progsize="${config[dm_progsize]}")
    [[ -n "${config[dm_sba]:-}" && "${config[dm_sba]}" != "0" ]] && \
        args+=(--dm-sba="${config[dm_sba]}")
    [[ "${config[dm_auth]:-false}" == "true" ]] && args+=(--dm-auth)
    [[ -n "${config[dm_abstract_rti]:-}" && "${config[dm_abstract_rti]}" != "0" ]] && \
        args+=(--dm-abstract-rti="${config[dm_abstract_rti]}")
    [[ "${config[dm_hasel]:-true}" == "false" ]] && args+=(--dm-no-hasel)
    [[ "${config[dm_abstract_csr]:-true}" == "false" ]] && args+=(--dm-no-abstract-csr)
    [[ "${config[dm_abstract_fpr]:-true}" == "false" ]] && args+=(--dm-no-abstract-fpr)
    [[ "${config[dm_halt_groups]:-true}" == "false" ]] && args+=(--dm-no-halt-groups)
    [[ "${config[dm_impebreak]:-true}" == "false" ]] && args+=(--dm-no-impebreak)
    [[ "${config[log]:-false}" == "true" ]] && args+=(-l)
    [[ -n "${config[log_file]:-}" ]] && args+=(--log="${config[log_file]}")
    [[ "${config[log_commits]:-false}" == "true" ]] && args+=(--log-commits)
    [[ "${config[log_cache_miss]:-false}" == "true" ]] && args+=(--log-cache-miss)
    [[ "${config[debug]:-false}" == "true" ]] && args+=(-d)
    [[ -n "${config[debug_cmd]:-}" ]] && args+=(--debug-cmd="${config[debug_cmd]}")
    [[ "${config[halted]:-false}" == "true" ]] && args+=(-H)
    [[ "${config[histogram]:-false}" == "true" ]] && args+=(-g)
    [[ "${config[real_time_clint]:-false}" == "true" ]] && args+=(--real-time-clint)

    for ext in "${extensions[@]+"${extensions[@]}"}"; do
        args+=(--extension="$ext")
    done
    for lib in "${extlibs[@]+"${extlibs[@]}"}"; do
        args+=(--extlib="$lib")
    done
    for dev in "${devices[@]+"${devices[@]}"}"; do
        args+=(--device="$dev")
    done

    [[ -n "${config[rbb_port]:-}" ]] && args+=(--rbb-port="${config[rbb_port]}")

    printf '%s\n' "${args[@]}"
}

# Detect if a config file specifies RV32
is_rv32_config() {
    local cfg_file="$1"
    grep -q 'isa.*=.*RV32' "$cfg_file" 2>/dev/null
}

# Detect if an ELF binary is 32-bit (returns 0 for 32-bit, 1 for 64-bit)
is_elf32() {
    local file="$1"
    # ELF byte 4 (EI_CLASS): 1=32-bit, 2=64-bit
    local class
    class=$(od -An -tx1 -j4 -N1 "$file" 2>/dev/null | tr -d ' ')
    [ "$class" = "01" ]
}

# Auto-select config based on ELF type
auto_config() {
    local program="$1"
    if is_elf32 "$program"; then
        echo "$CFG_PK32"
    else
        echo "$CFG_PK64"
    fi
}

cmd_gcc() {
    ensure_prefix

    local ARCH="64"

    while [ $# -gt 0 ]; do
        case "$1" in
            --rv32) ARCH="32"; shift ;;
            *) break ;;
        esac
    done

    if [ $# -lt 1 ]; then
        echo "Error: No source file specified."
        echo "Usage: $0 gcc [--rv32] <source.c> [-o output] [gcc flags...]"
        exit 1
    fi

    local SOURCE="$1"
    shift

    if [ ! -f "$SOURCE" ]; then
        echo "Error: Source file '$SOURCE' not found."
        exit 1
    fi

    local GCC="$PREFIX/bin/riscv${ARCH}-unknown-elf-gcc"
    if [ ! -x "$GCC" ]; then
        echo "Error: Compiler not found: $GCC"
        exit 1
    fi

    # Determine output name
    local has_output=0
    for arg in "$@"; do
        [ "$arg" = "-o" ] && has_output=1
    done

    if [ "$has_output" -eq 0 ]; then
        local OUTNAME="${SOURCE%.c}"
        echo "==> Compiling $SOURCE with $(basename "$GCC")..."
        "$GCC" -O2 -o "$OUTNAME" "$SOURCE" "$@"
        echo "==> Compiled: $OUTNAME"
    else
        echo "==> Compiling $SOURCE with $(basename "$GCC")..."
        "$GCC" -O2 "$SOURCE" "$@"
        echo "==> Done."
    fi
}

cmd_run() {
    ensure_prefix

    local cfg_file="$CFG_BAREMETAL"

    while [ $# -gt 0 ]; do
        case "$1" in
            --cfg)
                [ $# -lt 2 ] && { echo "Error: --cfg requires a file path."; exit 1; }
                cfg_file="$2"
                shift 2
                ;;
            *) break ;;
        esac
    done

    if [ $# -lt 1 ]; then
        echo "Error: No program specified."
        echo "Usage: $0 run [--cfg <file>] <program> [args...]"
        exit 1
    fi

    local PROGRAM="$1"
    shift

    if [ ! -f "$PROGRAM" ]; then
        echo "Error: Program '$PROGRAM' not found."
        exit 1
    fi

    local -a spike_args=()
    while IFS= read -r arg; do
        spike_args+=("$arg")
    done < <(parse_config "$cfg_file")

    echo "==> Running $PROGRAM on Spike..."
    echo "    $PREFIX/bin/spike ${spike_args[*]} $PROGRAM $*"
    "$PREFIX/bin/spike" "${spike_args[@]}" "$PROGRAM" "$@"
}

cmd_pk() {
    ensure_prefix

    local cfg_file=""
    local cfg_explicit=0

    while [ $# -gt 0 ]; do
        case "$1" in
            --cfg)
                [ $# -lt 2 ] && { echo "Error: --cfg requires a file path."; exit 1; }
                cfg_file="$2"
                cfg_explicit=1
                shift 2
                ;;
            *) break ;;
        esac
    done

    if [ $# -lt 1 ]; then
        echo "Error: No program specified."
        echo "Usage: $0 pk [--cfg <file>] <program> [args...]"
        exit 1
    fi

    local PROGRAM="$1"
    shift

    if [ ! -f "$PROGRAM" ]; then
        echo "Error: Program '$PROGRAM' not found."
        echo "Compile it first with: $0 gcc ${PROGRAM}.c"
        exit 1
    fi

    # Auto-detect config from ELF type if not explicitly specified
    if [ "$cfg_explicit" -eq 0 ]; then
        cfg_file="$(auto_config "$PROGRAM")"
        if is_elf32 "$PROGRAM"; then
            echo "==> Auto-detected RV32 ELF, using $(basename "$cfg_file")"
        else
            echo "==> Auto-detected RV64 ELF, using $(basename "$cfg_file")"
        fi
    fi

    local -a spike_args=()
    while IFS= read -r arg; do
        spike_args+=("$arg")
    done < <(parse_config "$cfg_file")

    # Select correct pk binary based on ISA width
    local PK_BIN="$PREFIX/riscv64-unknown-elf/bin/pk"
    if is_rv32_config "$cfg_file"; then
        PK_BIN="$PREFIX/riscv32-unknown-elf/bin/pk"
    fi

    if [ ! -f "$PK_BIN" ]; then
        echo "Error: Proxy kernel not found: $PK_BIN"
        exit 1
    fi

    echo "==> Running $PROGRAM on Spike with proxy kernel..."
    echo "    $PREFIX/bin/spike ${spike_args[*]} $PK_BIN $PROGRAM $*"
    "$PREFIX/bin/spike" "${spike_args[@]}" "$PK_BIN" "$PROGRAM" "$@"
}

cmd_make() {
    ensure_prefix
    echo "==> Running make in $SCRIPT_DIR (PREFIX=$PREFIX) ..."
    make -C "$SCRIPT_DIR" PREFIX="$PREFIX" "$@"
}

cmd_show_cmd() {
    ensure_prefix

    local cfg_file="$CFG_BAREMETAL"

    while [ $# -gt 0 ]; do
        case "$1" in
            --cfg)
                [ $# -lt 2 ] && { echo "Error: --cfg requires a file path."; exit 1; }
                cfg_file="$2"
                shift 2
                ;;
            *) break ;;
        esac
    done

    echo "Config: $cfg_file"
    echo ""

    local -a spike_args=()
    while IFS= read -r arg; do
        spike_args+=("$arg")
    done < <(parse_config "$cfg_file")

    echo "spike ${spike_args[*]} <program> [args...]"
}

# --- Parse global options ---
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)
            [ $# -lt 2 ] && { echo "Error: --prefix requires a path."; exit 1; }
            PREFIX="$2"
            shift 2
            ;;
        *) break ;;
    esac
done

[ $# -lt 1 ] && usage

COMMAND="$1"
shift

case "$COMMAND" in
    gcc)      cmd_gcc "$@" ;;
    run)      cmd_run "$@" ;;
    pk)       cmd_pk "$@" ;;
    show-cmd) cmd_show_cmd "$@" ;;
    make)     cmd_make "$@" ;;
    help)     usage ;;
    *)        usage ;;
esac
