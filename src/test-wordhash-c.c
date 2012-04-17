#include <stdio.h>
#include <string.h>

#include "wordhash.h"

#define MAX_KEY_SIZE 1024

int main(int argc, char *argv[]) {
    WordHashCtx ctx = read_wordhashes( argv[1] );
    char buffer[ MAX_KEY_SIZE ];
    fprintf( stderr, "Ready for lookups.\n" );
    while(1) {
        if(!fgets( buffer, MAX_KEY_SIZE, stdin )) break;
        char *key = strtok( buffer, "\n" );
        int rv = lookup_wordhash( ctx, key );
        printf( "%s: %d\n", key, rv );
    }
    free_wordhashes( ctx );
    return 0;
}
