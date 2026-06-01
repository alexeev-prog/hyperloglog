#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <string.h>

static const uint32_t C1 = 0xcc9e2d51;
static const uint32_t C2 = 0x1b873593;

static inline uint32_t murmur3_fmix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static inline uint32_t murmur3_32(const void* key, size_t len, uint32_t seed) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;

    uint32_t h1 = seed;

    const uint32_t* blocks = (const uint32_t*)(data);

    for (int i = 0; i < nblocks; i++) {
        uint32_t k1 = blocks[i];

        k1 *= C1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= C2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;

    switch (len & 3) {
        case 3:
            k1 ^= (uint32_t)tail[2] << 16;
        case 2:
            k1 ^= (uint32_t)tail[1] << 8;
        case 1:
            k1 ^= (uint32_t)tail[0];
            k1 *= C1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= C2;
            h1 ^= k1;
    }

    h1 ^= len;

    return murmur3_fmix(h1);
}

static inline uint32_t murmur3_32_string(const char* str, uint32_t seed) {
    return murmur3_32(str, strlen(str), seed);
}

#endif    // HASH_H
