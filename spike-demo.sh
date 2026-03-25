#!/usr/bin/env bash
set -euo pipefail

IMAGE="spike-sim"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE="$SCRIPT_DIR/workspace"
PREFIX_FILE="$SCRIPT_DIR/.spike-prefix"

usage() {
    cat <<EOF
Usage: $0 <command> [args...]

Commands:
  build [--prefix <path>]    Build the Spike simulator Docker image
                             --prefix sets the host install path (default: /opt/riscv)
                             Spike is compiled with this prefix so 'spike pk' works
                             natively on the host after install.
  install                    Install built tools to the prefix set during build
  gcc <source.c> [-o out]    Compile a C program with riscv64-unknown-elf-gcc
  pk  <program> [args...]    Run a RISC-V binary on Spike with the proxy kernel
  help                       Show this help message

Examples:
  $0 build --prefix ~/.local # Build with host-native prefix
  $0 install                 # Install tools to the prefix from build
  $0 gcc hello.c             # Cross-compile hello.c for RISC-V
  $0 pk hello                # Run the binary on Spike simulator
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

    # Save prefix for install and other commands
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
    # Mount host prefix at /hostprefix to avoid shadowing container's prefix path
    docker run --rm -v "$PREFIX:/hostprefix" "$IMAGE" \
        bash -c "cp -a \"$PREFIX\"/* /hostprefix/"
    echo "==> Installed. Make sure $PREFIX/bin is in your PATH:"
    echo "    export PATH=\"$PREFIX/bin:\$PATH\""
}

cmd_gcc() {
    ensure_image
    ensure_workspace
    if [ $# -lt 1 ]; then
        echo "Error: No source file specified."
        echo "Usage: $0 gcc <source.c> [-o output]"
        exit 1
    fi

    SOURCE="$1"
    shift

    # If source file is outside workspace, copy it in
    if [ ! -f "$WORKSPACE/$SOURCE" ]; then
        if [ -f "$SOURCE" ]; then
            cp "$SOURCE" "$WORKSPACE/"
            SOURCE="$(basename "$SOURCE")"
        else
            echo "Error: Source file '$SOURCE' not found."
            exit 1
        fi
    fi

    echo "==> Compiling $SOURCE with riscv64-unknown-elf-gcc..."
    docker run --rm -v "$WORKSPACE:/workspace" "$IMAGE" \
        riscv64-unknown-elf-gcc -o "/workspace/${SOURCE%.c}" "/workspace/$SOURCE" "$@"
    echo "==> Compiled: ${SOURCE%.c}"
}

cmd_pk() {
    ensure_image
    ensure_workspace
    if [ $# -lt 1 ]; then
        echo "Error: No program specified."
        echo "Usage: $0 pk <program> [args...]"
        exit 1
    fi

    PROGRAM="$1"
    shift

    if [ ! -f "$WORKSPACE/$PROGRAM" ]; then
        echo "Error: Program '$WORKSPACE/$PROGRAM' not found."
        echo "Compile it first with: $0 gcc ${PROGRAM}.c"
        exit 1
    fi

    echo "==> Running $PROGRAM on Spike with proxy kernel..."
    docker run --rm -v "$WORKSPACE:/workspace" "$IMAGE" \
        spike pk "/workspace/$PROGRAM" "$@"
}

[ $# -lt 1 ] && usage

COMMAND="$1"
shift

case "$COMMAND" in
    build)   cmd_build "$@" ;;
    install) cmd_install ;;
    gcc)     cmd_gcc "$@" ;;
    pk)      cmd_pk "$@" ;;
    help)    usage ;;
    *)       usage ;;
esac
