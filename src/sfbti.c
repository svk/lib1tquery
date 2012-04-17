#include "sfbti.h"
#include <stdio.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdint.h>

#define USE_VARSIZESTORE

static int sfbti_find_index(struct sfbti_record*, const int*, const int, int);

int sfbti_w_sizeofnode( struct sfbti_wctx* wctx ) {
#ifdef USE_VARSIZESTORE
    const int keyvals_size = KEYS_PER_RECORD * ( TOKEN_SIZE * wctx->ngram_size + SUFFIX_SIZE );
    const int total_size = keyvals_size + 2;
    return total_size;
#else
    return sizeof wctx->current_record;
#endif
}


int sfbti_w_writenode( struct sfbti_wctx* wctx, struct sfbti_record *rec) {
#ifdef USE_VARSIZESTORE
    const int keyvals_size = KEYS_PER_RECORD * ( TOKEN_SIZE * wctx->ngram_size + SUFFIX_SIZE );
    const int total_size = keyvals_size + 2;
    static uint8_t buffer[ sizeof *rec ];
    for(int i=0;i<KEYS_PER_RECORD;i++) {
        uint8_t *key = &buffer[ i * (TOKEN_SIZE * wctx->ngram_size + SUFFIX_SIZE ) ];
        uint8_t *value = &buffer[ (i+1) * TOKEN_SIZE * wctx->ngram_size + i * SUFFIX_SIZE ];
        memcpy( key, rec->keyvals[i], TOKEN_SIZE * wctx->ngram_size );
        memcpy( value, &rec->keyvals[i][KEY_SIZE], SUFFIX_SIZE );
    }
    buffer[ keyvals_size ] = rec->entries;
    buffer[ keyvals_size + 1 ] = rec->flags;
    if( FSF_FWRITE( buffer, total_size, 1, wctx->f ) != 1 ) return 1;
    return 0;
#else
    if( FSF_FWRITE( rec, sizeof *rec, 1, wctx->f ) != 1 ) return 1;
    return 0;
#endif
}

int sfbti_w_readnode( struct sfbti_wctx* wctx, struct sfbti_record *trec) {
#ifdef USE_VARSIZESTORE
    const int keyvals_size = KEYS_PER_RECORD * ( TOKEN_SIZE * wctx->ngram_size + SUFFIX_SIZE );
    const int total_size = keyvals_size + 2;
    static uint8_t buffer[ sizeof *trec ];
    if( FSF_FREAD( buffer, total_size, 1, wctx->f ) != 1 ) {
        return 1;
    }
    for(int i=0;i<KEYS_PER_RECORD;i++) {
        uint8_t *key = &buffer[ i * (TOKEN_SIZE * wctx->ngram_size + SUFFIX_SIZE ) ];
        uint8_t *value = &buffer[ (i+1) * TOKEN_SIZE * wctx->ngram_size + i * SUFFIX_SIZE ];
        memset( trec->keyvals[i], 0, KEY_SIZE );
        memcpy( trec->keyvals[i], key, TOKEN_SIZE * wctx->ngram_size );
        memcpy( &trec->keyvals[i][KEY_SIZE], value, SUFFIX_SIZE );
    }
    trec->entries = buffer[ keyvals_size ];
    trec->flags = buffer[ keyvals_size + 1];
    return 0;
#else
    if( FSF_FREAD( trec, sizeof *trec, 1, wctx->f ) != 1 ) {
        return 1;
    }
    return 0;
#endif
}

