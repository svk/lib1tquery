#include "ngread.h"
#include "hashbin.h"
#include "sfbti.h"

#include "../lib/freegetopt-0.11/getopt.h"

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "wordhash.h"

#include "judysort.h"

#include <Judy.h>

const char *next_name(void) {
    static char buffer[1024];
    static int count = 1;
    sprintf( buffer, "%08d", count++ );
    return buffer;
}

int lookup_judy( Pvoid_t jtable, const char *key ) {
    Word_t *jvalue;
    JSLG( jvalue, jtable, key );
    if( !jvalue ) return -1;
    if( jvalue == PJERR ) return -2;
    return (int) *jvalue;
}

#define LOOKUP(s) (transformerfile) ? lookup_judy( transformer, (s) ) : lookup_wordhash( words, (s) )

int main(int argc, char* argv[]) {
    int N = -1;
    int verbose = 0;
    char *prefix = "sorted.";
    char *suffix = ".tape.gz";
    char *vocfile = 0;
    int limit = 5000000;
    int add_wildcards = 0;
    char *transformerfile = 0;
    struct judysort_context judy_ctx;

    while(1) {
        int c = getopt( argc, argv, "vcWn:o:B:p:U:I:V:L:P:T:" );
        if( c < 0 ) break;
        switch( c ) {
            case 'T':
                transformerfile = optarg;
                break;
            case 'W':
                add_wildcards = 1;
                break;
            case 'P':
                prefix = optarg;
                break;
            case 'L':
                limit = atoi( optarg );
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
            case 'n':
                N = atoi( optarg );
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

    if( !vocfile && !transformerfile ) {
        fprintf( stderr, "fatal error: input arguments -V (vocabulary file) and -T (transformer) both missing\n" );
        return 1;
    }

    if( vocfile && transformerfile ) {
        fprintf( stderr, "fatal error: input arguments -V (vocabulary file) and -T (transformer) both supplied\n" );
        return 1;
    }



    char *fnbuffer = malloc(strlen(prefix) + strlen(suffix) + 1024);
    assert( fnbuffer );
    int *xs = malloc(sizeof *xs * N );
    assert( xs );

    int pp_unk_id;

    int wildcard_id = -1;
    
    if( verbose ) {
        fprintf( stderr, "Parameters:\n" );
        fprintf( stderr, "\tn: %d-grams\n", N );
        fprintf( stderr, "\tL: limit %d\n", limit );
        fprintf( stderr, "\tReading from stdin\n" );
        if( add_wildcards ) {
            fprintf( stderr, "\tAdding wildcards.\n" );
        }
        if( verbose ) {
            fprintf( stderr, "\tLoading:\n" );
        }
    }

    long long processed = 0;

    char buffer[ 4096 ], keybuf[ 4096 ];

    WordHashCtx words = 0;
    Pvoid_t transformer = 0;

    if( vocfile ) {
        fprintf( stderr, "Generating token hashes...\n" );
        WordHashCtx words = read_wordhashes( vocfile );
        fprintf( stderr, "Finished generating token hashes.\n" );
    } else if( transformerfile ) {
        fprintf( stderr, "Reading transformer...\n" );
        struct ngr_file *f = ngr_open( transformerfile );
        while( ngr_next( f ) ) {
            Word_t *jvalue;
            const char *key = ngr_s_col( f, 0 );
            int val = ngr_ll_col( f, 1 );

            JSLI( jvalue, transformer, key );
            if( !jvalue || jvalue == PJERR ) {
                fprintf( stderr, "Error reading transformer-file: %s on word \"%s\"\n", transformerfile, key );
                exit( 1 );
            } else {
                *jvalue = val;
            }
        }
        ngr_free( f );
        fprintf( stderr, "Finished reading transformer.\n" );
    }

    if( add_wildcards ) {
        wildcard_id = LOOKUP( "<*>" );
        assert( wildcard_id >= 0 );
    }

    pp_unk_id = LOOKUP( "<PP_UNK>" );
    assert( pp_unk_id >= 0 );


    gzFile output_file;


    judysort_initialize(&judy_ctx);

    while(1) {
        if(!fgets( buffer, 4096, stdin )) break;
        char *key = strtok( buffer, "\t" );
        strcpy( keybuf, key );
        char *countstr = strtok( 0, "\n" );
        char *colkey = strtok( buffer, " \t" );
        uint32_t isequence[ N ];
        int toknotfound = 0;
        uint64_t count = atoll( countstr );

        for(int i=0;i<N;i++) {
            if( !colkey ) break;
            int x = LOOKUP( colkey );
            if( x < 0 ) {
                fprintf( stderr, "warning: <PP_UNK>, \"%s\"\n", colkey );
                x = pp_unk_id;
            }
            isequence[i] = (uint32_t) x;

            colkey = strtok( 0, " \t" );
        }

        if( !add_wildcards ) {
            judysort_insert( &judy_ctx, isequence, N, count );

            ++processed;

            if( judysort_get_count(&judy_ctx) >= limit ) {
                sprintf( fnbuffer, "%s%s%s", prefix, next_name(), suffix );
                output_file = gzopen( fnbuffer, "wb" );
                fprintf( stderr, "Dumping to \"%s\" (%lld processed).\n", fnbuffer, processed );
                long long int memfreed = judysort_dump_free( &judy_ctx, N, judysort_dump_output_gzfile, &output_file );
                gzclose( output_file );
                fprintf( stderr, "Dumping done (used %lld bytes).\n", memfreed );
            }
        } else {
            int iseqa[ N ];
            const int k = 1 << N;
            for(int j=0;j<k;j++) {
                for(int i=0;i<N;i++) {
                    iseqa[ i ] = ((1<<i) & j) ? isequence[i] : wildcard_id;
                }
                judysort_insert( &judy_ctx, iseqa, N, count );

                ++processed;

                if( judysort_get_count(&judy_ctx) >= limit ) {
                    sprintf( fnbuffer, "%s%s%s", prefix, next_name(), suffix );
                    output_file = gzopen( fnbuffer, "wb" );
                    fprintf( stderr, "Dumping to \"%s\" (%lld processed, after %dx wildcarding).\n", fnbuffer, processed, k );
                    long long int memfreed = judysort_dump_free( &judy_ctx, N, judysort_dump_output_gzfile, &output_file );
                    gzclose( output_file );
                    fprintf( stderr, "Dumping done (used %lld bytes).\n", memfreed );
                }
            }

        }
    }

    sprintf( fnbuffer, "%s%s%s", prefix, next_name(), suffix );
    output_file = gzopen( fnbuffer, "wb" );
    fprintf( stderr, "Final-dumping to \"%s\" (%lld processed, %d unique).\n", fnbuffer, processed, judysort_get_count(&judy_ctx) );
    long long int memfreed = judysort_dump_free( &judy_ctx, N, judysort_dump_output_gzfile, &output_file );
    gzclose( output_file );
    fprintf( stderr, "Dumping done (used %lld bytes).\n", memfreed );

    if( transformerfile ) {
        Word_t rv;
        JSLFA( rv, transformer );
        fprintf( stderr, "Judy transformer used %ld bytes.\n", rv );
    }  else {
        free_wordhashes( words );
    }
    free( xs );
    free( fnbuffer );

    return 0;
}
