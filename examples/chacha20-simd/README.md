# ChaCha20 WebAssembly SIMD experiment

This experiment processes four ChaCha20 blocks in parallel with WebAssembly
128-bit SIMD and compares it with LibreSSL's portable scalar implementation.

```sh
./examples/chacha20-simd/build-o3.sh
./examples/chacha20-simd/run.sh
```

`build-o3.sh` builds a separate `-O3` LibreSSL tree, so the comparison does not
reuse the repository's default `-O2` build. The benchmark tests sizes from 64 B
through 4 MiB and first checks every output byte at every size. It stops without
reporting performance if the SIMD implementation differs from LibreSSL.

Each row processes 64 MiB in total. Inputs below 256 B use the LibreSSL tail
path because the SIMD implementation works on four 64-byte blocks at a time;
this makes the small-input rows useful for showing the wrapper overhead as well
as the point where parallel processing starts.

Results depend on the Wasmtime version, host CPU, compiler, and workload. This
is an optimization experiment, not a replacement selected by LibreSSL at
runtime. Integrating it into `libcrypto.a` would additionally require upstream-
quality test vectors, architecture dispatch, constant-time review, and broader
browser/runtime benchmarks.
