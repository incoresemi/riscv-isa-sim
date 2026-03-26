#!/usr/bin/env bash
set -euo pipefail

IMAGE="spike-sim"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE="$SCRIPT_DIR/workspace"
PREFIX_FILE="$SCRIPT_DIR/.spike-prefix"
DEFAULT_CFG="$SCRIPT_DIR/spike.cfg"

usage() {
    cat <<EOF
Usage: $0 <command> [args...]

Commands:
  build [--prefix <path>]    Build the Spike simulator Docker image
                             --prefix sets the host install path (default: /opt/riscv)
  install                    Install built tools to the prefix set during build
  gcc [--rv32] <source.c> [-o out]
                             Compile a C program (--rv32 for 32-bit target)
  run [--cfg <file>] <prog>  Run a binary on Spike using config file
  pk  [--cfg <file>] <prog>  Run a binary on Spike with proxy kernel using config file
  make [target...]           Run make in the workspace directory
  show-cmd [--cfg <file>]    Show the spike command that would be generated from config
  help                       Show this help message

Config file:
  Default config: $DEFAULT_CFG
  Override with --cfg <path> on 'run', 'pk', or 'show-cmd' commands.
  Format: key = value (one per line, # for comments)

Examples:
  $0 build --prefix ~/.local
  $0 install
  $0 gcc hello.c
  $0 pk hello                       # Run with default spike.cfg
  $0 pk --cfg myboard.cfg hello     # Run with custom config
  $0 run --cfg myboard.cfg prog.elf # Run ELF directly (no proxy kernel)
  $0 show-cmd --cfg myboard.cfg     # Preview the spike command line
EOF
    exit 1
}

ensure_image() {
    if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
        echo "Error: Docker image '$IMAGE' not found. Run '$0 build' first."
        exit 1
    fi
}

ensure_workspace() {
    mkdir -p "$WORKSPACE"
}

get_prefix() {
    if [ -f "$PREFIX_FILE" ]; then
        cat "$PREFIX_FILE"
    else
        echo "/opt/riscv"
    fi
}

# Parse a config file into spike command-line arguments.
# Outputs the arguments one per line to stdout.
parse_config() {
    local cfg_file="$1"
    local -a args=()

    if [ ! -f "$cfg_file" ]; then
        echo "Error: Config file '$cfg_file' not found." >&2
        exit 1
    fi

    # Read all key=value pairs, skipping comments and blank lines
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

        # Strip inline comments (but not inside values that contain #)
        # Only strip if there's a space before #
        if [[ "$val" =~ ^([^#]*[^[:space:]])[[:space:]]+#.*$ ]]; then
            val="${BASH_REMATCH[1]}"
        fi

        # Collect multi-value keys
        case "$key" in
            memory|memory\ *)
                # "memory = 0x80000000:0x80000000" or "memory MAIN_RAM = 0x80000000:0x80000000"
                # Either way, val is the base:size part
                mem_regions+=("$val")
                ;;
            extension) extensions+=("$val") ;;
            extlib)    extlibs+=("$val") ;;
            device)    devices+=("$val") ;;
            *)         config["$key"]="$val" ;;
        esac
    done < "$cfg_file"

    # --- Build spike arguments ---

    # ISA
    [[ -n "${config[isa]:-}" ]] && args+=(--isa="${config[isa]}")

    # Privilege modes
    [[ -n "${config[priv]:-}" ]] && args+=(--priv="${config[priv]}")

    # Vector architecture
    [[ -n "${config[varch]:-}" ]] && args+=(--varch="${config[varch]}")

    # Processors
    [[ -n "${config[processors]:-}" && "${config[processors]}" != "1" ]] && \
        args+=(-p"${config[processors]}")

    # Hart IDs
    [[ -n "${config[hartids]:-}" ]] && args+=(--hartids="${config[hartids]}")

    # PC override
    [[ -n "${config[pc]:-}" ]] && args+=(--pc="${config[pc]}")

    # Memory — join multiple regions with commas
    if [[ ${#mem_regions[@]} -gt 0 ]]; then
        local mem_arg=""
        for region in "${mem_regions[@]}"; do
            [[ -n "$mem_arg" ]] && mem_arg+=","
            mem_arg+="$region"
        done
        args+=(-m"$mem_arg")
    fi

    # Endianness
    [[ "${config[big_endian]:-false}" == "true" ]] && args+=(--big-endian)

    # Misaligned
    [[ "${config[misaligned]:-false}" == "true" ]] && args+=(--misaligned)

    # PMP
    [[ -n "${config[pmpregions]:-}" && "${config[pmpregions]}" != "16" ]] && \
        args+=(--pmpregions="${config[pmpregions]}")
    [[ -n "${config[pmpgranularity]:-}" && "${config[pmpgranularity]}" != "4" ]] && \
        args+=(--pmpgranularity="${config[pmpgranularity]}")

    # Caches
    [[ -n "${config[ic]:-}" ]] && args+=(--ic="${config[ic]}")
    [[ -n "${config[dc]:-}" ]] && args+=(--dc="${config[dc]}")
    [[ -n "${config[l2]:-}" ]] && args+=(--l2="${config[l2]}")

    # Block size
    [[ -n "${config[blocksz]:-}" && "${config[blocksz]}" != "64" ]] && \
        args+=(--blocksz="${config[blocksz]}")

    # Triggers
    [[ -n "${config[triggers]:-}" && "${config[triggers]}" != "4" ]] && \
        args+=(--triggers="${config[triggers]}")

    # Kernel / initrd / bootargs
    [[ -n "${config[kernel]:-}" ]] && args+=(--kernel="${config[kernel]}")
    [[ -n "${config[initrd]:-}" ]] && args+=(--initrd="${config[initrd]}")
    [[ -n "${config[bootargs]:-}" ]] && args+=(--bootargs="${config[bootargs]}")

    # Device tree
    [[ "${config[dtb_enabled]:-true}" == "false" ]] && args+=(--disable-dtb)
    [[ -n "${config[dtb]:-}" ]] && args+=(--dtb="${config[dtb]}")

    # Debug module
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

    # Logging
    [[ "${config[log]:-false}" == "true" ]] && args+=(-l)
    [[ -n "${config[log_file]:-}" ]] && args+=(--log="${config[log_file]}")
    [[ "${config[log_commits]:-false}" == "true" ]] && args+=(--log-commits)
    [[ "${config[log_cache_miss]:-false}" == "true" ]] && args+=(--log-cache-miss)

    # Debug mode
    [[ "${config[debug]:-false}" == "true" ]] && args+=(-d)
    [[ -n "${config[debug_cmd]:-}" ]] && args+=(--debug-cmd="${config[debug_cmd]}")
    [[ "${config[halted]:-false}" == "true" ]] && args+=(-H)

    # Histogram
    [[ "${config[histogram]:-false}" == "true" ]] && args+=(-g)

    # Real-time CLINT
    [[ "${config[real_time_clint]:-false}" == "true" ]] && args+=(--real-time-clint)

    # Extensions
    for ext in "${extensions[@]+"${extensions[@]}"}"; do
        args+=(--extension="$ext")
    done

    # External libraries
    for lib in "${extlibs[@]+"${extlibs[@]}"}"; do
        args+=(--extlib="$lib")
    done

    # Devices
    for dev in "${devices[@]+"${devices[@]}"}"; do
        args+=(--device="$dev")
    done

    # Remote bitbang
    [[ -n "${config[rbb_port]:-}" ]] && args+=(--rbb-port="${config[rbb_port]}")

    # Output arguments
    printf '%s\n' "${args[@]}"
}

# Build the full spike command array from a config file
build_spike_cmd() {
    local cfg_file="$1"
    local -a spike_args=()

    while IFS= read -r arg; do
        spike_args+=("$arg")
    done < <(parse_config "$cfg_file")

    printf '%s\n' "${spike_args[@]}"
}

cmd_build() {
    local PREFIX="/opt/riscv"

    while [ $# -gt 0 ]; do
        case "$1" in
            --prefix)
                [ $# -lt 2 ] && { echo "Error: --prefix requires a path argument."; exit 1; }
                PREFIX="$(realpath -m "$2")"
                shift 2
                ;;
            *)
                echo "Error: Unknown build option '$1'"
                exit 1
                ;;
        esac
    done

    echo "$PREFIX" > "$PREFIX_FILE"

    echo "==> Building Spike simulator Docker image (prefix=$PREFIX)..."
    docker build \
        --build-arg "RISCV_PREFIX=$PREFIX" \
        -t "$IMAGE" \
        -f "$SCRIPT_DIR/Dockerfile" \
        "$SCRIPT_DIR"
    echo "==> Build complete. Run '$0 install' to install to $PREFIX on the host."
}

cmd_install() {
    ensure_image
    local PREFIX
    PREFIX="$(get_prefix)"

    echo "==> Installing RISC-V tools to $PREFIX ..."
    mkdir -p "$PREFIX"
    docker run --rm -v "$PREFIX:/hostprefix" "$IMAGE" \
        bash -c "cp -a \"$PREFIX\"/* /hostprefix/"
    echo "==> Installed. Make sure $PREFIX/bin is in your PATH:"
    echo "    export PATH=\"$PREFIX/bin:\$PATH\""
}

cmd_gcc() {
    ensure_image
    ensure_workspace

    local ARCH="64"

    # Parse --rv32 option
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

    SOURCE="$1"
    shift

    if [ ! -f "$WORKSPACE/$SOURCE" ]; then
        if [ -f "$SOURCE" ]; then
            cp "$SOURCE" "$WORKSPACE/"
            SOURCE="$(basename "$SOURCE")"
        else
            echo "Error: Source file '$SOURCE' not found."
            exit 1
        fi
    fi

    local GCC="riscv${ARCH}-unknown-elf-gcc"
    echo "==> Compiling $SOURCE with $GCC..."
    docker run --rm -v "$WORKSPACE:/workspace" "$IMAGE" \
        "$GCC" -o "/workspace/${SOURCE%.c}" "/workspace/$SOURCE" "$@"
    echo "==> Compiled: ${SOURCE%.c}"
}

# Run a program directly on spike (no proxy kernel)
cmd_run() {
    ensure_image
    ensure_workspace

    local cfg_file="$DEFAULT_CFG"

    # Parse --cfg option
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

    if [ ! -f "$WORKSPACE/$PROGRAM" ]; then
        echo "Error: Program '$WORKSPACE/$PROGRAM' not found."
        exit 1
    fi

    # Build spike args from config
    local -a spike_args=()
    while IFS= read -r arg; do
        spike_args+=("$arg")
    done < <(parse_config "$cfg_file")

    echo "==> Running $PROGRAM on Spike..."
    echo "    spike ${spike_args[*]} /workspace/$PROGRAM $*"
    docker run --rm -v "$WORKSPACE:/workspace" "$IMAGE" \
        spike "${spike_args[@]}" "/workspace/$PROGRAM" "$@"
}

# Detect if a config file specifies RV32 (returns 0 for RV32, 1 for RV64)
is_rv32_config() {
    local cfg_file="$1"
    grep -q 'isa.*=.*RV32' "$cfg_file" 2>/dev/null
}

# Run a program on spike with proxy kernel
cmd_pk() {
    ensure_image
    ensure_workspace

    local cfg_file="$DEFAULT_CFG"

    # Parse --cfg option
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
        echo "Usage: $0 pk [--cfg <file>] <program> [args...]"
        exit 1
    fi

    local PROGRAM="$1"
    shift

    if [ ! -f "$WORKSPACE/$PROGRAM" ]; then
        echo "Error: Program '$WORKSPACE/$PROGRAM' not found."
        echo "Compile it first with: $0 gcc ${PROGRAM}.c"
        exit 1
    fi

    # Build spike args from config
    local -a spike_args=()
    while IFS= read -r arg; do
        spike_args+=("$arg")
    done < <(parse_config "$cfg_file")

    # Select correct pk binary based on ISA width
    # Paths are inside the Docker container (RISCV_PREFIX from build)
    local PK_BIN="pk"
    if is_rv32_config "$cfg_file"; then
        local PREFIX
        PREFIX="$(get_prefix)"
        PK_BIN="${PREFIX}/riscv32-unknown-elf/bin/pk"
        echo "==> Detected RV32 config, using 32-bit pk"
    fi

    echo "==> Running $PROGRAM on Spike with proxy kernel..."
    echo "    spike ${spike_args[*]} $PK_BIN /workspace/$PROGRAM $*"
    docker run --rm -v "$WORKSPACE:/workspace" "$IMAGE" \
        spike "${spike_args[@]}" "$PK_BIN" "/workspace/$PROGRAM" "$@"
}

cmd_make() {
    ensure_workspace
    echo "==> Running make in $WORKSPACE ..."
    make -C "$WORKSPACE" "$@"
}

cmd_show_cmd() {
    local cfg_file="$DEFAULT_CFG"

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

[ $# -lt 1 ] && usage

COMMAND="$1"
shift

case "$COMMAND" in
    build)    cmd_build "$@" ;;
    install)  cmd_install ;;
    gcc)      cmd_gcc "$@" ;;
    run)      cmd_run "$@" ;;
    pk)       cmd_pk "$@" ;;
    show-cmd) cmd_show_cmd "$@" ;;
    make)     cmd_make "$@" ;;
    help)     usage ;;
    *)        usage ;;
esac
