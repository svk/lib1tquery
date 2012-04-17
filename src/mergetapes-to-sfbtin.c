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
#include <sys/wait.h>
#include <unistd.h>

#include "mergetapes.h"

#define DESCRIPTORS_PER_BATCH 800
// limit tends to be 1024

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)>(b))?(b):(a))

int no_bins = -1;
int N = -1;
long long processed = 0;
long long hits = 0;
struct sfbti_wctx **histogram = 0;

#define MAX_PASSES 200

long long int *wcpattern_freqmax = 0;
long long int *wcpattern_freqmin = 0;
long long int *wcpattern_frequency = 0;

long long ipow(long long a, long long b) {
    if( !b ) return 1;
    if( b == 1 ) return a;
    long long b1 = b/2;
    long long b2 = b - b1;
    return ipow( a, b1 ) * ipow( a, b2 );
}

int output_to_sfbti(struct mertap_record* rec, void*arg) {
    long long index = 0;
    int iseq[ N ];
    int err;

    int wcpattern = 0;

    for(int i=0;i<N;i++) {
        index *= no_bins;
            // note we now take the hash of the INTEGER data
        index += classify_uint32( no_bins, murmur_hash( (char*) &rec->key[ i ], 4 ) );
        iseq[i] = rec->key[i];

        wcpattern <<= 1;
        if( iseq[i] == 0 ) {
            ++wcpattern;
        }
    }

    wcpattern_frequency[ wcpattern ]++;
    wcpattern_freqmax[ wcpattern ] = MAX( wcpattern_freqmax[ wcpattern ], rec->count );
    wcpattern_freqmin[ wcpattern ] = (wcpattern_freqmin[wcpattern] < 0 ) ? rec->count
                                     : MIN( wcpattern_freqmin[ wcpattern ], rec->count );

    if( histogram[ index ] ) {
        ++hits;
#if 0
        if( N == 5 ) {
            fprintf( stderr, "adding %d:%d:%d:%d:%d with count %lld\n", iseq[0], iseq[1], iseq[2], iseq[3], iseq[4], rec->count );
        }
#endif
        err = sfbti_add_entry( histogram[ index ], iseq, N, rec->count );
    } else err = 0;

    ++processed;

    return err;
}


