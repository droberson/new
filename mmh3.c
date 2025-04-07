#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "mmh3.h"

void mmh3_64_make_hashes(const void *data, size_t len, size_t count, uint64_t *hash_output) {
    uint64_t hash[2];
    mmh3_128(data, len, 0, hash);

    for (size_t i = 0; i < count; i++) {
        hash_output[i] = (hash[0] + i * hash[1]) % UINT64_MAX;
    }
}

uint64_t mmh3_64_string(const char *element, const uint64_t seed) {
	return mmh3_64((uint8_t *)element, strlen((char *)element), seed);
}

uint64_t mmh3_64(const void *key, const size_t len, uint64_t seed) {
    const uint64_t  c1 = 0x87c37b91114253d5ULL;
    const uint64_t  c2 = 0x4cf5ad432745937fULL;
    const uint8_t  *data = (const uint8_t *)key;
    const size_t     nblocks = len / 16;

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    // Process the data in 128-bit blocks (two 64-bit halves)
    const uint64_t *blocks = (const uint64_t *)(data);
    for (size_t i = 0; i < nblocks; i++) {
        uint64_t k1 = blocks[i * 2 + 0];
        uint64_t k2 = blocks[i * 2 + 1];

        k1 *= c1;
        k1 = (k1 << 31) | (k1 >> (64 - 31));
        k1 *= c2;
        h1 ^= k1;

        h1 = (h1 << 27) | (h1 >> (64 - 27));
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= c2;
        k2 = (k2 << 33) | (k2 >> (64 - 33));
        k2 *= c1;
        h2 ^= k2;

        h2 = (h2 << 31) | (h2 >> (64 - 31));
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    // Handle remaining bytes
    const uint8_t *tail = (const uint8_t *)(data + nblocks * 16);
    uint64_t k1 = 0;
    uint64_t k2 = 0;

    switch (len & 15) {
        case 15: k2 ^= (uint64_t)(tail[14]) << 48;
        case 14: k2 ^= (uint64_t)(tail[13]) << 40;
        case 13: k2 ^= (uint64_t)(tail[12]) << 32;
        case 12: k2 ^= (uint64_t)(tail[11]) << 24;
        case 11: k2 ^= (uint64_t)(tail[10]) << 16;
        case 10: k2 ^= (uint64_t)(tail[9]) << 8;
        case  9: k2 ^= (uint64_t)(tail[8]);
                 k2 *= c2; k2 = (k2 << 33) | (k2 >> (64 - 33)); k2 *= c1; h2 ^= k2;
        case  8: k1 ^= (uint64_t)(tail[7]) << 56;
        case  7: k1 ^= (uint64_t)(tail[6]) << 48;
        case  6: k1 ^= (uint64_t)(tail[5]) << 40;
        case  5: k1 ^= (uint64_t)(tail[4]) << 32;
        case  4: k1 ^= (uint64_t)(tail[3]) << 24;
        case  3: k1 ^= (uint64_t)(tail[2]) << 16;
        case  2: k1 ^= (uint64_t)(tail[1]) << 8;
        case  1: k1 ^= (uint64_t)(tail[0]);
                 k1 *= c1; k1 = (k1 << 31) | (k1 >> (64 - 31)); k1 *= c2; h1 ^= k1;
    }

    // Finalize
    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 ^= (h1 >> 33);
    h1 *= 0xff51afd7ed558ccdULL;
    h1 ^= (h1 >> 33);
    h1 *= 0xc4ceb9fe1a85ec53ULL;
    h1 ^= (h1 >> 33);

    h2 ^= (h2 >> 33);
    h2 *= 0xff51afd7ed558ccdULL;
    h2 ^= (h2 >> 33);
    h2 *= 0xc4ceb9fe1a85ec53ULL;
    h2 ^= (h2 >> 33);

    h1 += h2;
    h2 += h1;

    return h1 ^ h2;
}

char *mmh3_64_hexdigest(const char *input, uint64_t seed) {
    uint64_t hash = mmh3_64(input, strlen(input), seed);

    char *hex = malloc(17);
    if (!hex) {
		return NULL;
	}

    snprintf(hex, 17, "%016llx", (unsigned long long)hash);
    return hex;
}

void mmh3_128(const void *key, const size_t len, const uint64_t seed, uint64_t *out) {
    const uint64_t  c1 = 0x87c37b91114253d5;
    const uint64_t  c2 = 0x4cf5ad432745937f;
    const uint8_t   *data = (const uint8_t *)key;
    const size_t    nblocks = len / 16;
    uint64_t        h1 = seed;
    uint64_t        h2 = seed;
    const uint64_t  *blocks = (const uint64_t *)(data);

    // Process the data in 128-bit chunks
    for (size_t i = 0; i < nblocks; i++) {
        uint64_t k1 = blocks[i * 2 + 0];
        uint64_t k2 = blocks[i * 2 + 1];

        k1 *= c1; k1 = (k1 << 31) | (k1 >> 33); k1 *= c2; h1 ^= k1;
        h1 = (h1 << 27) | (h1 >> 37); h1 += h2; h1 = h1 * 5 + 0x52dce729;

        k2 *= c2; k2 = (k2 << 33) | (k2 >> 31); k2 *= c1; h2 ^= k2;
        h2 = (h2 << 31) | (h2 >> 33); h2 += h1; h2 = h2 * 5 + 0x38495ab5;
    }

    // Process the remaining bytes
    const uint8_t *tail = (const uint8_t *)(data + nblocks * 16);
    uint64_t k1 = 0;
    uint64_t k2 = 0;

    switch (len & 15) {
        case 15: k2 ^= ((uint64_t)tail[14]) << 48;
        case 14: k2 ^= ((uint64_t)tail[13]) << 40;
        case 13: k2 ^= ((uint64_t)tail[12]) << 32;
        case 12: k2 ^= ((uint64_t)tail[11]) << 24;
        case 11: k2 ^= ((uint64_t)tail[10]) << 16;
        case 10: k2 ^= ((uint64_t)tail[9]) << 8;
        case  9: k2 ^= ((uint64_t)tail[8]) << 0;
                 k2 *= c2; k2 = (k2 << 33) | (k2 >> 31); k2 *= c1; h2 ^= k2;

        case  8: k1 ^= ((uint64_t)tail[7]) << 56;
        case  7: k1 ^= ((uint64_t)tail[6]) << 48;
        case  6: k1 ^= ((uint64_t)tail[5]) << 40;
        case  5: k1 ^= ((uint64_t)tail[4]) << 32;
        case  4: k1 ^= ((uint64_t)tail[3]) << 24;
        case  3: k1 ^= ((uint64_t)tail[2]) << 16;
        case  2: k1 ^= ((uint64_t)tail[1]) << 8;
        case  1: k1 ^= ((uint64_t)tail[0]) << 0;
                 k1 *= c1; k1 = (k1 << 31) | (k1 >> 33); k1 *= c2; h1 ^= k1;
    }

    // Finalization
    h1 ^= len;
	h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 ^= h1 >> 33;
    h1 *= 0xff51afd7ed558ccd;
    h1 ^= h1 >> 33;
    h1 *= 0xc4ceb9fe1a85ec53;
    h1 ^= h1 >> 33;

    h2 ^= h2 >> 33;
    h2 *= 0xff51afd7ed558ccd;
    h2 ^= h2 >> 33;
    h2 *= 0xc4ceb9fe1a85ec53;
    h2 ^= h2 >> 33;

    h1 += h2;
    h2 += h1;

    out[0] = h1;
    out[1] = h2;
}

