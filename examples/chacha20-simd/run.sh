#!/bin/sh

set -eu
example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
module=${1:-$example_dir/benchmark.wasm}
[ -f "$module" ] || "$example_dir/build.sh" "$module"
wasmtime run -W simd=y "$module"