int sfbti_tar_readnode(struct sfbti_tar_rctx* rctx, struct sfbti_record* node ) {
#ifdef USE_VARSIZESTORE
    const int keyvals_size = KEYS_PER_RECORD * ( TOKEN_SIZE * rctx->ngram_size + SUFFIX_SIZE );
    const int total_size = keyvals_size + 2;
    static uint8_t buffer[ sizeof *node ];

    ssize_t done = 0;
    while( done < total_size ) {
        ssize_t rv = read( rctx->binding->context->descriptor, &buffer[done], sizeof *node - done );
        if( rv <= 0 ) {
            return 1;
        }
        done += rv;
    }
    for(int i=0;i<KEYS_PER_RECORD;i++) {
        uint8_t *key = &buffer[ i * (TOKEN_SIZE * rctx->ngram_size + SUFFIX_SIZE ) ];
        uint8_t *value = &buffer[ (i+1) * TOKEN_SIZE * rctx->ngram_size + i * SUFFIX_SIZE ];
/*
        if( i == 0 ) {
            fprintf( stderr, "first entry key: %02x %02x %02x %02x %02x %02x ...\n", (int) key[0], (int)  key[1], (int) key[2], (int)  key[3] , (int) key[4], (int)  key[5] );
            fprintf( stderr, "first value: %lld\n", *(uint64_t*)value );
        }
*/
        memset( node->keyvals[i], 0, KEY_SIZE );
        memcpy( node->keyvals[i], key, TOKEN_SIZE * rctx->ngram_size );
        memcpy( &node->keyvals[i][KEY_SIZE], value, SUFFIX_SIZE );
    }
/*
    fprintf( stderr, "dumping entire buffer:\n" );
    for(int i=0;i<total_size;i++) {
        fprintf(stderr, "%02x ", (int) buffer[i] );
    }
    fprintf(stderr, "\n" );
*/
    node->entries = buffer[ keyvals_size ];
    node->flags = buffer[ keyvals_size + 1];
//    fprintf(stderr, "resulting entries/flags: %d %d\n", node->entries, node->flags );
    return 0;
#else
    unsigned char *buf = (void*) node;
    ssize_t done = 0;
    while( done < sizeof *node ) {
        ssize_t rv = read( rctx->binding->context->descriptor, &buf[done], sizeof *node - done );
        if( rv <= 0 ) {
            return 1;
        }
        done += rv;
    }
    return 0;
#endif
}



static uint64_t stats_search_count = 0;
static uint64_t stats_search_read_count = 0;
static uint64_t stats_search_seek_count = 0;

#define CACHED_CHILD( node, i ) ( ( (node)->data.flags & FLAG_ENTRIES_ARE_CACHED ) ? (*((void**)&(node)->data.keyvals[(i)][KEY_SIZE])) : 0 )

void sfbti_free_cache(struct sfbti_cached_record* cache) {
    for(int i=0;i<KEYS_PER_RECORD;i++) if( CACHED_CHILD( cache, i ) ) {
        sfbti_free_cache( CACHED_CHILD( cache, i ) );
    }
    free( cache );
}

int sfbti_check_last_child_gen_at( struct sfbti_wctx* wctx, long foffset ) {
    // are there few enough nodes in this generation that the
    // next node will be the root node?

    do {
        FSF_FPOS cpos;
        if( FSF_FGETPOS( wctx->f, &cpos ) ) break;

        if( FSF_FSEEK( wctx->f, foffset, SEEK_SET ) ) break;

        int count = 0;

        while( count <= KEYS_PER_RECORD ) {
            struct sfbti_record buffer;
//            int rv = FSF_FREAD( &buffer, sizeof buffer, 1, wctx->f ); // SIZEUSE READUSE
//            if( rv != 1 ) {
            if( sfbti_w_readnode( wctx, &buffer ) ) {
                if( !FSF_FEOF( wctx->f ) ) return -1;
                FSF_CLEARERR( wctx->f );
                break;
            }

            count++;
        }

        // yes.
        if( FSF_FSETPOS( wctx->f, &cpos ) ) break;
        return count <= KEYS_PER_RECORD;
    } while(0);
    return -1;
}

