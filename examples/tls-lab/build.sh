#!/bin/sh

set -eu

example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$example_dir/../.." && pwd)
libressl_build=${LIBRESSL_BUILD_DIR:-$repository_dir/libressl/build-wasm32-wasi}
output=$example_dir/web/tls-lab.wasm
ZIG_GLOBAL_CACHE_DIR=${ZIG_GLOBAL_CACHE_DIR:-$repository_dir/.zig-cache/global}
ZIG_LOCAL_CACHE_DIR=${ZIG_LOCAL_CACHE_DIR:-$repository_dir/.zig-cache/local}
export ZIG_GLOBAL_CACHE_DIR ZIG_LOCAL_CACHE_DIR

if [ ! -f "$libressl_build/ssl/libssl.a" ]; then
    echo "LibreSSL build output is missing. Run ./build.sh libressl first." >&2
    exit 1
fi

zig cc --target=wasm32-wasi -mexec-model=reactor -O2 \
    -I"$libressl_build/include" \
    "$example_dir/src/tls_lab.c" \
    "$libressl_build/ssl/libssl.a" \
    "$libressl_build/crypto/libcrypto.a" \
    -Wl,--export=tls_lab_alloc \
    -Wl,--export=tls_lab_free \
    -Wl,--export=tls_lab_result \
    -Wl,--export=tls_lab_run \
    -o "$output"

echo "Built $output"
