#!/bin/sh

set -eu
example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
port=${1:-8080}

[ -f "$example_dir/web/tls-lab.wasm" ] || "$example_dir/build.sh"
echo "TLS Lab: http://localhost:$port"
python3 -m http.server "$port" --directory "$example_dir/web"
