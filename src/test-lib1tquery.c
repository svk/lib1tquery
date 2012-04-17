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

#define MAX_BATCHES 200000

int queries_batches = 0;
int random_batches = 0;
uint64_t queries_batch[ MAX_BATCHES ];
uint64_t random_batch[ MAX_BATCHES ];

uint64_t microseconds_on_queries = 0;
uint64_t microseconds_on_random = 0;
uint64_t queries_performed = 0;
uint64_t random_performed = 0;

struct timeval global_timer_t0;

FILE *seeklog_randoms = 0;
FILE *seeklog_queries = 0;

void begin_timer(void) {
    gettimeofday( &global_timer_t0, 0 );
}

uint64_t end_timer(void) {
    struct timeval t1;
    gettimeofday( &t1, 0 );

    uint64_t t0us = global_timer_t0.tv_sec * 1000000L + global_timer_t0.tv_usec;
    uint64_t t1us = t1.tv_sec * 1000000L + t1.tv_usec;

    return t1us - t0us;
}

int process_random_batch( int no_random ) {
#ifdef TARBIND_LOG_SEEKS_DEBUG
    lib1tquery_log_seeks_to( seeklog_randoms );
#endif

    begin_timer();

    for(int i=0;i<no_random;i++) {
        lib1tquery_test_random_read();
    }

    microseconds_on_random += random_batch[ random_batches++ ] = end_timer();
    random_performed += no_random;

#ifdef TARBIND_LOG_SEEKS_DEBUG
    lib1tquery_log_seeks_to( 0 );
#endif
}

int process_query_batch( char *queries[][6], int no_queries ) {
    int32_t q[5];

#ifdef TARBIND_LOG_SEEKS_DEBUG
    lib1tquery_log_seeks_to( seeklog_queries );
#endif

    begin_timer();

    for(int i=0;i<no_queries;i++) {
        int j = 0;
        while( queries[i][j] && j < 5) {
            q[j] = lib1tquery_dictionary( queries[i][j] );
            j++;
        }
        if( queries[i][j] ) return 1;
#ifdef DO_DEBUG
        fprintf( stderr, "query: " );
        for(int k=0;k<j;k++) {
            fprintf( stderr, "%s ", queries[i][k] );
        }
        fprintf( stderr, "\n" );
#endif

        lib1tquery_lookup_ngram( j, q );
    }

    microseconds_on_queries += queries_batch[ queries_batches++ ] = end_timer();
    queries_performed += no_queries;

#ifdef TARBIND_LOG_SEEKS_DEBUG
    lib1tquery_log_seeks_to( 0 );
#endif

    return 0;
}

void print_stats( const char *name, uint64_t *ibatch, int per_batch, int no_batches ) {
    double batch[ no_batches ];
    double mean = 0;
    double variance = 0;
    for(int i=0;i<no_batches;i++) {
        batch[i] = (double) ibatch[i] / (double) per_batch;
    }
    printf( "%d samples (batches) of %s\n", no_batches, name );
    for(int i=0;i<no_batches;i++) {
        printf( "%lf ", batch[i] );
        mean += batch[i];
    }
    printf( "\n");
    mean /= (double) no_batches;
    for(int i=0;i<no_batches;i++) {
        variance += (batch[i]-mean) * (batch[i]-mean);
    }
    variance /= (double) (no_batches - 1); // un
    printf( "%s:\tSample mean: %lf\n\tUnbiased sample variance: %lf\n\tUnbiased sample standard deviation: %lf\n", name, mean, variance, sqrt( variance ) );
}

