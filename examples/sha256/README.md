# SHA-256 example

This example links LibreSSL's `libcrypto.a` into a WASI module and calculates
the SHA-256 digest of `hello`.

Build LibreSSL from the repository root first:

```sh
./fetch-libressl.sh
./patch.sh libressl
./build.sh libressl
```

Then build and run the example from any directory:

```sh
./examples/sha256/build.sh
./examples/sha256/run.sh
```

`run.sh` builds the module automatically when it does not exist. The expected
output is:

```text
2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
```

Pass a path to either script to use a different output module:

```sh
./examples/sha256/build.sh /tmp/sha256.wasm
./examples/sha256/run.sh /tmp/sha256.wasm
```
