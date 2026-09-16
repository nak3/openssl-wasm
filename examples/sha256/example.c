#include <openssl/sha.h>
#include <stdio.h>

int main(void)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];

    if (SHA256((const unsigned char *)"hello", 5, digest) == NULL) {
        fprintf(stderr, "SHA256 failed\n");
        return 1;
    }

    for (size_t i = 0; i < sizeof(digest); i++)
        printf("%02x", digest[i]);

    putchar('\n');
    return 0;
}
