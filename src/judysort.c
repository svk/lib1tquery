#include "judysort.h"

#include <assert.h>
#include <Judy.h>

#include <zlib.h>

void judysort_initialize(struct judysort_context* ctx) {
    ctx->pointer = 0;
    ctx->count = 0;
    fprintf( stderr, "Word_t is of size %d bytes\n", sizeof(Word_t) );
    if( sizeof(Word_t) < 8 ) {
        fprintf( stderr, "WARNING: judysort running on a 32-bit system -- counts WILL overflow\n" );
    }
}

union sortable_record_key {
    uint32_t symbols[MAX_SYMBOLS];
    uint8_t key[MAX_SYMBOLS*4];
};

void judysort_insert( struct judysort_context* ctx, const uint32_t* keys, int n, int64_t count) {
    Pvoid_t root_table = ctx->pointer;
    Word_t * jvalue = (void*) &root_table;

    Pvoid_t *current_table = &ctx->pointer;

    for(int i=0;i<n;i++) {
        JLI( jvalue, *current_table, keys[i] );
        if( jvalue == PJERR ) {
            fprintf( stderr, "judy table error (out of memory?)\n" );
            exit( 1 );
        }
        current_table = (void*) jvalue;
    }

    if( !*jvalue ) {
        ctx->count++;
    }

    *jvalue = (Word_t) (count + (uint64_t) *jvalue);
}

int judysort_get_count(struct judysort_context *ctx) {
    return ctx->count;
}

void judysort_insert_test( struct judysort_context* ctx, int a, int b, int c, int d, int e, int64_t count ) {
    uint32_t abcde[5] = { a, b, c, d, e };
    judysort_insert( ctx, abcde, 5, count );
}

Word_t judysort_dump_free_st(Pvoid_t jarr, int n, int i, int (*f)(int*,int,int64_t, void*), void *arg) {
    static int indices[ MAX_SYMBOLS ];
    Word_t * jvalue;
    Word_t index = 0;

    Word_t subtables_freed = 0;

    JLF( jvalue, jarr, index );

    if( jvalue == PJERR ) {
        fprintf( stderr, "judy table error (???)\n" );
        exit( 1 );
    }
    if( (i+1) == n ) {
        while( jvalue ) {
            indices[ i ] = (uint32_t) index;
            int err = f( indices, n, (int64_t) *jvalue, arg );
            if( err ) {
                fprintf( stderr, "output error\n" );
                exit( 1 );
            }
            JLN( jvalue, jarr, index );
        }
    } else {
        while( jvalue ) {
            indices[ i ] = (uint32_t) index;
            subtables_freed += judysort_dump_free_st( (void*) *jvalue, n, i + 1, f, arg );
            JLN( jvalue, jarr, index );
        }
    }

    Word_t rc_word;
    JLFA( rc_word, jarr );

    return rc_word + subtables_freed;
};

long long judysort_dump_free(struct judysort_context* ctx, int n, int (*f)(int*,int,int64_t,void*), void *arg ) {
    Word_t rv = judysort_dump_free_st( ctx->pointer, n, 0, f, arg );

    ctx->pointer = 0;
    ctx->count = 0;

    return rv;
}

int judysort_dump_output_test(int *indices, int n, int64_t count, void *null) {
    for(int i=0;i<n;i++) {
        printf( "%d%c", indices[i], ((i+1) == n) ? '\t' : ' ' );
    }
    printf( "%lld\n", count );
    return 0;
}

int judysort_dump_output_gzfile(int *indices, int n, int64_t count, void *arg) {
    gzFile *f = (void*) arg;
    uint32_t tmp;
    for(int i=0;i<n;i++) {
        tmp = indices[i];
        if( gzwrite( *f, (void*) &tmp, sizeof tmp ) != 4 ) return 1;
    }
    gzwrite( *f, (void*) &count, sizeof count );
    return 0;
}
