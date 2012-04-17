#include "judysort.h"

#include <Judy.h>

int main(int argc, char *argv[]) {
    struct judysort_context ctx;

    judysort_initialize(&ctx);

    judysort_insert_test(&ctx, 10, 20, 30, 40, 50, 100 );
    judysort_insert_test(&ctx, 5, 4, 3, 2, 1, 100 );
    judysort_insert_test(&ctx, 5, 4, 3, 2, 1, 101 );
    judysort_insert_test(&ctx, 10, 20, 30, 40, 50, 101 );
    judysort_insert_test(&ctx, 1, 1, 1, 1, 1, 101 );
    judysort_insert_test(&ctx, 1, 2, 3, 4, 5, 101 );

    srand( 1337 );

    for(int i=0;i<10000;i++) {
        const int k = 1000;
        int a = 1337;
        int b = rand() % k;
        int c = rand() % k;
        int d = rand() % k;
        int e = rand() % k;
        judysort_insert_test(&ctx, a, b, c, d, e, 100 );
    }

    Word_t rv = judysort_dump_free(&ctx, 5, judysort_dump_output_test, 0 );

    fprintf( stderr, "memory used: %ld\n", rv );

    return 0;
}
