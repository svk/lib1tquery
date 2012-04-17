#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if( argc < 1 ) {
        printf( "usage: test-fdlimit n\n" );
        return 1;
    }
    int n = atoi( argv[1] );
    char buffer[1024];
    char template[]="/usit/sh/scratch/steinavk/test-fdlimit.output/testfile%08d";
    FILE *f[n];
    for(int i=0;i<n;i++) {
        snprintf( buffer, sizeof buffer - 1, template, i );
        f[i] = fopen( buffer, "w" );
        if( !f[i] ) {
            fprintf( stderr, "fatal error: failed to open #%d: %s\n", n, buffer );
            while( --i >= 0 ) {
                fclose( f[i] );
            }
            return 1;
        }
    }
    for(int j=0;j<512;j++) {
        for(int i=0;i<n;i++) {
            fputs( "Hello world.\n", f[i] );
        }
    }
    for(int i=0;i<n;i++) {
        fclose(f[i]);
    }

    return 0;
}
