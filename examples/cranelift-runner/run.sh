#!/bin/sh

set -eu
example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
module=$example_dir/benchmark.wasm
runner=$example_dir/target/release/libressl-cranelift-runner

if [ "$#" -gt 0 ] && [ "${1#-}" = "$1" ]; then
    module=$1
    shift
fi

if [ ! -f "$module" ] || [ ! -x "$runner" ]; then
    "$example_dir/build.sh" "$module"
fi

exec "$runner" "$module" "$@"
