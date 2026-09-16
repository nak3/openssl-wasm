#!/bin/sh

set -eu

example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
port=${1:-8080}

if [ ! -f "$example_dir/web/libressl-lab.wasm" ]; then
    "$example_dir/build.sh"
fi

echo "Certificate Lab: http://localhost:$port"
python3 -m http.server "$port" --directory "$example_dir/web"
