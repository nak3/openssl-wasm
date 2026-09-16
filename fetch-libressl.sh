#!/bin/sh

set -eu

version=${LIBRESSL_VERSION:-4.3.2}
expected_sha256=edf01aee24c65d69e6a9efcb9d44bcda682ff9d4f3bbbd95e794e1dfa90847b5
archive="libressl-${version}.tar.gz"
url="https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/${archive}"

if [ "$version" != "4.3.2" ]; then
    echo "No checksum is recorded for LibreSSL ${version}" >&2
    exit 1
fi
if [ -e libressl ]; then
    echo "libressl already exists; remove or move it before fetching another copy" >&2
    exit 1
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/openssl-wasm-libressl.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM

curl -fL "$url" -o "$tmp_dir/$archive"
if command -v sha256sum >/dev/null 2>&1; then
    actual_sha256=$(sha256sum "$tmp_dir/$archive" | awk '{print $1}')
else
    actual_sha256=$(shasum -a 256 "$tmp_dir/$archive" | awk '{print $1}')
fi
if [ "$actual_sha256" != "$expected_sha256" ]; then
    echo "LibreSSL archive checksum mismatch" >&2
    exit 1
fi

tar -xzf "$tmp_dir/$archive" -C "$tmp_dir"
mv "$tmp_dir/libressl-${version}" libressl
echo "Fetched LibreSSL ${version} into ./libressl"