int sfbti_write_collected_node( struct sfbti_wctx* wctx ) {
    // ok!
    do {
        struct sfbti_record rec;

        memset( &rec, 0, sizeof rec );

        for(int i=0; i < wctx->collected_children; i++) {
            memcpy( rec.keyvals[i], wctx->child_first_key[i], KEY_SIZE ); 

            memcpy( &rec.keyvals[i][KEY_SIZE], &wctx->child_offset[i], OFFSET_SIZE ); 
        }
        rec.entries = wctx->collected_children;
        rec.flags = 0;

        if( sfbti_w_writenode( wctx, &rec ) ) break;

        return 0;
    } while(0);
    return 1;
}

int sfbti_collect_children( struct sfbti_wctx* wctx, long stop_pos ) {
    struct sfbti_record buffer;
    unsigned char *firstkey = (unsigned char*) &buffer.keyvals[0][0];

    do {
        wctx->collected_children = 0;

        while( wctx->collected_children < KEYS_PER_RECORD ) {
            long foffset = FSF_FTELL( wctx->f );

            if( foffset >= stop_pos ) break;
                
            if( sfbti_w_readnode( wctx, &buffer ) ) {
                return 1;
            }

            memcpy( wctx->child_first_key[ wctx->collected_children ], firstkey, KEY_SIZE );
            wctx->child_offset[ wctx->collected_children ] = foffset;

            wctx->collected_children++;
        }
//        fprintf( stderr, "round of child-collecting finished at %d\n", wctx->collected_children );

        return 0;
    } while(0);
    return 1;
}

int sfbti_write_parents( struct sfbti_wctx* wctx ) {
    do {
        long next_gen_foffset = FSF_FTELL( wctx->f );

        long last_gen_cur = wctx->current_generation_foffset;
        FSF_FPOS cur;

        if( FSF_FGETPOS( wctx->f, &cur ) ) break;

        if( FSF_FSEEK( wctx->f, last_gen_cur, SEEK_SET ) ) break;
        if(sfbti_collect_children( wctx, next_gen_foffset )) break;
        last_gen_cur = FSF_FTELL( wctx->f );
        if( FSF_FSETPOS( wctx->f, &cur ) ) break;
        
        while( wctx->collected_children > 0 ) {
            if(sfbti_write_collected_node( wctx )) return 1;
            if( FSF_FGETPOS( wctx->f, &cur ) ) return 1;

            if( FSF_FSEEK( wctx->f, last_gen_cur, SEEK_SET ) ) return 1;
            if(sfbti_collect_children( wctx, next_gen_foffset )) return 1;
            last_gen_cur = FSF_FTELL( wctx->f );
            if( FSF_FSETPOS( wctx->f, &cur ) ) return 1;
        }

        wctx->current_generation_foffset = next_gen_foffset;
        return 0;
    } while(0);
    return 1;
}

int sfbti_write_root( struct sfbti_wctx* wctx ) {
    do {
        long t = FSF_FTELL( wctx->f );
        if( FSF_FSEEK( wctx->f, wctx->current_generation_foffset, SEEK_SET ) ) break;
        if(sfbti_collect_children( wctx, t )) break;
        fprintf( stderr, "[sfbti-finalize] root of tree \"%s\" has %d children\n", wctx->filename, wctx->collected_children );
        if( !wctx->collected_children ) {
            // should maybe fix this, but it is low-priority because it is not
            // necessary in the binning scheme: if the bin is empty, not
            // ... [logic fail. it is necessary. TODO]
            // [ besides, if I'm experiencing empty bins at all with the hashing
            //   scheme, that's highly likely to be a bug and so a warning is
            //   useful. ]
            fprintf( stderr, "warning: %s is empty and is left invalid\n", wctx->filename );
        } else {
            FSF_REWIND( wctx->f );
            if(sfbti_write_collected_node( wctx )) break;
        }

        return 0;
    } while(0);
    return 1;
}

