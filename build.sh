#!/bin/sh

set -eu

provider=${1:-openssl}
nprocessors=$(getconf NPROCESSORS_ONLN 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

build_openssl() {
    if [ ! -f openssl/Configure ]; then
        echo "OpenSSL source is missing. Run: git submodule update --init --recursive --depth=1" >&2
        exit 1
    fi

    (
        cd openssl
        env \
            CROSS_COMPILE="" \
            AR="zig ar" \
            RANLIB="zig ranlib" \
            CC="zig cc --target=wasm32-wasi" \
            CFLAGS="-Ofast -Werror -Qunused-arguments -Wno-shift-count-overflow" \
            CPPFLAGS="${CPPFLAGS:-} -D_BSD_SOURCE -D_WASI_EMULATED_GETPID -Dgetuid=getpagesize -Dgeteuid=getpagesize -Dgetgid=getpagesize -Dgetegid=getpagesize" \
            CXXFLAGS="-Werror -Qunused-arguments -Wno-shift-count-overflow" \
            LDFLAGS="-s -lwasi-emulated-getpid" \
            ./Configure \
            --banner="wasm32-wasi port" \
            no-asm \
            no-async \
            no-egd \
            no-ktls \
            no-module \
            no-posix-io \
            no-secure-memory \
            no-shared \
            no-sock \
            no-stdio \
            no-thread-pool \
            no-threads \
            no-ui-console \
            no-weak-ssl-ciphers \
            wasm32-wasi

        make "-j${nprocessors}"
    )

    mkdir -p precompiled/lib precompiled/include
    cp openssl/libcrypto.a openssl/libssl.a precompiled/lib/
    rm -rf precompiled/include/openssl
    cp -R openssl/include/openssl precompiled/include/
}

build_libressl() {
    if [ ! -f libressl/CMakeLists.txt ]; then
        echo "LibreSSL source is missing. Run: ./fetch-libressl.sh" >&2
        exit 1
    fi
    if ! command -v cmake >/dev/null 2>&1; then
        echo "cmake is required to build LibreSSL" >&2
        exit 1
    fi
    if ! command -v zig >/dev/null 2>&1; then
        echo "zig is required to build LibreSSL" >&2
        exit 1
    fi

    build_dir=libressl/build-wasm32-wasi
    cmake -S libressl -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/zig-wasi-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DENABLE_ASM=OFF \
        -DLIBRESSL_APPS=OFF \
        -DLIBRESSL_TESTS=OFF
    cmake --build "$build_dir" --parallel "$nprocessors" --target crypto ssl

    mkdir -p precompiled/lib precompiled/include
    cp "$build_dir/crypto/libcrypto.a" "$build_dir/ssl/libssl.a" precompiled/lib/
    rm -rf precompiled/include/openssl
    cp -R "$build_dir/include/openssl" precompiled/include/
}

case "$provider" in
    openssl) build_openssl ;;
    libressl) build_libressl ;;
    *)
        echo "usage: $0 [openssl|libressl]" >&2
        exit 2
        ;;
esac
