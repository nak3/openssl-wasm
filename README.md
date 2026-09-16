# OpenSSL 3 and LibreSSL for WebAssembly

This repository builds OpenSSL or LibreSSL as static libraries for WebAssembly/WASI.

Up-to-date. Maintained.

Related: [BoringSSL for WebAssembly](https://github.com/jedisct1/boringssl-wasm).

## Precompiled library

For convenience, [precompiled files](precompiled/) libraries for WebAssembly can be directly downloaded from this repository.

They can be directly linked to C, Rust, Zig, etc. as regular static libraries.

## OpenSSL source

This repository includes an unmodified version of `OpenSSL` as a submodule. If you didn't clone it with the `--recursive` flag, the following command can be used to pull the submodule:

```sh
git submodule update --init --recursive --depth=1
```

## Dependencies

The only required dependencies to rebuild the library are:

* Perl - Required by OpenSSL to generate files
* CMake - Required when building LibreSSL
* curl - Required by the LibreSSL source fetcher
* [Zig](https://www.ziglang.org) - To compile C code to WebAssembly

## Building

```sh
./patch.sh
./build.sh
```

The resulting files can be found in the `precompiled` directory.

## Building LibreSSL

LibreSSL 4.3.2 is downloaded from the official OpenBSD mirror and verified
against a pinned SHA-256 checksum. It is then cross-compiled with Zig through
LibreSSL's CMake build.

```sh
./fetch-libressl.sh
./patch.sh libressl
./build.sh libressl
```

The result uses the same layout as the OpenSSL build. It contains
`libcrypto.a` and `libssl.a` in `precompiled/lib`, with the
LibreSSL headers in `precompiled/include`. Building one provider replaces the
other provider's headers and same-named libraries, so consumers cannot
accidentally mix the two implementations.

WASI Preview 1 has no socket API, so LibreSSL's socket BIO implementations are
excluded, matching the OpenSSL build's `no-sock` configuration. TLS can still
be driven by an application-provided BIO.

Running `./build.sh` without an argument continues to build OpenSSL.

## Examples

The [SHA-256 example](examples/sha256/) links `libcrypto.a` into a WASI module
and runs it with Wasmtime:

```sh
./examples/sha256/build.sh
./examples/sha256/run.sh
```

The [LibreSSL Certificate Lab](examples/certificate-lab/) runs certificate
inspection and CA verification locally in a browser:

```sh
./examples/certificate-lab/build.sh
./examples/certificate-lab/serve.sh
```

The [WebAssembly assembly example](examples/wasm-assembly/) combines a
handwritten `i32.rotl` routine with LibreSSL SHA-256:

```sh
./examples/wasm-assembly/build.sh
./examples/wasm-assembly/run.sh
```

The [LibreSSL TLS Lab](examples/tls-lab/) visualizes a real in-memory TLS 1.2
handshake and encrypted `ping`/`pong` exchange in the browser:

```sh
./examples/tls-lab/build.sh
./examples/tls-lab/serve.sh
```

The [ChaCha20 SIMD experiment](examples/chacha20-simd/) compares LibreSSL's
portable implementation with a four-block WebAssembly SIMD implementation:

```sh
./examples/chacha20-simd/build.sh
./examples/chacha20-simd/run.sh
```
