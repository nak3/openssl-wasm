#!/bin/sh

set -eu
example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$example_dir/../.." && pwd)
build_dir=${LIBRESSL_O3_BUILD_DIR:-$repository_dir/libressl/build-wasm32-wasi-o3}
nprocessors=$(getconf NPROCESSORS_ONLN 2>/dev/null || echo 1)
ZIG_GLOBAL_CACHE_DIR=${ZIG_GLOBAL_CACHE_DIR:-$repository_dir/.zig-cache/global}
ZIG_LOCAL_CACHE_DIR=${ZIG_LOCAL_CACHE_DIR:-$repository_dir/.zig-cache/local}
export ZIG_GLOBAL_CACHE_DIR ZIG_LOCAL_CACHE_DIR

cmake -S "$repository_dir/libressl" -B "$build_dir" \
    -DCMAKE_TOOLCHAIN_FILE="$repository_dir/cmake/zig-wasi-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS_RELEASE=-O3 \
    -DHAVE_SYSLOG=0 \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_ASM=OFF \
    -DLIBRESSL_APPS=OFF \
    -DLIBRESSL_TESTS=OFF
cmake --build "$build_dir" --parallel "$nprocessors" --target crypto

LIBRESSL_BUILD_DIR="$build_dir" "$example_dir/build.sh"
