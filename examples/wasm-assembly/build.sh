#!/bin/sh

set -eu

example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$example_dir/../.." && pwd)
output=${1:-$example_dir/example.wasm}
libressl_build=${LIBRESSL_BUILD_DIR:-$repository_dir/libressl/build-wasm32-wasi}
ZIG_GLOBAL_CACHE_DIR=${ZIG_GLOBAL_CACHE_DIR:-$repository_dir/.zig-cache/global}
ZIG_LOCAL_CACHE_DIR=${ZIG_LOCAL_CACHE_DIR:-$repository_dir/.zig-cache/local}
export ZIG_GLOBAL_CACHE_DIR ZIG_LOCAL_CACHE_DIR

if ! command -v zig >/dev/null 2>&1; then
    echo "zig is required to build this example" >&2
    exit 1
fi
if [ ! -f "$libressl_build/crypto/libcrypto.a" ]; then
    echo "LibreSSL build output is missing. Build it first with ./build.sh libressl" >&2
    exit 1
fi

zig cc --target=wasm32-wasi \
    -I"$libressl_build/include" \
    "$example_dir/example.c" \
    "$example_dir/rotate.s" \
    "$libressl_build/crypto/libcrypto.a" \
    -o "$output"

echo "Built $output"
