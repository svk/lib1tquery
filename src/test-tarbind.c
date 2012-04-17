#include "tarbind.h"

#include <string.h>

#define MAX(a,b) (((a)<(b))?(b):(a))
#define MIN(a,b) (((a)>(b))?(b):(a))

int main(int argc, char *argv[]) {
    for(int i=1;i<argc;i++) {
        const char *tarfilename = argv[i];
        struct tarbind_context * ctx = tarbind_create( tarfilename );
        if( !ctx || tarbind_prepare_mmap( ctx ) ) {
            fprintf( stderr, "error: skipping \"%s\"..\n", tarfilename );
            continue;
        }

        const char *looking_for = "tarbind.test/hello/world";
        struct tarbind_binding * binding = tarbind_get_binding( ctx, looking_for );
        char buffer[512];

        if( !binding ) {
            fprintf( stderr, "no %s in %s\n", looking_for, tarfilename );
        } else {
            memcpy( buffer, binding->data, MIN( binding->length, sizeof buffer ) );
            buffer[sizeof buffer - 1 ] = '\0';
            fprintf( stderr, "%s in %s: %s\n", looking_for, tarfilename,  buffer);
        }

        tarbind_free( ctx );
    }

    return 0;
}
