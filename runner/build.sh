#!/bin/bash
# Rebuilds runner.g3a from scratch on Linux (or WSL on Windows).
# Needs: git, make, gcc, python3, and the GMP/MPFR/MPC/libpng dev packages
#   (Debian/Ubuntu: sudo apt install build-essential git libgmp-dev libmpfr-dev libmpc-dev libpng-dev texinfo)
# The first run builds a SuperH cross compiler, which takes 30-60 minutes.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
DEPS="$HERE/../deps"
PREFIX="$DEPS/sh-elf"
JOBS="$(nproc 2>/dev/null || echo 2)"
mkdir -p "$DEPS"
cd "$DEPS"
export PATH="$PREFIX/bin:$PATH"

fetch() {  # fetch <dir> <git url> <tag/branch>
    [ -d "$1" ] || git clone --depth 1 -b "$3" "$2" "$1"
}

# 1. sh3eb-elf binutils + gcc (big-endian SuperH, bare metal)
if ! command -v sh3eb-elf-gcc >/dev/null; then
    fetch binutils https://github.com/gnutools/binutils-gdb binutils-2_42
    fetch gcc https://github.com/gcc-mirror/gcc releases/gcc-13.2.0
    mkdir -p build-binutils && (cd build-binutils &&
        ../binutils/configure --target=sh3eb-elf --prefix="$PREFIX" --disable-nls --disable-werror \
            --with-sysroot --disable-gdb --disable-gdbserver --disable-sim --disable-readline --disable-gprofng &&
        make -j"$JOBS" && make install)
    mkdir -p build-gcc && (cd build-gcc &&
        ../gcc/configure --target=sh3eb-elf --prefix="$PREFIX" --without-headers --enable-languages=c \
            --disable-tls --disable-nls --disable-threads --disable-shared --disable-libssp --disable-libquadmath \
            --disable-libgomp --disable-libatomic --with-endian=big --with-multilib-list= &&
        make -j"$JOBS" inhibit_libc=true all-gcc && make install-gcc &&
        make -j"$JOBS" inhibit_libc=true all-target-libgcc && make install-target-libgcc)
fi

# 2. libfxcg (the Prizm SDK: OS calls, crt0, a small libc)
fetch libfxcg https://github.com/Jonimoose/libfxcg master
[ -f libfxcg/lib/libfxcg.a ] || make -C libfxcg FXCGSDK="$DEPS/libfxcg"

# 3. mkg3a (wraps a binary in the g3a header)
fetch mkg3a https://github.com/tari/mkg3a master
if [ ! -x mkg3a/mkg3a ]; then
    cat > mkg3a/src/config.h <<'EOF'
#ifndef _CONFIG_H
#define _CONFIG_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define mkg3a_VERSION_TAG "py2g3a"
#define FREAD_CHUNK 512
#define IS_BIG_ENDIAN 0
#define HAVE_UNISTD_H
#define USE_PNG 1
#endif
EOF
    (cd mkg3a/src && gcc -O2 -w -o ../mkg3a mkg3a.c g3a.c images.c util.c -lpng)
fi

# 4. MicroPython
fetch micropython https://github.com/micropython/micropython v1.24.1

# 5. The runner itself
make -C "$HERE" -j"$JOBS" MPY="$DEPS/micropython" FXCGSDK="$DEPS/libfxcg" MKG3A="$DEPS/mkg3a/mkg3a"
cp "$HERE/runner.g3a" "$HERE/../runner.g3a"
echo "Built $(cd "$HERE/.." && pwd)/runner.g3a"