int sfbti_finalize( struct sfbti_wctx* wctx ) {
    int height = 1;
    if( sfbti_flush_record( wctx ) ) return 1;
    do {
        int rv = sfbti_check_last_child_gen_at( wctx, wctx->current_generation_foffset );
        if( rv < 0 ) return 1;
        if( rv ) break;
        if( sfbti_write_parents( wctx ) ) return 1;
        height++;
    } while(1);
    height++;
    fprintf( stderr, "[sfbti-finalize] height of tree \"%s\" is %d\n", wctx->filename, height );
    if( sfbti_write_root( wctx ) ) return 1;
    return 0;
}

int sfbti_flush_record( struct sfbti_wctx* wctx ) {
    do {
//        if( FSF_FGETPOS( wctx->f, &cpos ) ) break;
//        fprintf( stderr, "wctx fpos before flush : %ld\n", cpos );

        /*

        for(int i=0;i< wctx->current_record.entries;i++) {
            int nonzero = 0;
            for(int j=0;j<KEY_SIZE;j++) {
                if( wctx->current_record.keyvals[i][j] ) nonzero = 1;
            }
            if( !nonzero ) {
                fprintf( stderr, "record has zero key: %d\n", i );
            }
        }
        */

        if( sfbti_w_writenode( wctx, &wctx->current_record ) ) break;
//        fprintf( stderr, "[beta]\n" );

//        if( FSF_FSETPOS( wctx->f, &cpos ) ) break;
//        fprintf( stderr, "[gamma]\n" );

        memset( &wctx->current_record, 0, sizeof wctx->current_record );


        return 0;
    } while(0);
    return 1;
}

int sfbti_new_leaf_record( struct sfbti_wctx* wctx ) {
    do {
        memset( &wctx->current_record, 0, sizeof wctx->current_record );

        wctx->current_record.entries = 0;
        wctx->current_record.flags = FLAG_ENTRIES_ARE_LEAVES;

        return 0;
    } while(0);
    return 1;
}

int sfbti_add_entry( struct sfbti_wctx* wctx, const int * key, int ngram_size, int64_t count) {
    do {

        if( wctx->current_record.entries == KEYS_PER_RECORD ) {
            if( sfbti_flush_record( wctx ) ) break;
            if( sfbti_new_leaf_record( wctx ) ) break;
        }
        assert( wctx->current_record.entries < KEYS_PER_RECORD );

        int i = wctx->current_record.entries++;
        for(int j=0;j<ngram_size;j++) {
            // assumes little endian!
            memcpy( &wctx->current_record.keyvals[i][TOKEN_SIZE*j], &key[j], TOKEN_SIZE );
        }
        assert( sizeof count == SUFFIX_SIZE );
        memcpy( &wctx->current_record.keyvals[i][KEY_SIZE], &count, sizeof count );

        return 0;
    } while(0);
    return 1;
}

struct sfbti_wctx *sfbti_new_wctx(const char * filename, int ngram_size) {
    struct sfbti_wctx *rv = malloc(sizeof *rv);
    int phase = 0;
    do {
        if( !rv ) break;
        ++phase;

        memset( rv, 0, sizeof *rv );
        ++phase;

        rv->ngram_size = ngram_size;

        strcpy( rv->filename, filename );
        ++phase;

#ifdef USE_SEMIFILES
        rv->f = semifile_fopen( filename, 1 );
#else
        rv->f = fopen( filename, "wb+" );
#endif
        if( !rv->f ) break;
        ++phase;

        if( FSF_FSEEK( rv->f, sfbti_w_sizeofnode(rv), SEEK_SET ) ) break;
        ++phase;

        rv->current_generation_foffset = FSF_FTELL( rv->f );
        ++phase;

        if( sfbti_new_leaf_record( rv ) ) break;
        ++phase;

        return rv;
    } while(0);
    fprintf( stderr, "warning: new_wctx failed at phase %d\n", phase );
    if( rv ) free(rv);
    return 0;
}

int sfbti_close_wctx(struct sfbti_wctx* wctx) {
    int rv = sfbti_finalize( wctx );

    if( wctx->f ) {
        FSF_FCLOSE( wctx->f );
    }
    free( wctx );

    return rv;
}

