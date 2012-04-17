#ifndef H_HASHBIN
#define H_HASHBIN

#include <stdint.h>

uint32_t murmur_hash( const char *, int );

int classify_uint32( const int, const uint32_t );

#endif
