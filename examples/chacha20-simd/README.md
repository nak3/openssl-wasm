# ChaCha20 WebAssembly SIMD experiment

This experiment processes four ChaCha20 blocks in parallel with WebAssembly
128-bit SIMD and compares it with LibreSSL's portable scalar implementation.

```sh
./build.sh libressl
./examples/chacha20-simd/build.sh
./examples/chacha20-simd/run.sh
```

The benchmark first compares every output byte over 4 MiB. It stops without
reporting performance if the SIMD implementation differs from LibreSSL.

Results depend on the Wasmtime version, host CPU, compiler, and workload. This
is an optimization experiment, not a replacement selected by LibreSSL at
runtime. Integrating it into `libcrypto.a` would additionally require upstream-
quality test vectors, architecture dispatch, constant-time review, and broader
browser/runtime benchmarks.
