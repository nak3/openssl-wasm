#include "chacha20_simd.h"

#include <openssl/chacha.h>
#include <string.h>
#include <wasm_simd128.h>

static uint32_t load32_le(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
        (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void store32_le(unsigned char *p, uint32_t value)
{
    p[0] = value;
    p[1] = value >> 8;
    p[2] = value >> 16;
    p[3] = value >> 24;
}

static v128_t rotl32(v128_t value, int bits)
{
    return wasm_v128_or(wasm_i32x4_shl(value, bits),
        wasm_u32x4_shr(value, 32 - bits));
}

#define QUARTERROUND(a, b, c, d) do { \
    x[a] = wasm_i32x4_add(x[a], x[b]); \
    x[d] = rotl32(wasm_v128_xor(x[d], x[a]), 16); \
    x[c] = wasm_i32x4_add(x[c], x[d]); \
    x[b] = rotl32(wasm_v128_xor(x[b], x[c]), 12); \
    x[a] = wasm_i32x4_add(x[a], x[b]); \
    x[d] = rotl32(wasm_v128_xor(x[d], x[a]), 8); \
    x[c] = wasm_i32x4_add(x[c], x[d]); \
    x[b] = rotl32(wasm_v128_xor(x[b], x[c]), 7); \
} while (0)

static void four_blocks(unsigned char *out, const unsigned char *in,
    const unsigned char key[32], const unsigned char iv[8], uint64_t counter)
{
    static const uint32_t constants[4] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574
    };
    v128_t initial[16], x[16];
    uint32_t counter_low[4], counter_high[4];
    uint32_t words[16][4];

    for (int lane = 0; lane < 4; lane++) {
        uint64_t value = counter + (uint64_t)lane;
        counter_low[lane] = (uint32_t)value;
        counter_high[lane] = (uint32_t)(value >> 32);
    }
    for (int i = 0; i < 4; i++)
        initial[i] = wasm_i32x4_splat(constants[i]);
    for (int i = 0; i < 8; i++)
        initial[4 + i] = wasm_i32x4_splat(load32_le(key + i * 4));
    initial[12] = wasm_v128_load(counter_low);
    initial[13] = wasm_v128_load(counter_high);
    initial[14] = wasm_i32x4_splat(load32_le(iv));
    initial[15] = wasm_i32x4_splat(load32_le(iv + 4));
    memcpy(x, initial, sizeof(x));

    for (int round = 0; round < 10; round++) {
        QUARTERROUND(0, 4, 8, 12);
        QUARTERROUND(1, 5, 9, 13);
        QUARTERROUND(2, 6, 10, 14);
        QUARTERROUND(3, 7, 11, 15);
        QUARTERROUND(0, 5, 10, 15);
        QUARTERROUND(1, 6, 11, 12);
        QUARTERROUND(2, 7, 8, 13);
        QUARTERROUND(3, 4, 9, 14);
    }
    for (int word = 0; word < 16; word++) {
        x[word] = wasm_i32x4_add(x[word], initial[word]);
        wasm_v128_store(words[word], x[word]);
    }

    for (int lane = 0; lane < 4; lane++) {
        for (int word = 0; word < 16; word++) {
            uint32_t input_word = load32_le(in + lane * 64 + word * 4);
            store32_le(out + lane * 64 + word * 4,
                input_word ^ words[word][lane]);
        }
    }
}

void wasm_simd_chacha20(unsigned char *out, const unsigned char *in, size_t len,
    const unsigned char key[32], const unsigned char iv[8], uint64_t counter)
{
    unsigned char key_copy[32], iv_copy[8];

    /* Preserve parameters in case the output overlaps the key or IV. */
    memcpy(key_copy, key, sizeof(key_copy));
    memcpy(iv_copy, iv, sizeof(iv_copy));

    while (len >= 256) {
        four_blocks(out, in, key_copy, iv_copy, counter);
        out += 256;
        in += 256;
        len -= 256;
        counter += 4;
    }
    if (len > 0)
        CRYPTO_chacha_20(out, in, len, key_copy, iv_copy, counter);
}
