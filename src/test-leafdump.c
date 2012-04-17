#define _BSD_SOURCE

#include "lib1tquery.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#include <assert.h>

#include <math.h>

#include "sfbti.h"

int main(int argc, char *argv[]) {
    int interactive = 0;
    FILE * infile;
    const char *usagestr = "Usage: test-leafdump configfile.ini n\n";

    if( argc < 3 ) {
        fprintf( stderr, usagestr );
        return 1;
    }

    const char *configfile = argv[1];
    int n = atoi( argv[2] );

    int failure = lib1tquery_init( configfile );
    if( failure ) {
        fprintf( stderr, "Failed to initialize, aborting.\n" );
        return 1;
    } 

    fprintf( stderr, "Dumping %d-grams.\n", n );
    
    lib1tquery_debug_leafdump( n );

    fprintf( stderr, "Shutting down.\n" );
    lib1tquery_quit();
    fprintf( stderr, "Quitting.\n" );

    return 0;
}
