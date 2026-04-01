FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

ARG RISCV_PREFIX=/opt/riscv
ENV RISCV=$RISCV_PREFIX
ENV PATH="$RISCV/bin:$PATH"

# Install base dependencies
RUN apt-get update && apt-get install -y \
    autoconf \
    automake \
    autotools-dev \
    curl \
    python3 \
    libmpc-dev \
    libmpfr-dev \
    libgmp-dev \
    gawk \
    build-essential \
    bison \
    flex \
    texinfo \
    gperf \
    libtool \
    patchutils \
    bc \
    zlib1g-dev \
    libexpat-dev \
    device-tree-compiler \
    libboost-regex-dev \
    libboost-system-dev \
    git \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Build the RISC-V GNU Toolchain — single multilib build (RV64 + RV32)
#
# Base: rv64gc / lp64d
# Multilib variants include RV32 with and without floating-point,
# matching the InCore toolchain approach.
#
# The multilib generator format is: arch-abi--
#   (empty reuse and subset fields = standalone library set)
RUN git clone --depth 1 https://github.com/riscv-collab/riscv-gnu-toolchain.git /tmp/riscv-gnu-toolchain && \
    cd /tmp/riscv-gnu-toolchain && \
    mkdir -p $RISCV && \
    ./configure --prefix=$RISCV \
        --with-arch=rv64gc --with-abi=lp64d \
        --with-multilib-generator="\
rv32gc-ilp32d--;\
rv32imac-ilp32--;\
rv32im-ilp32--;\
rv64gc-lp64d--;\
rv64imac-lp64--" && \
    make -j$(nproc) && \
    rm -rf /tmp/riscv-gnu-toolchain

# Build Spike (riscv-isa-sim) from local source
COPY . /tmp/riscv-isa-sim
RUN cd /tmp/riscv-isa-sim && \
    mkdir -p build && cd build && \
    ../configure --prefix=$RISCV && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/riscv-isa-sim

# Build the RISC-V Proxy Kernel — RV64 pk
RUN git clone --depth 1 https://github.com/riscv-software-src/riscv-pk.git /tmp/riscv-pk && \
    cd /tmp/riscv-pk && \
    mkdir build64 && cd build64 && \
    ../configure --prefix=$RISCV --host=riscv64-unknown-elf && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/riscv-pk

# Build the RISC-V Proxy Kernel — RV32 pk (using multilib compiler)
RUN git clone --depth 1 https://github.com/riscv-software-src/riscv-pk.git /tmp/riscv-pk32 && \
    cd /tmp/riscv-pk32 && \
    mkdir build32 && cd build32 && \
    ../configure \
        --prefix=$RISCV \
        --host=riscv32-unknown-elf \
        --with-arch=rv32gc_zicsr_zifencei \
        CC=riscv64-unknown-elf-gcc \
        OBJCOPY=riscv64-unknown-elf-objcopy \
        AR=riscv64-unknown-elf-ar \
        RANLIB=riscv64-unknown-elf-ranlib \
        READELF=riscv64-unknown-elf-readelf \
        CFLAGS="-march=rv32gc -mabi=ilp32d" \
        LDFLAGS="-march=rv32gc -mabi=ilp32d" && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/riscv-pk32

WORKDIR /workspace
