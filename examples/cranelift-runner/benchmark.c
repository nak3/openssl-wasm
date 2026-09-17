#include <openssl/sha.h>
#include <stdint.h>

/*
 * Keep the guest interface deliberately tiny. The linked C/LibreSSL runtime
 * may retain WASI imports, but the runner grants it no stdio, files, sockets,
 * arguments, or environment variables.
 */
__attribute__((export_name("libressl_sha256_benchmark")))
uint64_t libressl_sha256_benchmark(uint32_t rounds)
{
    unsigned char input[SHA256_DIGEST_LENGTH] = {0};
    unsigned char digest[SHA256_DIGEST_LENGTH];

    for (uint32_t round = 0; round < rounds; round++) {
        input[0] ^= (unsigned char)round;
        input[1] ^= (unsigned char)(round >> 8);
        input[2] ^= (unsigned char)(round >> 16);
        input[3] ^= (unsigned char)(round >> 24);
        if (SHA256(input, sizeof(input), digest) == NULL)
            return UINT64_MAX;
        for (uint32_t i = 0; i < sizeof(input); i++)
            input[i] = digest[i];
    }

    uint64_t fingerprint = 0;
    for (uint32_t i = 0; i < sizeof(fingerprint); i++)
        fingerprint |= (uint64_t)digest[i] << (i * 8);
    return fingerprint;
}
