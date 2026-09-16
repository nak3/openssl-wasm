#!/bin/sh

set -eu

example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
module=${1:-$example_dir/example.wasm}

if ! command -v wasmtime >/dev/null 2>&1; then
    echo "wasmtime is required to run this example" >&2
    exit 1
fi
if [ ! -f "$module" ]; then
    "$example_dir/build.sh" "$module"
fi

wasmtime "$module"
