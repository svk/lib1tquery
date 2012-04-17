#include "mergetapes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

static int mertap_n;

#ifdef USE_MERTAP_ASSEMBLY
extern int mertap_cmp_asm1( uint32_t*, uint32_t* );
extern int mertap_cmp_asm2( uint32_t*, uint32_t* );
extern int mertap_cmp_asm3( uint32_t*, uint32_t* );
extern int mertap_cmp_asm4( uint32_t*, uint32_t* );
extern int mertap_cmp_asm5( uint32_t*, uint32_t* );

static int (*mertap_cmp_p)(uint32_t*, uint32_t*) = 0;

#define MERTAP_CMP( alpha, beta ) (mertap_cmp_p((alpha)->key, (beta)->key))

#else
static inline int mertap_cmp( struct mertap_record* alpha, struct mertap_record* beta) {
    for(int i=0;i<mertap_n;i++) {
        const int rv = ((int) alpha->key[i]) - ((int) beta->key[i]);
        if( rv ) return rv;
    }
    return 0;
}

#define MERTAP_CMP mertap_cmp

#endif

void mertap_initialize(int n) {
    mertap_n = n;
#ifdef USE_MERTAP_ASSEMBLY
    switch( n ) {
        case 1: mertap_cmp_p = mertap_cmp_asm1; break;
        case 2: mertap_cmp_p = mertap_cmp_asm2; break;
        case 3: mertap_cmp_p = mertap_cmp_asm3; break;
        case 4: mertap_cmp_p = mertap_cmp_asm4; break;
        case 5: mertap_cmp_p = mertap_cmp_asm5; break;
        default: fprintf( stderr, "error: mertap_initialize(n=%d): invalid n\n", n );
                 exit(1 );
    }
#endif
}

void mertap_heap_read_push_or_finish( struct mertap_heap* heap, struct mertap_file* f ) {
    int rv = mertap_file_read_peek( f );
    if( !rv ) {
        mertap_heap_push( heap, f );
    } else if( rv < 0 ) {
        fprintf( stderr, "warning: read error\n" );
    }
}

struct mertap_file* mertap_heap_pop( struct mertap_heap* heap ) {
    assert( heap->no_files > 0 );
    struct mertap_file *rv = heap->files[0];
    heap->files[0] = heap->files[ (heap->no_files--) - 1 ];

    int i = 0;
    while( MERTAP_HEAP_RIGHT(i) < heap->no_files ) {
            // both children
        const int lc = MERTAP_HEAP_LEFT(i), rc = MERTAP_HEAP_RIGHT(i);
        const int to_lc = MERTAP_CMP( &heap->files[i]->peek, &heap->files[lc]->peek );
        const int to_rc = MERTAP_CMP( &heap->files[i]->peek, &heap->files[rc]->peek );
        if( to_lc < 0 && to_rc < 0 ) return rv;

        const int lc_to_rc = MERTAP_CMP( &heap->files[lc]->peek, &heap->files[rc]->peek );
        if( lc_to_rc < 0 ) {
            void *temp = heap->files[lc];
            heap->files[lc] = heap->files[i];
            heap->files[i] = temp;

            i = lc;
        } else {
            void *temp = heap->files[rc];
            heap->files[rc] = heap->files[i];
            heap->files[i] = temp;

            i = rc;
        }
    }

    if( MERTAP_HEAP_LEFT(i) < heap->no_files ) {
        const int lc = MERTAP_HEAP_LEFT(i), rc = MERTAP_HEAP_RIGHT(i);
        const int to_lc = MERTAP_CMP( &heap->files[i]->peek, &heap->files[lc]->peek );
        if( to_lc > 0 ) {
            void *temp = heap->files[lc];
            heap->files[lc] = heap->files[i];
            heap->files[i] = temp;
        }
    }

    return rv;
}

