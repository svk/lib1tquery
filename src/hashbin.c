#include "hashbin.h"

int classify_uint32( const int no_bins, const uint32_t h ) {
    const uint32_t bs = (0xffffffffl / (unsigned long long) no_bins);
    return (h / bs) % no_bins;
}

uint32_t murmur_hash( const char *s, int len ) {
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    uint32_t h = 0x12345678 ^ len;

    const uint8_t *p = (const uint8_t*) s;

    while( len >= 4 ) {
        uint32_t k = *(uint32_t*)p;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        p += 4;
        len -= 4;
    }

    switch(len) {
        case 3: h ^= p[2] << 16;
        case 2: h ^= p[1] << 8;
        case 1: h ^= p[0];
                h *= m;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}
