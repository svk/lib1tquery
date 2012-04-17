#ifndef H_MERGETAPES
#define H_MERGETAPES

#include <zlib.h>
#include <stdint.h>

#define MAX_N 5
#define MAX_HEAP_FILES 500
#define BUFFER_SIZE 4096

struct mertap_record {
    uint32_t key[ MAX_N ];
    uint64_t count;
};

struct mertap_file {
    gzFile sorted_input;

    struct mertap_record peek;

    unsigned char *buffer;
    int offset, fill;

};

struct mertap_heap {
    int no_files;
    struct mertap_file *files[ MAX_HEAP_FILES ];
};

void mertap_initialize(int);

int mertap_file_open( struct mertap_file*, const char * );
int mertap_file_read_peek( struct mertap_file* );
void mertap_file_close( struct mertap_file* );

void mertap_heap_push( struct mertap_heap*, struct mertap_file* );
struct mertap_file* mertap_heap_pop( struct mertap_heap* );
int mertap_heap_empty( struct mertap_heap* );
void mertap_heap_read_push_or_finish( struct mertap_heap*, struct mertap_file* );

#define MERTAP_HEAP_EMPTY(h) (!((h)->no_files))
#define MERTAP_HEAP_PARENT(n) (((n)-1)/2)
#define MERTAP_HEAP_LEFT(n) ((n)*2+1)
#define MERTAP_HEAP_RIGHT(n) ((n)*2+2)

int mertap_loop( struct mertap_file *, int, int (*)(struct mertap_record*,void*), void* );

#endif