int sfbti_find_index(struct sfbti_record* buf, const int* key, const int ngram_size, int exact) {
    int mn = 0, mx = buf->entries - 1;

    while( mn != mx ) {
        int mp = (mn + mx + 1) / 2;
        assert( mn < mx );

        int cr = 0;

        for(int i=0;i<ngram_size;i++) {
            int t = (*(int*)(&buf->keyvals[mp][TOKEN_SIZE*i]) & TOKEN_INT_MASK);
            if( key[i] < t ) {
                // arg-key is earlier than rec-key
                mx = mp - 1;
                break;
            } else if( key[i] > t || (i+1) == ngram_size ) {
                // arg-key is later or equal to rec-key
                mn = mp;
                break;
            }
        }
    }
    assert( mn == mx );

    if( exact ) {
        for(int i=0;i<ngram_size;i++) {
            int t = (*(int*)(&buf->keyvals[mn][TOKEN_SIZE*i]) & TOKEN_INT_MASK);
            if( t != key[i] ) return -1;
        }
    }
    return mn;
}

#ifdef TEST
int main(int argc, char *argv[]) {
    const int N = 1000000;

    struct sfbti_record my;
    fprintf( stderr, "%d\n", sizeof my );

    struct sfbti_wctx *wctx = sfbti_new_wctx( "my.test.sfbti" );
    assert( wctx );
    for(int i=0;i<N;i++) {
        int vals[NGRAM_SIZE];
        int v = i;
        for(int j=0;j<NGRAM_SIZE;j++) {
            vals[NGRAM_SIZE-j-1] = v % 25;
            v /= 25;
        }
        if( (i%100000) == 0 ) {
            fprintf( stderr, "adding %d: %d, %d, %d, %d, %d\n", i, vals[0], vals[1], vals[2], vals[3], vals[4] );
        }
        if( sfbti_add_entry( wctx, vals, i + 1 ) ) {
            return 1;
        }
    }
    fprintf( stderr, "closing wctx\n" );
    if( sfbti_close_wctx( wctx ) ) {
        fprintf(stderr, "failed 2.\n" );
        return 1;
    }
    fprintf( stderr, "done writing\n" );

    struct sfbti_rctx rctx;
    if( sfbti_open_rctx( "my.test.sfbti", &rctx, 1 ) ) {
        fprintf( stderr, "readfail\n" );
        return 1;
    }
    fprintf( stderr, "Spending %d bytes on cache.\n", rctx.cached_bytes );

    fprintf( stderr, "Beginning reads.\n" );
    for(int i=0;i<N;i++) {
        int vals[NGRAM_SIZE];
        int v = i;
        for(int j=0;j<NGRAM_SIZE;j++) {
            vals[NGRAM_SIZE-j-1] = v % 25;
            v /= 25;
        }
        int64_t count;
        int rv = sfbti_search( &rctx, vals, &count );
        if( count != i + 1 ) {
            fprintf( stderr, "oops (%d). %d\n", i, rv );
            return 1;
        }
    }
    fprintf( stderr, "Finished reads.\n" );

    if( sfbti_close_rctx( &rctx ) ) {
        fprintf( stderr, "readclosefail\n" );
        return 1;
    }
    
    return 0;
}

#endif


#define USE_AB1