int main(int argc, char* argv[]) {
    char *TFN = 0;
    int verbose = 0;
    int confirmed = 0;
    char *ODN = "hashbins-to-sfbtin.output";
    int prefix_digits = 0;
    char *suffix = ".sfbtin";
    int forking = 0;

    while(1) {
        int c = getopt( argc, argv, "vcFn:o:B:p:U:" );
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
            case 'o':
                ODN = optarg;
                break;
            case 'n':
                N = atoi( optarg );
                break;
            case 'F':
                forking = 1;
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

    long long memreq = sizeof *histogram * ipow( no_bins, N );
    long long totalmemreq = memreq + sizeof **histogram * ipow( no_bins, N );
    long long maxindex = ipow( no_bins, N );
    int *xs = malloc(sizeof *xs * N );
    assert( xs );

    wcpattern_frequency = malloc(sizeof *wcpattern_frequency * (1 << N) );
    assert( wcpattern_frequency );
    for(int i=0;i<(1<<N);i++) {
        wcpattern_frequency[i] = 0;
    }

    wcpattern_freqmax = malloc(sizeof *wcpattern_freqmax * (1 << N) );
    assert( wcpattern_freqmax );
    for(int i=0;i<(1<<N);i++) {
        wcpattern_freqmax[i] = 0;
    }

    wcpattern_freqmin = malloc(sizeof *wcpattern_freqmin * (1 << N) );
    assert( wcpattern_freqmin );
    for(int i=0;i<(1<<N);i++) {
        wcpattern_freqmin[i] = -1;
    }


    const int number_of_trees = maxindex;
    fprintf( stderr, "[mergetapes-to-sfbtin] generating %d trees, width %d, node size %d (cached %d)\n", number_of_trees, KEYS_PER_RECORD, sizeof (struct sfbti_record), sizeof (struct sfbti_cached_record) );
    int nodes_need_caching = number_of_trees;
    for(int i=0;i<=1;i++) {
        nodes_need_caching *= KEYS_PER_RECORD;
        nodes_need_caching += number_of_trees;
        fprintf( stderr, "[mergetapes-to-sfbtin] caching %d level(s) beyond root would require at most %d bytes\n", i, nodes_need_caching * sizeof (struct sfbti_cached_record) );
    }

    int no_passes = (maxindex+DESCRIPTORS_PER_BATCH-1) / DESCRIPTORS_PER_BATCH;
    int passes_min[ MAX_PASSES ];
    int passes_max[ MAX_PASSES ];
    for(int i=0;i<no_passes;i++) {
        passes_min[i] = DESCRIPTORS_PER_BATCH * i;
        passes_max[i] = MIN( maxindex - 1, DESCRIPTORS_PER_BATCH * (i+1) - 1 );
    }

    if( verbose ) {
        fprintf( stderr, "Parameters:\n" );
        fprintf( stderr, "\tn: %d-grams\n", N );
        fprintf( stderr, "\tB: %d hash bins\n", no_bins );
        if( verbose ) {
            fprintf( stderr, "\tinput from:\n" );
            for(int j=argi;j<argc;j++) {
                fprintf( stderr, "\t\t\"%s\"\n", argv[j] );
            }
            fprintf( stderr, "\toutput to:\n" );
        }
        fprintf(stderr, "Passes: %d\n", no_passes );
        for(int i=0;i<no_passes;i++) {
            fprintf( stderr, "\tPass #%d: %d-%d\n", i, passes_min[i], passes_max[i] );
        }
    }


    int passno = 1;

    mkdir( ODN, 0744 );
    long long fdno = 0;


    if( ipow( no_bins, N - prefix_digits ) > 10000 ) {
        fprintf( stderr, "fatal error: would create too many files per directory (use more prefix digits)\n" );
        return 1;
    } 

    if( totalmemreq > (10 * 1024 * 1024) && !confirmed ) {
        fprintf( stderr, "fatal error: need %lld bytes of memory, confirm with -c\n", totalmemreq );
        return 1;
    } else {
        fprintf( stderr, "Estimated memory usage: %lld bytes\n", totalmemreq );
    }

    histogram = malloc( memreq );
    if( !histogram ) {
        fprintf( stderr, "fatal error: unable to allocate %lld bytes of memory for histogram\n", memreq );
        return 1;
    }

    mertap_initialize( N );

    int no_files = argc - argi;
    struct mertap_file files[ no_files ];

#if 0
    for(int i=0;i<no_files;i++) {
        fprintf( stderr, "%p\n", &files[ no_files ] );
    }
#endif

    int chosen_pass = -1;
    pid_t children[ no_passes ];
    if( forking ) {
        int i;
        for(i=0;i<no_passes;i++) {
            children[ i ] = fork();
            if( children[i] == 0 ) {
                chosen_pass = i;
                fprintf( stderr, "Process covering pass %d: descriptors %d-%d\n", chosen_pass+1, passes_min[ chosen_pass ], passes_max[ chosen_pass ] );
                break;
            }
        }
        if( i == no_passes ) {
            for(i=0;i<no_passes;i++) {
                fprintf( stderr, "Child process #%d: %d\n", i, children[ i ] );
            }
        }
    }

    for(int passno=0;passno<no_passes;passno++) {
        if( forking && passno != chosen_pass ) {
            continue;
        }
        long long fdno = passes_min[ passno ];
        long long dmax = passes_max[ passno ] + 1;
        fprintf( stderr, "Beginning pass %d, covering files %lld to %lld.\n", passno+1, fdno, dmax-1 );
        for(long long j=0;j<maxindex;j++) {
            histogram[j] = 0;
        }
        for(long long j=fdno;j<dmax;j++) {
            long long index = j;
            for(int k=0;k<N;k++) {
                int x = index % no_bins;
                index /= no_bins;
                xs[ N-1-k ] = x;
            }
            assert( N >= 1 );
            char filename[4096];
            strcpy( filename, ODN );
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
            mkdir( filename, 0744 );
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
            histogram[j] = sfbti_new_wctx( filename, N );
            if( !histogram[j] ) {
                fprintf( stderr, "fatal error: failed to construct #%lld wctx (%s)\n", j, filename );
                return 1;
            }
        }

        processed = hits = 0;

        for(int i=0;i<no_files;i++) {
            int rv = mertap_file_open( &files[i], argv[argi+i] );
            if( rv ) {
                fprintf( stderr, "Problem opening file: \"%s\"\n", argv[argi+i] );
                exit( 1 );
            } else {
                fprintf( stderr, "Loaded file: \"%s\"\n", argv[argi+i] );
            }
        }
        
        int oerror = mertap_loop( files, no_files, output_to_sfbti, 0 );

        for(int i=0;i<no_files;i++) {
            mertap_file_close( &files[i] );
        }
        fprintf( stderr, "All files closed.\n" );

        fprintf( stderr, "%lld %d-grams processed (%lld were hits).\n", processed, N, hits );
        
        for(long long j=fdno;j<dmax;j++) {
            if( verbose ) {
                fprintf( stderr, "Finalizing SFBT %lld of %lld.\n", j + 1, dmax );
            }
            if( histogram[j] ) {
                int rv = sfbti_close_wctx( histogram[j] );
                if( rv ) {
                    fprintf( stderr, "fatal error: failure.\n" );
                    return 1;
                }
            }
        }

        fprintf( stderr, "Finished pass %d, covering files %lld to %lld.\n", passno+1, fdno, dmax-1 );
    }


    free(xs);
    free(histogram);

    if( forking && chosen_pass == -1 ) {
        for(int i=0;i<no_passes;i++) {
            int status;
            wait( &status );
            fprintf( stderr, "Child returned status: %d\n", status );
        }
        fprintf( stderr, "Success.\n" );
    } else {
        fprintf( stderr, "Child exit.\n" );
        fprintf( stderr, "Wildcarding statistics (total across all passes):\n\t" );
        for(int i=0;i<(1<<N);i++) {
            for(int j=0;j<N;j++) {
                fprintf( stderr, "%c", (i & (1<<(N-1-j))) ? '*' : '-' );
            }
            fprintf( stderr, "\t%lld\n", wcpattern_frequency[i] );
        }

        for(int i=0;i<(1<<N);i++) {
            fprintf( stderr, "((" );
            for(int j=0;j<N;j++) {
                fprintf( stderr, "%s%s", (i & (1<<(N-1-j))) ? "t" : "nil",
                                              ((j+1) == N) ? "" : " " );
            }
            fprintf( stderr, ") %lld %lld)", wcpattern_freqmin[i], wcpattern_freqmax[i] );
        }
    }

    free( wcpattern_frequency );
    free( wcpattern_freqmin );
    free( wcpattern_freqmax );

    return 0;
}
