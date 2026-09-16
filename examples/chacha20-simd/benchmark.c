#include "chacha20_simd.h"

#include <openssl/chacha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_BUFFER_SIZE (4 * 1024 * 1024)
#define TARGET_BYTES (64 * 1024 * 1024)

static volatile unsigned char benchmark_sink;

static const size_t buffer_sizes[] = {
    64, 128, 256, 1024, 4 * 1024, 16 * 1024, 64 * 1024,
    256 * 1024, 1024 * 1024, 4 * 1024 * 1024
};

static double now(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

static double benchmark(void (*function)(unsigned char *, const unsigned char *,
    size_t, const unsigned char *, const unsigned char *, uint64_t),
    unsigned char *out, const unsigned char *in, const unsigned char key[32],
    const unsigned char iv[8], size_t size, size_t iterations)
{
    double start = now();
    for (size_t i = 0; i < iterations; i++)
        function(out, in, size, key, iv, 0);
    double elapsed = now() - start;
    benchmark_sink ^= out[size - 1];
    return elapsed;
}

static void print_size(size_t size)
{
    if (size < 1024)
        printf("%7zu B", size);
    else if (size < 1024 * 1024)
        printf("%7zu KiB", size / 1024);
    else
        printf("%7zu MiB", size / (1024 * 1024));
}

int main(void)
{
    unsigned char key[32], iv[8];
    unsigned char *input = malloc(MAX_BUFFER_SIZE);
    unsigned char *scalar = malloc(MAX_BUFFER_SIZE);
    unsigned char *simd = malloc(MAX_BUFFER_SIZE);

    if (input == NULL || scalar == NULL || simd == NULL) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    for (size_t i = 0; i < sizeof(key); i++) key[i] = (unsigned char)i;
    for (size_t i = 0; i < sizeof(iv); i++) iv[i] = (unsigned char)(0xa0 + i);
    for (size_t i = 0; i < MAX_BUFFER_SIZE; i++)
        input[i] = (unsigned char)(i * 31u + 7u);

    for (size_t i = 0; i < sizeof(buffer_sizes) / sizeof(buffer_sizes[0]); i++) {
        size_t size = buffer_sizes[i];
        CRYPTO_chacha_20(scalar, input, size, key, iv, 0);
        wasm_simd_chacha20(simd, input, size, key, iv, 0);
        if (memcmp(scalar, simd, size) != 0) {
            fprintf(stderr, "FAIL: SIMD output differs at %zu bytes\n", size);
            return 1;
        }
    }
    puts("PASS: SIMD output matches LibreSSL at every tested size");
    puts("");
    puts("   size     LibreSSL -O3      WASM SIMD     speedup");

    for (size_t i = 0; i < sizeof(buffer_sizes) / sizeof(buffer_sizes[0]); i++) {
        size_t size = buffer_sizes[i];
        size_t iterations = TARGET_BYTES / size;
        double mebibytes, scalar_time, simd_time;

        if (iterations < 8)
            iterations = 8;
        mebibytes = (double)size * iterations / (1024.0 * 1024.0);
        scalar_time = benchmark(CRYPTO_chacha_20, scalar, input, key, iv,
            size, iterations);
        simd_time = benchmark(wasm_simd_chacha20, simd, input, key, iv,
            size, iterations);
        print_size(size);
        printf("  %10.1f MiB/s  %10.1f MiB/s    %5.2fx\n",
            mebibytes / scalar_time, mebibytes / simd_time,
            scalar_time / simd_time);
    }

    free(simd); free(scalar); free(input);
    return 0;
}