int sfbti_tar_open_rctx(struct tarbind_context *context, const char *filename, struct sfbti_tar_rctx* rctx, int depth, int ngram_size) {
    memset( rctx, 0, sizeof *rctx );

#ifdef EXCESSIVELY_VERBOSE
    fprintf( stderr, "[sfbti-tar] Getting binding for \"%s\"..", filename );
#endif
    rctx->binding = tarbind_get_binding( context, filename );
    if( !rctx->binding ) {
#ifdef EXCESSIVELY_VERBOSE
        fprintf( stderr, "failed!\n" );
#endif
        return 1;
    }
    tarbind_seek_to( rctx->binding );
#ifdef EXCESSIVELY_VERBOSE
    fprintf( stderr, "done.\n" );
#endif

    rctx->ngram_size = ngram_size;

    do {

#ifdef USE_AB1
#ifdef EXCESSIVELY_VERBOSE
        fprintf( stderr, "[sfbti-tar] AB1 caching!\n" );
#endif
        if( sfbti_tar_cache_node_ab1( rctx, &rctx->root ) ) {
            break;
        }
#else
        rctx->root = sfbti_tar_cache_node( rctx, depth );
        if( !rctx->root ) break;
#endif

        assert( strlen( rctx->filename ) < MAX_SFBT_FILENAME_LEN );
        strcpy( rctx->filename, filename );

        return 0;
    } while(0);
    if( rctx->root ) {
        sfbti_free_cache( rctx->root );
    }
    return 1;
}

int sfbti_tar_close_rctx(struct sfbti_tar_rctx* rctx) {
    // bindings are freed by tarbind
    return 0;
}

#if 0
// TODO reintroduce
struct sfbti_cached_record* sfbti_tar_cache_node(struct sfbti_tar_rctx* rctx, int depth) {
    struct sfbti_cached_record *rv = malloc(sizeof *rv);
    do {
        rctx->cached_bytes += sizeof *rv;
        if( !rv ) break;

        memset( rv, 0, sizeof *rv );

        if( sfbti_tar_readnode( rctx, &rv->data ) ) break;

        if( depth > 0 ) {
            int i;
            for(i=0;i<KEYS_PER_RECORD;i++) { //XXX?
                off_t* fpos = (off_t*) &rv->data.keyvals[i][KEY_SIZE]; // XXX TODO
                if( tarbind_seek_into( rctx->binding, *fpos ) ) break;
                rv->cached[i] = sfbti_tar_cache_node( rctx, depth - 1 );
                if( !rv->cached[i] ) break;
            }
            if( i < KEYS_PER_RECORD ) break;
        }

        return rv;
    } while(0);

    if( rv ) {
        for(int i=0;i<KEYS_PER_RECORD;i++) if( rv->cached[i] ) {
            sfbti_free_cache( rv->cached[i] );
        }
    }

    if( rv ) free(rv);

    return 0;
}
#endif

int sfbti_tar_random_read(struct sfbti_tar_rctx* rctx) {
    // assumes the layout of the trees: 1 root node, then all the leaves (then irrelevant nodes)
    const int leafno = rand() % rctx->leaves_reachable_by_cache;
    const int keyvals_size = KEYS_PER_RECORD * ( TOKEN_SIZE * rctx->ngram_size + SUFFIX_SIZE );
    const int total_size = keyvals_size + 2;
    const off_t leafoffset = total_size * (leafno + 1);
    unsigned char buffer[ total_size ];
    if( tarbind_seek_into( rctx->binding, leafoffset ) ) return 1;
    ssize_t rv = read( rctx->binding->context->descriptor, buffer, total_size );
    if( rv != 1 ) return 1;
    return 0;
}

int sfbti_tar_search(struct sfbti_tar_rctx* rctx, const int* key, const int ngram_size, int64_t* count_out) {
    struct sfbti_cached_record *cache = rctx->root;
    struct sfbti_record *node = &cache->data;

    while( !(node->flags & FLAG_ENTRIES_ARE_LEAVES) ) {
        int index = sfbti_find_index( node, key, ngram_size, 0 );

        if( index < 0 ) return 1;

        void *cache_ptr = cache ? CACHED_CHILD( cache, index ) : 0;

        if( cache_ptr ) {
            cache = cache_ptr;
            node = &cache->data;
        } else {
            off_t* fpos = (off_t*) &node->keyvals[index][KEY_SIZE]; // XXX TODO
//            if( ((off_t)-1) == lseek( rctx->binding->context->descriptor, *fpos, SEEK_SET ) ) return 1;
            if( tarbind_seek_into( rctx->binding, *fpos ) ) return 1;
            ++ stats_search_seek_count;
            if( sfbti_tar_readnode( rctx, &rctx->buffer ) ) return 1;
            ++ stats_search_read_count;
            cache = 0;
            node = &rctx->buffer;
        }
    }
    int index = sfbti_find_index( node, key, ngram_size, 1 );
    if( index < 0 ) {
        *count_out = 0;
    } else {
        int64_t* countp = (int64_t*)&node->keyvals[index][KEY_SIZE];
        *count_out = *countp;
    }

    ++ stats_search_count;

    return 0;
}

