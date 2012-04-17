#include "ngread.h"
#include "hashbin.h"
#include "sfbti.h"
#include "tarbind.h"

#include "../lib/freegetopt-0.11/getopt.h"

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "wordhash.h"

long long ipow(long long a, long long b) {
    if( !b ) return 1;
    if( b == 1 ) return a;
    long long b1 = b/2;
    long long b2 = b - b1;
    return ipow( a, b1 ) * ipow( a, b2 );
}

int main(int argc, char* argv[]) {
    int N = -1;
    int no_bins = -1;
    char *TFN = 0;
    int verbose = 0;
    int confirmed = 0;
    char *IDN = 0;
    char *ITN = 0;
    int prefix_digits = 0;
    char *suffix = ".sfbtin";
    char *vocfile = 0;

    struct tarbind_context *tarctx = 0;

    while(1) {
        int c = getopt( argc, argv, "vcn:o:B:p:U:I:V:T:" );
        if( c < 0 ) break;
        switch( c ) {
            case 'c':
                confirmed = 1;
                break;
            case 'U':
                if( optarg ) {
                    suffix = optarg;
                } else {
                    suffix = "";
                }
                break;
            case 'V':
                vocfile = optarg;
                break;
            case 'I':
                IDN = optarg;
                break;
            case 'T':
                ITN = optarg;
                break;
            case 'n':
                N = atoi( optarg );
                break;
            case 'B':
                no_bins = atoi( optarg );
                break;
            case 'p':
                prefix_digits = atoi( optarg );
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                fprintf(stderr, "unknown option: '%c'\n", c );
                exit(1);
        }
    }
    int argi = optind;

    if( N < 0 ) {
        fprintf( stderr, "fatal error: input argument -n (length of n-grams) missing\n" );
        return 1;
    }

    if( no_bins < 0 ) {
        fprintf( stderr, "fatal error: input argument -B (number of hash bins) missing\n" );
        return 1;
    }

    if( !IDN ) {
        fprintf( stderr, "fatal error: input argument -I (input directory) missing\n" );
        return 1;
    }

    if( !ITN ) {
        fprintf( stderr, "fatal error: input argument -I (input tar file) missing\n" );
        return 1;
    }



    struct sfbti_tar_rctx *histogram = 0;
    long long memreq = sizeof *histogram * ipow( no_bins, N );
    long long maxindex = ipow( no_bins, N );
    int *xs = malloc(sizeof *xs * N );
    assert( xs );

    histogram = malloc( memreq );
    if( !histogram ) {
        fprintf( stderr, "fatal error: unable to allocate %lld bytes of memory for histogram\n", memreq );
        return 1;
    }

    if( verbose ) {
        fprintf( stderr, "Parameters:\n" );
        fprintf( stderr, "\tn: %d-grams\n", N );
        fprintf( stderr, "\tB: %d hash bins\n", no_bins );
        if( verbose ) {
            fprintf( stderr, "\tLoading:\n" );
        }
    }

    tarctx = tarbind_create( ITN );

    for(long long j=0;j<maxindex;j++) {
        long long index = j;
        for(int k=0;k<N;k++) {
            int x = index % no_bins;
            index /= no_bins;
            xs[ N-1-k ] = x;
        }
        assert( N >= 1 );
        char filename[4096];
        strcpy( filename, IDN );
        if( prefix_digits > 0 ) {
            char buf[1024];
            strcpy( buf, "" );
            for(int k=0;k<prefix_digits;k++) {
                char buf2[8];
                sprintf( buf2, "%c", xs[k] + 'a' );
                strcat( buf, buf2 );
            }
            strcat( filename, "/" );
            strcat( filename, buf );
        }
        strcat( filename, "/" );
        for(int k=prefix_digits;k<N;k++) {
            char buf2[8];
            sprintf( buf2, "%c", xs[k] + 'a' );
            strcat( filename, buf2 );
        }
        strcat( filename, suffix );
        if( verbose ) {
            fprintf( stderr, "\t\t\"%s\"\n", filename );
        }
        int rv = sfbti_tar_open_rctx( tarctx, filename, &histogram[j], 0, N );
        if( rv ) {
            fprintf( stderr, "fatal error: load/suspend error on %s or out of memory\n", filename );
            return 1;
        }
    }

    long long processed = 0;

    char buffer[ 4096 ], keybuf[ 4096 ];

    fprintf( stderr, "Generating token hashes...\n" );
    WordHashCtx words = read_wordhashes( vocfile );
    fprintf( stderr, "Finished generating token hashes.\n" );

    fprintf( stderr, "Ready for lookups.\n" );

    while(1) {
        if(!fgets( buffer, 4096, stdin )) break;
        char *key = strtok( buffer, "\t\n" );
        strcpy( keybuf, key );
        char *colkey = strtok( buffer, " \t" );
        int index = 0;
        int i;
        int isequence[ N ];
        int toknotfound = 0;
        for(i=0;i<N;i++) {
            if( !colkey ) break;

            isequence[i] = lookup_wordhash( words, colkey );
            uint32_t wordkey = isequence[i];

                // XXX changed!
			int n = classify_uint32( no_bins, murmur_hash( (char*) &wordkey, 4 ) );
            index *= no_bins;
            index += n;
#if 0
			if( verbose ) {
				fprintf( stderr, "Classified \"%s\" as %d\n", colkey, n );
                fprintf( stderr, "Looked up \"%s\" as %d\n", colkey, isequence[i] );
			}
#endif

            if( isequence[i] < 0 ) {
                toknotfound = 1;
            }

            colkey = strtok( 0, " \t" );
        }
        if( i != N ) {
            fprintf( stderr, "warning: input \"%s\" invalid (not enough columns), ignoring\n", keybuf );
            continue;
        }
		if( verbose ) {
			fprintf( stderr, "Classified \"%s\" as %d (%s)\n", keybuf, index, histogram[index].filename );
			fprintf( stderr, "isequence: %d", isequence[0] );
            for(int j=1;j<N;j++) {
                fprintf( stderr, ":%d", isequence[j] );
            }
            fprintf( stderr, "\n" );
		}
        int64_t count = 0;
        int rv = 0;
        if( !toknotfound ) {
            rv = sfbti_tar_search( &histogram[index], isequence, N, &count );
        }
        if( rv ) {
            fprintf( stderr, "warning: lookup error on \"%s\"\n", keybuf );
        } else {
            fprintf( stdout, "%s\t%lld\n", keybuf, count );
        }
        ++processed;
    }

    if( verbose ) {
        fprintf( stderr, "%lld queries processed.\n", processed );
    }

    for(long long j=0;j<maxindex;j++) {
		sfbti_tar_close_rctx( &histogram[j] );
	}

    free(histogram);
    free_wordhashes( words );

    tarbind_free( tarctx );

    return 0;
}
