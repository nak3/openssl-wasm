#include "chacha20_simd.h"

#include <openssl/chacha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE (4 * 1024 * 1024)
#define ITERATIONS 32

static double now(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

static double benchmark(void (*function)(unsigned char *, const unsigned char *,
    size_t, const unsigned char *, const unsigned char *, uint64_t),
    unsigned char *out, const unsigned char *in, const unsigned char key[32],
    const unsigned char iv[8])
{
    double start = now();
    for (int i = 0; i < ITERATIONS; i++)
        function(out, in, BUFFER_SIZE, key, iv, 0);
    return now() - start;
}

int main(void)
{
    unsigned char key[32], iv[8];
    unsigned char *input = malloc(BUFFER_SIZE);
    unsigned char *scalar = malloc(BUFFER_SIZE);
    unsigned char *simd = malloc(BUFFER_SIZE);
    double scalar_time, simd_time;
    double mebibytes = (double)BUFFER_SIZE * ITERATIONS / (1024.0 * 1024.0);

    if (input == NULL || scalar == NULL || simd == NULL) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    for (size_t i = 0; i < sizeof(key); i++) key[i] = (unsigned char)i;
    for (size_t i = 0; i < sizeof(iv); i++) iv[i] = (unsigned char)(0xa0 + i);
    for (size_t i = 0; i < BUFFER_SIZE; i++) input[i] = (unsigned char)(i * 31u + 7u);

    CRYPTO_chacha_20(scalar, input, BUFFER_SIZE, key, iv, 0);
    wasm_simd_chacha20(simd, input, BUFFER_SIZE, key, iv, 0);
    if (memcmp(scalar, simd, BUFFER_SIZE) != 0) {
        fprintf(stderr, "FAIL: SIMD output differs from LibreSSL\n");
        return 1;
    }
    puts("PASS: SIMD output matches LibreSSL byte-for-byte");

    scalar_time = benchmark(CRYPTO_chacha_20, scalar, input, key, iv);
    simd_time = benchmark(wasm_simd_chacha20, simd, input, key, iv);
    printf("LibreSSL scalar: %8.1f MiB/s\n", mebibytes / scalar_time);
    printf("WASM SIMD:       %8.1f MiB/s\n", mebibytes / simd_time);
    printf("Speedup:         %8.2fx\n", scalar_time / simd_time);

    free(simd); free(scalar); free(input);
    return 0;
}