int sfbti_maptar_search(struct sfbti_tar_rctx* rctx, const int* key, const int ngram_size, int64_t* count_out) {
    struct sfbti_cached_record *cache = rctx->root;
    struct sfbti_record *node = &cache->data;

    while( !(node->flags & FLAG_ENTRIES_ARE_LEAVES) ) {
        int index = sfbti_find_index( node, key, ngram_size, 0 );

        if( index < 0 ) return 1;

        void *cache_ptr = cache ? CACHED_CHILD( cache, index ) : 0;

        if( cache_ptr ) {
            cache = cache_ptr;
            node = &cache->data;
        } else {
            uint32_t offset = *(uint32_t*)&node->keyvals[index][KEY_SIZE];
            cache = 0;
            node = (void*) &rctx->binding->data[ offset ];
        }
    }
    int index = sfbti_find_index( node, key, ngram_size, 1 );
    if( index < 0 ) {
        *count_out = 0;
    } else {
        int64_t* countp = (int64_t*)&node->keyvals[index][KEY_SIZE];
        *count_out = *countp;
    }

    ++ stats_search_count;

    return 0;
}

void sfbti_print_statistics(FILE *f) {
    fprintf( f, "Total searches run:\t%llu\n", stats_search_count );
    fprintf( f, "Total reads for search:\t%llu\n", stats_search_read_count );
    fprintf( f, "Total seeks for search:\t%llu\n", stats_search_seek_count );
    fprintf( f, "[last two only accurate for non-memory-mapped]\n" );
}

#include <time.h>

int sfbti_leafdump(struct sfbti_tar_rctx* rctx) {
    struct sfbti_record this_node;

    if( sfbti_tar_readnode( rctx, &this_node ) ) return 1;

    if( this_node.flags & FLAG_ENTRIES_ARE_LEAVES ) {
        printf( "(leaf %d\n", this_node.entries );
        for(int i=0;i<this_node.entries;i++) {
            uint8_t* kbuf = (uint8_t*) &this_node.keyvals[i][0];
            uint64_t* vbuf = (uint64_t*) &this_node.keyvals[i][KEY_SIZE];
            printf( "((" );
            for(int j=0;j<rctx->ngram_size;j++) {
                uint32_t z = 0;
                memcpy( &z, &kbuf[3*j], 3 ); // XXX le/be
                printf("%u%c", z, ((j+1) == rctx->ngram_size) ? ')' : ' ' );
            }
            printf( " %llu)%s\n", *vbuf, ((i+1) == this_node.entries) ? ")" : "" );
        }
    } else for(int i=0;i<this_node.entries;i++) {
        off_t* fpos = (off_t*) &this_node.keyvals[i][KEY_SIZE]; // XXX TODO
        if( *fpos == 0 ) continue;

        if( tarbind_seek_into( rctx->binding, *fpos ) ) return 1;

        if( sfbti_leafdump( rctx ) ) return 1;
    }
    
    return 0;
}

