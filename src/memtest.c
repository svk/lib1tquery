#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    const int asize = 1024*1024;
    const int allocs = 4000;
    unsigned char *p[allocs];
    int m = 0;
    for(m=0;m<allocs;m++) {
        p[m] = malloc( asize );
        if( !p[m] ) {
            fprintf( stderr, "unable to allocate %lld bytes\n", (long long int) (m+1) * asize );
            ++m;
            break;
        }
        fprintf( stderr, "allocated %lld bytes\n", (long long int) (m+1) * asize );
        for(int i=0;i<asize;i++) {
            p[m][i] = i % 10;
        }
    }
    fprintf( stderr, "sleeping.\n" );
    sleep( 120 );
    unsigned long long int x = 0;
    for(int i=0;i<m;i++) {
        for(int j=0;j<asize;j++) {
            x += p[i][j];
        }
    }
    while( --m >= 0 ) {
        free(p[m]);
    }
    fprintf( stderr, "done.\n" );
    return 0;
}
