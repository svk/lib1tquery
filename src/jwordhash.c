#include "wordhash.h"

#include "ngread.h"

#include <Judy.h>

WordHashCtx read_wordhashes(const char *filename) {
    Pvoid_t rv = 0;
    struct ngr_file * f = ngr_open( filename );
    while( ngr_next( f ) ) {
        const char *s = ngr_s_col( f, 0 );
        int v = (int) ngr_ll_col( f, 1 );
        Word_t *value;

        JSLI( value, rv, s );
        if( value == PJERR ) {
            fprintf( stderr, "error: problem inserting entry \"%s\"/%d into table\n", s, v );
            exit( 1 );
        }
        *value = v;
    }
    ngr_free( f );
    return (WordHashCtx) rv;
}

void free_wordhashes(WordHashCtx ctx) {
    Pvoid_t rv = (void*) ctx;
    Word_t c;
    JSLFA( c, rv );
}

int lookup_wordhash( WordHashCtx ctx, const char * key ) {
    Pvoid_t rv = (void*) ctx;
    Word_t *value;
    JSLG( value, rv, key );
    if( !value ) {
        return -1;
    }
    return (int) *value;
}
