# LibreSSL Certificate Lab

A browser demo that parses PEM certificates and verifies them against a CA
certificate entirely inside LibreSSL WebAssembly. Certificate data never
leaves the browser.

## Build

Build LibreSSL first:

```sh
./fetch-libressl.sh
./patch.sh libressl
./build.sh libressl
```

Then build and serve the demo:

```sh
./examples/certificate-lab/build.sh
./examples/certificate-lab/serve.sh
```

Open <http://localhost:8080>. A local HTTP server is required because browsers
do not allow `fetch()` to load WebAssembly from a `file://` page.

The browser module uses the WASI reactor execution model and a small local WASI
shim. It has no runtime package or CDN dependency.