void mertap_heap_push( struct mertap_heap *heap, struct mertap_file* f ) {
    assert( heap->no_files < MAX_HEAP_FILES );
    int i = heap->no_files++;
    heap->files[ i ] = f;
    while( i ) {
        const int pi = MERTAP_HEAP_PARENT(i);
        if( MERTAP_CMP( &heap->files[i]->peek, &heap->files[pi]->peek ) < 0 ) {
            void *temp = heap->files[pi];
            heap->files[pi] = heap->files[i];
            heap->files[i] = temp;

            i = pi;
        } else break;
    }
}

int mertap_loop( struct mertap_file *files, int no_files, int (*f)(struct mertap_record*,void*), void* arg) {
    struct mertap_heap heap;

    heap.no_files = 0;
    for(int i=0;i<no_files;i++) {
//        fprintf( stderr, "pushing file %p\n", &files[i] );
        mertap_heap_read_push_or_finish( &heap, &files[i] );
    }

    if( MERTAP_HEAP_EMPTY( &heap ) ) return 0;

    struct mertap_record buf;

    struct mertap_file *topfile = mertap_heap_pop( &heap );
//    fprintf( stderr, "popped file %p\n", topfile );
    memcpy( &buf, &topfile->peek, sizeof buf );
    mertap_heap_read_push_or_finish( &heap, topfile );

    while( !MERTAP_HEAP_EMPTY( &heap ) ) {
        topfile = mertap_heap_pop( &heap );
//        fprintf( stderr, "popped file %p w %lld\n", topfile, topfile->peek.count );
        if( !MERTAP_CMP( &buf, &topfile->peek ) ) {
            buf.count += topfile->peek.count;
        } else {
            if( f( &buf, arg ) ) return 1;
            memcpy( &buf, &topfile->peek, sizeof buf );
        }
        mertap_heap_read_push_or_finish( &heap, topfile );
    }

    if( f( &buf, arg ) ) return 1;

    return 0;
}

int mertap_file_open( struct mertap_file* f, const char *filename) {
    f->buffer = malloc( BUFFER_SIZE );
    if( !f->buffer ) {
        return 1;
    }
    f->sorted_input = gzopen( filename, "rb" );
    if( f->sorted_input == NULL ) {
        free( f->buffer );
        return 1;
    }
    f->fill = f->offset = 0;
//    fprintf( stderr, "succ init %p\n", f );
    return 0;
}

void mertap_file_close( struct mertap_file* f ) {
    if( f->sorted_input ) {
        gzclose( f->sorted_input );
        f->sorted_input = 0;
    }
    if( f->buffer ) {
        free( f->buffer );
        f->buffer = 0;
    }
    f->fill = f->offset = 0;
}

int mertap_file_read_peek( struct mertap_file* f ) {
    const int keysz = mertap_n * 4;
    const int sz = keysz + 8;
//    fprintf( stderr, "in %p fill %d offset %d\n", f, f->fill, f->offset );
    if( f->fill < sz ) {
        if( f->fill > 0 ) {
            memmove( f->buffer, &f->buffer[ f->offset ], f->fill );
        }
        f->offset = 0;
//        fprintf( stderr, "fill is %d buf is %p\n", f->fill, f->buffer );
        int bytes = gzread( f->sorted_input, &f->buffer[ f->fill ], BUFFER_SIZE - f->fill );
        if( !f->fill && !bytes ) return 1;
        if( bytes < 0 ) return -1;
        // TODO fix bytes==0

        f->fill += bytes;

        return mertap_file_read_peek( f );
    }
    memcpy( (void*) f->peek.key, &f->buffer[ f->offset ], keysz );
    memcpy( (void*) &f->peek.count, &f->buffer[ f->offset + keysz ], 8 );
//   fprintf( stderr, "memcp offset is %d fill is %d\n", f->offset, f->fill );
    f->offset += sz;
    f->fill -= sz;
//    fprintf( stderr, "amemcp offset is %d fill is %d\n", f->offset, f->fill );
    return 0;
}