int main(int argc, char *argv[]) {
    int interactive = 0;
    FILE * infile;
    char *cachekiller = 0;
    const char *usagestr = "Usage: test-lib1tquery configfile.ini testfile batch_size number_of_batches [cachekiller command]\n";

    if( argc < 2 ) {
        fprintf( stderr, usagestr );
        return 1;
    }

    if( argc < 3 ) {
        fprintf( stderr, usagestr );
        return 1;
    } else {
        infile = fopen( argv[2], "r" );
        if( !infile ) {
            fprintf( stderr, "fatal error: unable to open test file \"%s\"\n", argv[2] );
            return 1;
        }
    }

    int batch_size = 100;
    int number_of_batches = 10;

    if( argc >= 4 ) {
        batch_size = atoi( argv[3] );
    } else {
        fprintf( stderr, usagestr );
        return 1;
    }
    if( argc >= 5 ) {
        number_of_batches = atoi( argv[4] );
    } else {
        fprintf( stderr, usagestr );
        return 1;
    }

    if( argc >= 6 ) {
        cachekiller = argv[5];
        fprintf( stderr, "Running with cache-killer command: %s\n", cachekiller );
    }

    int number_of_tests = batch_size * number_of_batches;
    fprintf( stderr, "Running with %d batches of size %d (%d tests)\n", batch_size, number_of_batches, number_of_tests );

    int just_print_stats = 1;


    const char *configfile = argv[1];

    time_t seed = time(0);

    fprintf( stderr, "Seeding with %d.\n", (int) seed );

#ifdef TARBIND_LOG_SEEKS_DEBUG
    seeklog_queries = fopen( "seeklog.queries.dat", "w" );
    seeklog_randoms = fopen( "seeklog.randoms.dat", "w" );
    assert( seeklog_queries && seeklog_randoms );
#endif

    srand( seed );

#if 0
    if( !interactive && proglog ) {
        char hostname[512];
        time_t now = time(0);
        char *atime = asctime( gmtime( &now ) );
        gethostname( hostname, sizeof hostname );
        hostname[ sizeof hostname - 1 ] = '\0';
        atime[ strlen(atime) - 1 ] = '\0';
        fprintf( proglog, "#beginning run %lu (%s)\n#at %s\n#from config %s\nfrom test queries %s\n", now, atime, hostname, configfile, argv[2] );
    }
#endif

    char buffer[4096], keybuf[4096];

    int failure = lib1tquery_init( configfile );
    if( failure ) {
        fprintf( stderr, "Failed to initialize, aborting.\n" );
        return 1;
    } 

#if 0
    if( just_print_stats ) {
        fprintf( stderr, "Just printing cache stats.\nThis could take a while to calculate..\n" );
        lib1tquery_print_cache_stats( stderr );
        lib1tquery_quit();
        return 0;
    }

    struct timeval t0, t1;
    uint64_t queries = 0;

    uint64_t queries_last = 0;
    struct timeval tv_last;

    int show_progress = 1;
    const int progress_per = 1000;

    if( !interactive && proglog ) {
        char hostname[512];
        time_t now = time(0);
        char *atime = asctime( gmtime( &now ) );
        gethostname( hostname, sizeof hostname );
        hostname[ sizeof hostname - 1 ] = '\0';
        atime[ strlen(atime) - 1 ] = '\0';
        fprintf( proglog, "#finished initialization of run %lu (%s)\n", now, atime );
    }

    fflush( proglog );
#endif

    fprintf( stderr, "Reading queries from file.\n" );

    const int max_ngram = 5;
    char *(*queries)[max_ngram+1] = malloc( sizeof *queries * number_of_tests );
    memset( queries, 0, sizeof *queries * number_of_tests );
    if( !queries ) {
        fprintf( stderr, "oops\n" );
        return 1;
    }
    for(int q=0;q<number_of_tests;q++) {
        if(!fgets( buffer, 4096, infile )) break;
        char *key = strtok( buffer, "\t\n" );
        strcpy( keybuf, key );
        char *colkey = strtok( buffer, " \t" );
        int i = 0;
        int toknotfound = 0;
        while( colkey ) {
            queries[q][i] = strdup( colkey );

            i++;
            colkey = strtok( 0, " \t" );
        }
        assert( i < (max_ngram+1) );
        queries[q][i] = 0;
    }

    if( cachekiller ) {
        lib1tquery_suspend_tar();
        fprintf( stderr, "Beginning cache kill with: %s\n", cachekiller );
        system( cachekiller );
        fprintf( stderr, "Finished cache kill with: %s\n", cachekiller );
        lib1tquery_reopen_tar();
    }

    fprintf( stderr, "Running interleaved tests.\n" );
    for(int b=0;b<number_of_batches;b++) {
        fprintf( stderr, "Batch %d.\n", b );
        process_query_batch( &queries[b * batch_size], batch_size );
        process_random_batch( batch_size );
    }

    for(int q=0;q<number_of_tests;q++) {
        for(int j=0;j<max_ngram;j++) {
            if( queries[q][j] ) {
                free(queries[q][j] );
            }
        }
    }
    free( queries );

    fprintf( stderr, "%lld us on %lld queries\n", microseconds_on_queries, queries_performed );
    fprintf( stderr, "%lld us on %lld random\n", microseconds_on_random, random_performed );

    print_stats( "queries", queries_batch, batch_size, queries_batches ); 
    print_stats( "random", random_batch, batch_size, random_batches ); 

#ifdef TARBIND_LOG_SEEKS_DEBUG
    fclose( seeklog_queries );
    fclose( seeklog_randoms );
#endif

#if 0
    fprintf( stderr, "Running.\n" );
    gettimeofday( &tv_last, 0 );
    gettimeofday( &t0, 0 );

    while(1) {
        if(!fgets( buffer, 4096, infile )) break;
        char *key = strtok( buffer, "\t\n" );
        strcpy( keybuf, key );
        char *colkey = strtok( buffer, " \t" );
        int i = 0;
        int toknotfound = 0;
        int32_t iseq[5];

        while( colkey ) {
            iseq[i] = lib1tquery_dictionary( colkey );

            i++;
            colkey = strtok( 0, " \t" );
        }

        uint64_t rv = lib1tquery_lookup_ngram( i, iseq );

        ++queries;
        fprintf( stdout, "%llu\n", rv );

        if( show_progress && (queries % progress_per) == 0) {
            gettimeofday( &t1, 0 );

            uint64_t last_mpics = tv_last.tv_sec * 1000000L + tv_last.tv_usec;
            uint64_t mics0 = t0.tv_sec * 1000000L + t0.tv_usec;
            uint64_t mpmics = t1.tv_sec * 1000000L + t1.tv_usec;
            uint64_t newq = queries - queries_last;

            uint64_t mpdelta_mics = mpmics - mics0;
            double avg_mic = (double) mpdelta_mics / (double) queries;

            uint64_t last_mpdelta_mics = mpmics - last_mpics;
            double lastavgmic = (double) last_mpdelta_mics / (double) newq;

            fprintf( stderr, "[progress] %llu processed, total [%lluq %llut %0.4lfp], last [%lluq %llut %0.4lfp]\n", queries, queries, mpdelta_mics, avg_mic, newq, last_mpdelta_mics, lastavgmic );
            if( proglog ) {
                time_t now = time(0);
                fprintf( proglog, "%lu\t%llu\t%0.4lf\t%0.4lf\n", now, queries, avg_mic, lastavgmic );
                fflush( proglog );
            }

            queries_last = queries;
            gettimeofday( &tv_last, 0 );
        }
    }

    gettimeofday( &t1, 0 );
    fprintf( stderr, "Stopped.\n" );

    uint64_t mics0 = t0.tv_sec * 1000000L + t0.tv_usec;
    uint64_t mics1 = t1.tv_sec * 1000000L + t1.tv_usec;
    
    if( !interactive ) {
        uint64_t delta_mics = mics1 - mics0;
        double avg_mic = (double) delta_mics / (double) queries;
        fprintf( stderr, "Took %llu microseconds to process %llu queries.\n", delta_mics, queries );
        fprintf( stderr, "Mean processing time: %0.4lf microseconds.\n", avg_mic );

        fclose( infile );

        if( proglog ) {
            char hostname[512];
            time_t now = time(0);
            char *atime = asctime( gmtime( &now ) );
            gethostname( hostname, sizeof hostname );
            hostname[ sizeof hostname - 1 ] = '\0';
            atime[ strlen(atime) - 1 ] = '\0';
            fprintf( proglog, "#ended run %lu (%s)\n#at %s\n#from config %s\n#", now, atime, hostname, configfile );
            fprintf( proglog, "#took %llu microseconds to process %llu queries.\n", delta_mics, queries );
            fprintf( proglog, "#mean processing time: %0.4lf microseconds.\n", avg_mic );
            fflush( proglog );
        }
    }
#endif

    fprintf( stderr, "Shutting down.\n" );
    lib1tquery_quit();
    fprintf( stderr, "Quitting.\n" );
#if 0

    if( !interactive && proglog ) {
        char hostname[512];
        time_t now = time(0);
        char *atime = asctime( gmtime( &now ) );
        gethostname( hostname, sizeof hostname );
        hostname[ sizeof hostname - 1 ] = '\0';
        atime[ strlen(atime) - 1 ] = '\0';
        fprintf( proglog, "#finished de-initialization of run %lu (%s)\n", now, atime );
        fclose( proglog );
    }
#endif

    return 0;
}
