#ifndef CHACHA20_SIMD_H
#define CHACHA20_SIMD_H

#include <stddef.h>
#include <stdint.h>

void wasm_simd_chacha20(unsigned char *out, const unsigned char *in, size_t len,
    const unsigned char key[32], const unsigned char iv[8], uint64_t counter);

#endif