#if 0
int sfbti_tar_estimate_ab1_size(struct sfbti_tar_rctx* rctx, uint64_t *counter, int depth ) {
    struct sfbti_record this_node;

    int doprint = 0;

    do {
        if( sfbti_tar_readnode( rctx, &this_node ) ) break;
        if( this_node.flags & FLAG_ENTRIES_ARE_LEAVES ) {
            if( doprint ) {
                fprintf( stderr, "read a leaf! (depth %d)\n", depth );
            }
            return 0;
        }
        (*counter)++;
        if( doprint ) {
            fprintf( stderr, "read one node (depth %d).\n", depth);
            if( depth == 0 ) {
                for(int i=0;i<KEYS_PER_RECORD;i++) {
                    off_t* fpos = (off_t*) &this_node.keyvals[i][KEY_SIZE]; // XXX TODO
                    fprintf( stderr, "node positions: #%d -- %lu\n", i, (uint64_t) *fpos );
                }
            }
        }

        int i;
        for(i=0;i<KEYS_PER_RECORD;i++) {
            off_t* fpos = (off_t*) &this_node.keyvals[i][KEY_SIZE]; // XXX TODO
            if( *fpos == 0 ) continue;

            if( tarbind_seek_into( rctx->binding, *fpos ) ) break;
            if( doprint ) {
                fprintf( stderr, "reading child #%d at fpos %lu (depth %d)..\n", i, (uint64_t) *fpos, depth + 1 );
            }
            if( sfbti_tar_estimate_ab1_size( rctx, counter, depth + 1 ) ) break;
        }
        if( i < KEYS_PER_RECORD ) break;

        return 0;
    } while(0);

    return 1;
}
#endif

int sfbti_tar_cache_node_ab1(struct sfbti_tar_rctx* rctx, struct sfbti_cached_record **ptr) {
    struct sfbti_record this_node;
    struct sfbti_cached_record *rv = 0;
    void *p[KEYS_PER_RECORD];
    for(int i=0;i<KEYS_PER_RECORD;i++) {
        p[i] = 0;
    }
    do {
//        fprintf( stderr, "reading a node..\n" );
        if( sfbti_tar_readnode( rctx, &this_node ) ) break;

//        fprintf( stderr, "read a node with %d entries %d flags\n", this_node.entries, this_node.flags );
//        fprintf( stderr, "first entry key: %02x %02x %02x %02x %02x %02x ...\n", (int) this_node.keyvals[0][0], (int)  this_node.keyvals[0][1], (int) this_node.keyvals[0][2], (int)  this_node.keyvals[0][3] , (int) this_node.keyvals[0][4], (int)  this_node.keyvals[0][5] ); 

        if( ! (this_node.flags & FLAG_ENTRIES_ARE_LEAVES) ) {
            rv = malloc(sizeof *rv);
            if( !rv ) break;
            memset( rv, 0, sizeof *rv );
            memcpy( &rv->data, &this_node, sizeof rv->data );
            rctx->cached_bytes += sizeof *rv;
            rctx->cached_internal_nodes++;

            int children_cached = 0;
            int i;
            for(i=0;i<rv->data.entries;i++) { //XXX?
                off_t* fpos = (off_t*) &rv->data.keyvals[i][KEY_SIZE]; // XXX TODO
                if( tarbind_seek_into( rctx->binding, *fpos ) ) break;
                if( sfbti_tar_cache_node_ab1( rctx, (struct sfbti_cached_record**) &p[i] ) ) {
                    break;
                }
                if( p[i] ) {
                    ++children_cached;
                }
            }

            if( i < rv->data.entries ) break;
            if( rv->data.entries == children_cached ) {
                rv->data.flags |= FLAG_ENTRIES_ARE_CACHED;
                for(i=0;i<rv->data.entries;i++) {
                    *((void**) &rv->data.keyvals[i][KEY_SIZE]) = p[i];
                }
            } else {
                assert( children_cached == 0 );
            }

            *ptr = rv;
        } else {
            rctx->leaves_reachable_by_cache++;
            *ptr = 0;
        }

        return 0;
    } while(0);

    for(int i=0;i<KEYS_PER_RECORD;i++) if( p[i] ) {
        sfbti_free_cache( p[i] );
    }

    if( rv ) free(rv);

    return 1;
}
