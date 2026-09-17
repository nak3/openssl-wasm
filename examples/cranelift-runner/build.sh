#!/bin/sh

set -eu
example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$example_dir/../.." && pwd)
libressl_build=${LIBRESSL_BUILD_DIR:-$repository_dir/libressl/build-wasm32-wasi}
output=${1:-$example_dir/benchmark.wasm}
ZIG_GLOBAL_CACHE_DIR=${ZIG_GLOBAL_CACHE_DIR:-$repository_dir/.zig-cache/global}
ZIG_LOCAL_CACHE_DIR=${ZIG_LOCAL_CACHE_DIR:-$repository_dir/.zig-cache/local}
export ZIG_GLOBAL_CACHE_DIR ZIG_LOCAL_CACHE_DIR

if ! command -v zig >/dev/null 2>&1; then
    echo "zig is required to build the guest" >&2
    exit 1
fi
if ! command -v cargo >/dev/null 2>&1; then
    echo "cargo is required to build the Cranelift runner" >&2
    exit 1
fi
if [ ! -f "$libressl_build/crypto/libcrypto.a" ]; then
    echo "LibreSSL build output is missing. Run ./build.sh libressl first." >&2
    exit 1
fi

zig cc --target=wasm32-wasi -O3 \
    -I"$libressl_build/include" \
    -Wl,--no-entry \
    -Wl,--export=libressl_sha256_benchmark \
    "$example_dir/benchmark.c" \
    "$libressl_build/crypto/libcrypto.a" \
    -o "$output"

cargo build --release --manifest-path "$example_dir/Cargo.toml"
echo "Built $output and the Cranelift runner"

