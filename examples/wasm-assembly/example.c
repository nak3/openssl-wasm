#include <openssl/sha.h>
#include <stdint.h>
#include <stdio.h>

extern uint32_t wasm_rotl32(uint32_t value, uint32_t bits);

int main(void)
{
    const uint32_t input = UINT32_C(0x12345678);
    const uint32_t rotated = wasm_rotl32(input, 8);
    unsigned char digest[SHA256_DIGEST_LENGTH];
    char message[9];

    snprintf(message, sizeof(message), "%08x", rotated);
    if (SHA256((const unsigned char *)message, 8, digest) == NULL) {
        fprintf(stderr, "SHA256 failed\n");
        return 1;
    }

    printf("WebAssembly i32.rotl: %08x -> %s\n", input, message);
    printf("LibreSSL SHA-256:    ");
    for (size_t i = 0; i < sizeof(digest); i++)
        printf("%02x", digest[i]);
    putchar('\n');

    return 0;
}
