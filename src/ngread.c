#include "ngread.h"
#include <assert.h>
#include <stdlib.h>

#include <string.h>

#include <stdio.h>

struct ngr_file* ngr_open(const char *filename) {
    struct ngr_file *rv = malloc(sizeof *rv);
    if( rv ) {
        memset( rv, 0, sizeof *rv );
        rv->input = gzopen( filename, "rb" );
        rv->fill = 0;
        rv->linesize = 0;
        rv->newlines = 0;
    }
    return rv;
}

void ngr_free(struct ngr_file* ngrf) {
    gzclose( ngrf->input );
    free( ngrf );
}

int ngr_next(struct ngr_file* ngrf) {
    do {
        memmove( ngrf->buffer, &ngrf->buffer[ngrf->linesize], LINE_BUFFER_SIZE - ngrf->linesize );
        ngrf->fill -= ngrf->linesize;
        while( !ngrf->newlines && ngrf->fill < LINE_BUFFER_SIZE ) {
            if( ngrf->fill < LINE_BUFFER_SIZE ) {
                int rv = gzread( ngrf->input, &ngrf->buffer[ngrf->fill], LINE_BUFFER_SIZE - ngrf->fill );
                if( rv < 0 ) {
                    fprintf( stderr, "fatal error: read error\n" );
                    exit(1);
                }
                for(int i=0;i<rv;i++) {
                    if( ngrf->buffer[ngrf->fill+i] == '\n' ) {
                        ngrf->newlines++;
                    }
                }
                ngrf->fill += rv;
            }
            if( !ngrf->newlines ) {
                if( gzeof( ngrf->input ) ) {
                    if( ngrf->fill > 0 ) {
                        fprintf( stderr, "warning: no newline at end of file (last line ignored)\n" );
                    }
                    return 0;
                }

                if( ngrf->fill == LINE_BUFFER_SIZE ) {
                    fprintf( stderr, "fatal error: line too long\n" );
                    exit(1);
                }
            }
        }
        ngrf->columns = 1;
        assert( ngrf->newlines > 0 );
        ngrf->col[ 0 ] = ngrf->buffer;
        for(ngrf->linesize=0;ngrf->buffer[ngrf->linesize] != '\n';ngrf->linesize++) {
            if( ngrf->buffer[ngrf->linesize] == '\t'
                || ngrf->buffer[ngrf->linesize] == ' ' ) {
                ngrf->buffer[ngrf->linesize] = '\0';
                if( ngrf->columns >= MAX_COLUMNS ) {
                    fprintf( stderr, "fatal error: too many columns\n" );
                    exit(1);
                }
                ngrf->col[ ngrf->columns++ ] = &ngrf->buffer[ngrf->linesize+1];
            }
        }
        ngrf->buffer[ngrf->linesize] = '\0';
        --ngrf->newlines;

        ++ngrf->linesize;
    } while( ngrf->columns == 1 && strlen( ngrf->col[0] ) == 0 );

    return 1;
}

long long ngr_ll_col(struct ngr_file* ngrf, int j) {
    assert( ngrf->columns > j );
    return atoll( ngrf->col[j] );
}

const char* ngr_s_col(struct ngr_file* ngrf, int j) {
    assert( ngrf->columns > j );
    return ngrf->col[j];
}

int ngr_columns(struct ngr_file* ngrf) {
    return ngrf->columns;
}
