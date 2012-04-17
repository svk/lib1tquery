#include <stdio.h>

#include <assert.h>
#include <string.h>

#include "../lib/freegetopt-0.11/getopt.h"
#include "ngread.h"

#include <stdlib.h>

int main(int argc, char *argv[]) {
    int verbose = 0;
    while(1) {
        int c = getopt( argc, argv, "v" );
        if( c < 0 ) break;
        switch( c ) {
            case 'v':
                verbose = 1;
                break;
            default:
                fprintf(stderr, "unknown option: '%c'\n", c );
                exit(1);
        }
    }
    int argi = optind;

    while( argi < argc ) {
        const char *fn = argv[argi];
        struct ngr_file *ngrf = ngr_open( fn );
        int lineno = 1;

        if( !ngrf ) {
            fprintf( stderr, "warning: failed to open \"%s\", skipping.\n", fn );
            continue;
        }
        if( verbose ) {
            printf( "=== BEGIN === %s ===\n", fn );
        }

        while( ngr_next(ngrf) ) {
            printf( "%d: %d columns.\n", lineno, ngr_columns(ngrf) );
            for(int i=0;i<ngr_columns(ngrf);i++) {
                if( (i+1) == ngr_columns(ngrf) ) {
                    printf( "%d: (%d/ll) %lld.\n", lineno, i, ngr_ll_col(ngrf, i) );
                } else {
                    printf( "%d: (%d/s) \"%s\".\n", lineno, i, ngr_s_col(ngrf, i) );
                }
            }
            ++lineno;
        }

        if( verbose ) {
            printf( "=== END === %s ===\n", fn );
        }

        ngr_free( ngrf );
        argi++;
    }

}
