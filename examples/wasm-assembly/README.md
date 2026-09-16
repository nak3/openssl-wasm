# WebAssembly assembly example

This example mixes C, LibreSSL, and handwritten WebAssembly assembly in one
WASI module. [`rotate.s`](rotate.s) implements a 32-bit rotate-left operation
using the WebAssembly `i32.rotl` instruction. C calls that function, formats
its result, and hashes the text with LibreSSL SHA-256.

Build LibreSSL first from the repository root:

```sh
./fetch-libressl.sh
./patch.sh libressl
./build.sh libressl
```

Then build and run the example:

```sh
./examples/wasm-assembly/build.sh
./examples/wasm-assembly/run.sh
```

Expected output:

```text
WebAssembly i32.rotl: 12345678 -> 34567812
LibreSSL SHA-256:    97d2b863161762f0839afc569ccfef0a0de9a8547113b0216678cfe189e053c8
```

This is WebAssembly assembly syntax understood by LLVM/Zig. LibreSSL's native
x86 and ARM assembly implementations cannot be reused for a `wasm32` target,
which is why the main LibreSSL build keeps `ENABLE_ASM=OFF`.
