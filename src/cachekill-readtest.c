#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <signal.h>

#include <stdint.h>
#include <fcntl.h>
#include <string.h>

#include <stdlib.h>

#include <sys/time.h>
#include <time.h>

#include <math.h>

#include <errno.h>

static int doquit = 0;

void handler( int sig ) {
    doquit = 1;
}

int main(int argc, char *argv[]) {
    int fd;
    struct stat info;
    off_t seekrange;
    const int bs = 2923;
    int reportno = 100;
    int stop_after_samples = 1;
    const int no_samples = 100; // too many REDUCES accuracy since cache will have built up!

    double samples[ no_samples ];
    int samples_taken = 0;

    if( argc < 2 ) {
        fprintf( stderr, "usage:\n\t[cachekill/readtest] [data file]\n" );
        fprintf( stderr, "\tto generate an appropriate data file:\t\tdd if=dev/urandom of=DATAFILE bs=1048576 count=MEGABYTES\n" );
        return 1;
    }
    if( strstr( argv[0], "cachekill" ) && !strstr( argv[0], "readtest" ) ) {
        reportno = 10000;
        stop_after_samples = 0;
    }  else if( !strstr( argv[0], "cachekill" ) && strstr( argv[0], "readtest" ) ) {
        reportno = 100;
        stop_after_samples = 1;
    } else {
        fprintf( stderr, "error: cachekill or readtest? [rename binary appropriately]\n" );
        return 1;
    }
    fd = open( argv[1], O_RDONLY );
    if( fd == -1 ) {
        fprintf( stderr, "error: unable to open %s\n", argv[1] );
        return 1;
    }
    fprintf( stderr, "Opened file %s.\n", argv[1] );
    if( fstat( fd, &info ) ) {
        close( fd );
        fprintf( stderr, "error: unable to stat data file\n" );
        return 1;
    }
    fprintf( stderr, "Statted file, size: %llu\n", (uint64_t) info.st_size );
    seekrange = info.st_size - bs;
    if( seekrange < 0 ) {
        close( fd );
        fprintf( stderr, "error: file too small for block size %d\n", bs );
        return 1;
    }

    signal( SIGINT, handler );

    srand( time(0) );

    struct timeval tv0, tv_last;
    uint64_t seekread_count = 0;

    gettimeofday( &tv_last, 0 );
    gettimeofday( &tv0, 0 );

    fprintf( stderr, "Beginning reads..\n" );

    while( !doquit ) {
        uint64_t large_rand = ((uint64_t)rand()<<32) | rand();
        off_t target = large_rand % (seekrange + 1);
        char buffer[ bs ];
        if( lseek( fd, target, SEEK_SET ) == -1 ) {
            fprintf( stderr, "fatal error: seek error\n" );
            break;
        }
        if( read( fd, buffer, bs ) == -1 ) {
            fprintf( stderr, "fatal error: read error (errno %d) seeked to %lld\n", errno, target );
            break;
        }
        seekread_count += 1;
        if( seekread_count % reportno == 0 ) {
            struct timeval tv1;
            gettimeofday( &tv1, 0 );
            uint64_t tnow = (tv1.tv_sec * 1000000L + tv1.tv_usec);
            uint64_t tdelta = tnow - (tv_last.tv_sec * 1000000L + tv_last.tv_usec);
            uint64_t ttotal = tnow - (tv0.tv_sec * 1000000L + tv0.tv_usec);
            double microsecs_per_seekread_now = ((double) tdelta) / (double) reportno;
            double microsecs_per_seekread_avg = ((double) ttotal) / (double) seekread_count;

            printf( "%llu\t%llu\t%lf\t%lf\n", tnow, seekread_count, microsecs_per_seekread_now, microsecs_per_seekread_avg );
            fflush( stdout );

            gettimeofday( &tv_last, 0 );

            if( stop_after_samples ) {
                samples[ samples_taken++ ] = microsecs_per_seekread_now;
                if( samples_taken >= no_samples ) break;
            }
        }
    }

    if( !stop_after_samples || samples_taken < no_samples ) {
        fprintf( stderr, "Interrupted, quitting.\n" );
    } else {
        double mean = 0;
        double variance = 0;
        printf( "Took %d samples (each averaged from %d seek/reads): ", no_samples, reportno );
        for(int i=0;i<samples_taken;i++) {
            printf( "%lf ", samples[i] );
            mean += samples[i];
        }
        printf( "\n");
        mean /= (double) samples_taken;
        for(int i=0;i<samples_taken;i++) {
            variance += (samples[i]-mean) * (samples[i]-mean);
        }
        variance /= (double) (samples_taken - 1); // un
        printf( "Sample mean: %lf\nUnbiased sample variance: %lf\nUnbiased sample standard deviation: %lf\n", mean, variance, sqrt( variance ) );
    }

    close( fd );

    return 0;
}
