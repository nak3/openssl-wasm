# LibreSSL Cranelift Runner

This example links LibreSSL SHA-256 into a small WebAssembly module, then uses
Wasmtime's Cranelift backend in two ways:

1. compile the original WebAssembly module to native code;
2. serialize that compiled module, load the AOT artifact, and run it again.

Both paths execute the same Cranelift-generated native code. The useful AOT
comparison is startup cost: loading a trusted precompiled artifact avoids the
Wasm-to-native compilation step.

## Build and run

Build LibreSSL first, then run the example:

```sh
./build.sh libressl
./examples/cranelift-runner/build.sh
./examples/cranelift-runner/run.sh
```

The runner writes `benchmark.cwasm` next to the input module. A `.cwasm` file
is native, host-specific executable data and must only be loaded when it was
produced by a trusted process. It is not a portable WebAssembly file.

The default workload performs 20 guest calls with 10,000 SHA-256 operations
per call. It can be adjusted, and an optional fuel limit can stop a guest that
uses more Wasm instructions than allowed:

```sh
./examples/cranelift-runner/run.sh \
    ./examples/cranelift-runner/benchmark.wasm \
    --rounds 50000 --calls 10

./examples/cranelift-runner/run.sh \
    ./examples/cranelift-runner/benchmark.wasm \
    --fuel 1000000000
```

The runner supplies WASI Preview 1 without inheriting host stdio, arguments,
environment variables, directory access, or sockets. The guest's linear memory
is limited to 32 MiB.
