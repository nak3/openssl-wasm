# LibreSSL TLS Lab

An interactive browser visualization of a real TLS 1.2 handshake between a
LibreSSL client and server. Both endpoints run in the same WebAssembly module
and exchange records through memory BIOs, so no network connection is made.

```sh
./build.sh libressl
./examples/tls-lab/build.sh
./examples/tls-lab/serve.sh
```

Open <http://localhost:8080>, then play the handshake automatically or advance
one TLS record at a time.

The committed private key is only a throwaway demo identity used inside this
local simulation. Never use it for a real server or any security-sensitive
purpose.

TLS 1.2 is used intentionally: its handshake record types remain visible long
enough to make the exchange educational. In TLS 1.3, most handshake messages
after ServerHello are encrypted.
