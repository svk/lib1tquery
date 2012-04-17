#include "mergetapes.h"
#include <stdio.h>
#include <stdlib.h>

int test_output( struct mertap_record *rec, void *null) {
    for(int i=0;i<5;i++) {
        fprintf( stdout, "%u%s", rec->key[i], ((i+1) == 5) ? "\t" : " " );
    }
    fprintf( stdout, "%llu\n", rec->count );
    return 0;
}

int main(int argc, char *argv[]) {
    const int n = argc - 1;
    struct mertap_file files[ n ];

    mertap_initialize( 5 );

    int rv;

    for(int i=1;i<argc;i++) {
        rv = mertap_file_open( &files[i-1], argv[i] );
        if( rv ) {
            fprintf( stderr, "troubles opening file \"%s\"\n", argv[i] );
            exit(1);
        }
    }

    rv = mertap_loop( files, n, test_output, 0 );

    for(int i=0;i<n;i++) {
        mertap_file_close( &files[i] );
    }

    return 0;
}
