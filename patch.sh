#!/bin/sh

set -eu

provider=${1:-openssl}

case "$provider" in
    openssl)
        cd openssl
        for patch_file in ../patches/*.patch; do
            patch -p1 <"$patch_file"
        done
        ;;
    libressl)
        cd libressl
        for patch_file in ../patches/libressl/*.patch; do
            patch -p1 <"$patch_file"
        done
        ;;
    *)
        echo "usage: $0 [openssl|libressl]" >&2
        exit 2
        ;;
esac
