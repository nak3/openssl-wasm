#!/bin/sh

set -eu

example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$example_dir/../.." && pwd)
output=${1:-$example_dir/example.wasm}
ZIG_GLOBAL_CACHE_DIR=${ZIG_GLOBAL_CACHE_DIR:-$repository_dir/.zig-cache/global}
ZIG_LOCAL_CACHE_DIR=${ZIG_LOCAL_CACHE_DIR:-$repository_dir/.zig-cache/local}
export ZIG_GLOBAL_CACHE_DIR ZIG_LOCAL_CACHE_DIR

if ! command -v zig >/dev/null 2>&1; then
    echo "zig is required to build this example" >&2
    exit 1
fi
if [ ! -f "$repository_dir/precompiled/lib/libcrypto.a" ]; then
    echo "libcrypto.a is missing. Build LibreSSL first with ./build.sh libressl" >&2
    exit 1
fi

zig cc --target=wasm32-wasi \
    -I"$repository_dir/precompiled/include" \
    "$example_dir/example.c" \
    "$repository_dir/precompiled/lib/libcrypto.a" \
    -o "$output"

echo "Built $output"
