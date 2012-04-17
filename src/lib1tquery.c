#include "lib1tquery.h"

#include "iniparser.h"
#include <alloca.h>

#include "wordhash.h"
#include "hashbin.h"
#include "sfbti.h"

#include <string.h>

#include <assert.h>

#include <time.h>

#include "tarbind.h"

#include "sfbti.h"

#include "ngread.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


/* TODO consider threading */

struct lib1tquery_tree_context {
    int n;
    int bins;
    int prefix;

//    pthread_mutex_t lock;

    struct sfbti_tar_rctx *rctxs;
};

struct lib1tquery_context {
    WordHashCtx wordhash;
    char tar_filename[4096];
    struct lib1tquery_tree_context *grams[6];
    struct tarbind_context *tarbind_ctx;
    int do_mmap;
    int64_t *unigrams;

#define MAX_WEIGHTED_RCTX_PTRS 1000000
    struct sfbti_tar_rctx * leaf_weighted_tree_ptrs[ MAX_WEIGHTED_RCTX_PTRS ];
    int no_weighted_tree_ptrs;
};

static struct lib1tquery_context ctx;
static int library_initialized = 0;

const uint64_t assumed_total_leafs = 2142340 + 11195857 + 33470995 + 80131765;
uint64_t actual_total_leafs = 0;


void lib1tquery_suspend_tar(void) {
    close( ctx.tarbind_ctx->descriptor );
    fprintf( stderr, "[debug] suspended tar file\n" );
    ctx.tarbind_ctx->descriptor = -1;
}

void lib1tquery_reopen_tar(void) {
    fprintf( stderr, "[debug] reloading tar file from %s\n", ctx.tar_filename );
    ctx.tarbind_ctx->descriptor = open( ctx.tar_filename, O_RDONLY );
    assert( ctx.tarbind_ctx->descriptor != -1 );
    fprintf( stderr, "[debug] reloaded tar file from %s\n", ctx.tar_filename );
}

static long long ipow(long long a, long long b) {
    if( !b ) return 1;
    if( b == 1 ) return a;
    long long b1 = b/2;
    long long b2 = b - b1;
    return ipow( a, b1 ) * ipow( a, b2 );
}

#if 0
int lib1tquery_print_cache_stats(FILE *f ) {
    const uint64_t sz = sizeof (struct sfbti_cached_record);
    uint64_t total = 0;
    for(int i=0;i<6;i++) if( ctx.grams[i] ) {
        const int maxindex = ipow( ctx.grams[i]->bins, i );
        uint64_t local = 0;
        for(int j=0;j<maxindex;j++) {
            tarbind_seek_to( ctx.grams[i]->rctxs[j].binding );
            int rv = sfbti_tar_estimate_ab1_size( &ctx.grams[i]->rctxs[j], &local, 0 );
            if( rv ) {
                fprintf( f, "print_cache_stats(): error\n" );
            }
        }
        fprintf( f, "Internal node count for %d-gram (over %d files)s: %lu\n", i, maxindex, local );
        fprintf( f, "Bytes (at %lu per cached %d-node): %lu\n", sz, KEYS_PER_RECORD, sz * local );
        total += local;
    }
    fprintf( f, "Internal node count for all n-grams: %lu\n", total );
    fprintf( f, "Bytes (at %lu per cached %d-node): %lu\n", sz, KEYS_PER_RECORD, sz * total );
    return 0;
}
#endif

static void lib1tquery_free_tree_context( struct lib1tquery_tree_context *tctx ) {
    if( !tctx ) return;
    int maxindex = ipow( tctx->bins, tctx->n );
    for(int i=0;i<maxindex;i++) {
        sfbti_tar_close_rctx( &tctx->rctxs[i] );
    }
    free( tctx->rctxs );
//    pthread_mutex_destroy( &tctx->lock );
    free( tctx );
}

struct lib1tquery_tree_context* lib1tquery_make_tree_context( int n, const char *filename_prefix, int bins, int prefix_digits, int cache_level, struct tarbind_context *tarbind_context ) {
    struct lib1tquery_tree_context *rv = malloc(sizeof *rv);
    int64_t cache_spent = 0;
    int64_t leaves_reachable = 0;
    int64_t internals_cached = 0;
    time_t t0 = time(0);
    fprintf( stderr, "Loading tree[%d,%d,%d] from \"%s\"..", n, bins, prefix_digits, filename_prefix );
    if( rv ) do {
        rv->n = n;
        rv->bins = bins;
        rv->prefix = prefix_digits;
        
        int maxindex = ipow( bins, n );
        int sz = sizeof *rv->rctxs * maxindex;
        rv->rctxs = malloc( sz );
        if( !rv->rctxs ) break;
        memset( rv->rctxs, 0, sz );

        int xs[5];
        const char *suffix = ".sfbtin";

        long long j;
        for(j=0;j<maxindex;j++) {
            long long index = j;
            for(int k=0;k<n;k++) {
                int x = index % bins;
                index /= bins;
                xs[ n-1-k ] = x;
            }
            assert( n >= 1 );
            char filename[4096];
            strcpy( filename, filename_prefix );
            if( prefix_digits > 0 ) {
                char buf[1024];
                strcpy( buf, "" );
                for(int k=0;k<prefix_digits;k++) {
                    char buf2[8];
                    sprintf( buf2, "%c", xs[k] + 'a' );
                    strcat( buf, buf2 );
                }
                if( strlen(filename) > 0 && filename[ strlen(filename) - 1 ] != '/' ) {
                    strcat( filename, "/" );
                }
                strcat( filename, buf );
            }
            if( strlen(filename) > 0 && filename[ strlen(filename) - 1 ] != '/' ) {
                strcat( filename, "/" );
            }
            for(int k=prefix_digits;k<n;k++) {
                char buf2[8];
                sprintf( buf2, "%c", xs[k] + 'a' );
                strcat( filename, buf2 );
            }
            strcat( filename, suffix );

            int irv = sfbti_tar_open_rctx( tarbind_context, filename, &rv->rctxs[j], cache_level, n );
            if( irv ) {
                fprintf( stderr, "fatal error: load error on %s or out of memory\n", filename );
                break;
            }
/*
            if( irv || sfbti_os_suspend_rctx( &rv->rctxs[j] ) ) {
                fprintf( stderr, "fatal error: load/suspend error on %s or out of memory\n", filename );
                break;
            }
*/

            cache_spent += rv->rctxs[j].cached_bytes;
            internals_cached += rv->rctxs[j].cached_internal_nodes;
            leaves_reachable += rv->rctxs[j].leaves_reachable_by_cache;

            // testing.
            {
                const uint64_t k = rv->rctxs[j].leaves_reachable_by_cache;
                int slots = (int) (((double)k / (double) assumed_total_leafs) * (double) MAX_WEIGHTED_RCTX_PTRS);
                actual_total_leafs += k;
                for(int i=0;i<slots;i++) {
                    ctx.leaf_weighted_tree_ptrs[ ctx.no_weighted_tree_ptrs++ ] = &rv->rctxs[j];
                }
                fprintf( stderr, "tree %d: %d internal nodes, %d leaves reachable, got %d slots\n", j, rv->rctxs[j].cached_internal_nodes, rv->rctxs[j].leaves_reachable_by_cache, slots );
#ifdef EXCESSIVELY_VERBOSE
#endif
            }

        }
        if( j < maxindex ) {
            while( j >= 0 ) {
                sfbti_tar_close_rctx( &rv->rctxs[j] );
                j--;
            }
            free( rv->rctxs );
            rv->rctxs = 0;
            free( rv );
            rv = 0;
        }
    } while(0);
    if( !rv ) {
        fprintf( stderr, "failed.\n" );
    } else {
        time_t t1 = time(0);
        time_t dt = t1 - t0;
        fprintf( stderr, " using %lld bytes as cache %d level(s) beyond root.. ", cache_spent, cache_level );
        fprintf( stderr, " cached %lld internal nodes and can reach %lld leaves.. ", internals_cached, leaves_reachable );
        fprintf( stderr, " loaded in %d seconds.\n", dt );
    }
    return rv;
}

void lib1tquery_quit(void) {
    if( !library_initialized ) return;
    
    tarbind_free( ctx.tarbind_ctx );

    free_wordhashes( ctx.wordhash );

    free( ctx.unigrams );

    for(int i=0;i<=5;i++) {
        lib1tquery_free_tree_context( ctx.grams[i] );
    }

    sfbti_print_statistics( stderr );
}

int64_t *initialize_unigrams(const char *vocfile, int max, const char *transff ) {
    WordHashCtx trans_wh = 0;
    int64_t *rv = 0;
    struct ngr_file *f = 0;

    do {
        if( ! (trans_wh = read_wordhashes( transff ) ) ) break;
        if( ! (rv = malloc(sizeof *rv * max) ) ) break;
        if( ! (f = ngr_open( vocfile ) ) ) break;

        int error = 0;
        while( ngr_next( f ) ) {
            const char *word = ngr_s_col(f, 0);
            long long freq = ngr_ll_col(f, 1);
            int wordid = lookup_wordhash( trans_wh, word );
            if( wordid < 0 || wordid >= max ) {
                error = 1;
                break;
            }
            rv[ wordid ] += freq;
            rv[ 0 ] += freq;
        }
        if( error ) break;
        
        free_wordhashes( trans_wh );
        ngr_free( f );

        return rv;
    } while(0 );
    if( trans_wh ) {
        free_wordhashes( trans_wh );
    }

    if( rv ) {
        free( rv );
    }
    if( f ) {
        ngr_free( f );
    }
    return 0;
}

int lib1tquery_init(const char *inifile) {
    char * inifilename = alloca( strlen(inifile) + 1 );

    if( library_initialized ) {
        // TODO
        return 1;
    }

    strcpy( inifilename, inifile );

    fprintf( stderr, "Loading configuration file \"%s\".\n", inifilename );
    dictionary *ini = iniparser_load( inifilename );

    memset( &ctx, 0, sizeof ctx );

    do {
        int len;
        if( !ini ) break;

        const char *root = iniparser_getstring( ini, "general:root", "./" );

        char *suffix;
        char *filename;
        char *tarfilename;

        char *vocfilename;
        int voc_max_size = 0;
        char *transfilename;

        suffix = iniparser_getstring( ini, "general:tarfile", "trees.tar" );
        len = strlen(root) + strlen( suffix ) + 1;
        tarfilename = alloca( len );
        if( !tarfilename ) break;
        snprintf( tarfilename, len, "%s%s", root, suffix );

        memset( ctx.tar_filename, 0, sizeof ctx.tar_filename );
        snprintf( ctx.tar_filename, sizeof ctx.tar_filename - 1, "%s", tarfilename );
        fprintf( stderr, "[info] opening %s\n", ctx.tar_filename );


        suffix = iniparser_getstring( ini, "dictionary:location", "vocabulary.gz" );
        len = strlen(root) + strlen( suffix ) + 1;
        filename = alloca( len );
        if( !filename ) break;
        snprintf( filename, len, "%s%s", root, suffix );

        suffix = iniparser_getstring( ini, "unigrams:location", "vocab.gz" );
        len = strlen(root) + strlen( suffix ) + 1;
        vocfilename = alloca( len );
        if( !vocfilename ) break;
        snprintf( vocfilename, len, "%s%s", root, suffix );

        suffix = iniparser_getstring( ini, "dictionary:transformer-location", "vocabulary.gz" );
        len = strlen(root) + strlen( suffix ) + 1;
        transfilename = alloca( len );
        if( !transfilename ) break;
        snprintf( transfilename, len, "%s%s", root, suffix );

        voc_max_size = iniparser_getint( ini, "unigrams:size", 13588391 );

        fprintf( stderr, "Binding tar file \"%s\".\n", tarfilename );
        ctx.tarbind_ctx = tarbind_create( tarfilename );
        if( !ctx.tarbind_ctx ) break;

        int do_mmap = iniparser_getboolean( ini, "general:mmap", -1 );
        if( do_mmap == -1 ) {
            fprintf( stderr, "error: general.mmap not specified\n" );
            break;
        }

        if( do_mmap ) {
            fprintf( stderr, "Mapping tar file.. ");
            if( tarbind_prepare_mmap( ctx.tarbind_ctx ) ) break;
            fprintf( stderr, "done.\n" );
        } else {
            fprintf( stderr, "NOT memory-mapping tar file.\n" );
        }

        ctx.do_mmap = do_mmap;

        fprintf( stderr, "Loading dictionary from \"%s\"..", filename );
        ctx.wordhash = read_wordhashes( filename );
        if( !ctx.wordhash ) break;
        fprintf( stderr, "done.\n" );

        ctx.unigrams = initialize_unigrams( vocfilename, voc_max_size, transfilename );
        if( !ctx.unigrams ) break;

        int n;
        for(n=2;n<=5;n++) {
            char key[128];
            snprintf( key, sizeof key, "%dgrams:location", n );

            // changed: for tar-names wqe use the name directly
            filename = iniparser_getstring( ini, key, 0 );
            if( !filename ) {
                fprintf( stderr, "Skipping %d-grams.\n", n );
                continue;
            }
/*
            len = strlen(root) + strlen( suffix ) + 1;
            filename = alloca( len );
            if( !filename ) break;
            snprintf( filename, len, "%s%s", root, suffix );
*/

            int bins;
            int prefix;
            int cache_level = 0;

            snprintf( key, sizeof key, "%dgrams:bins", n );
            bins = iniparser_getint( ini, key, -1 );
            if( bins < 0 ) break;

            snprintf( key, sizeof key, "%dgrams:prefix", n );
            prefix = iniparser_getint( ini, key, -1 );
            if( prefix < 0 ) break;

            snprintf( key, sizeof key, "%dgrams:cache", n );
            cache_level = iniparser_getint( ini, key, 0 );
            if( cache_level < 0 ) break;

            ctx.grams[n] = lib1tquery_make_tree_context( n, filename, bins, prefix, cache_level, ctx.tarbind_ctx );
            if( !ctx.grams[n] ) break;
        }
        if( n <= 5 ) break;

        if( assumed_total_leafs != actual_total_leafs ) {
            fprintf( stderr, "Initialized WRONG, expected %lld leafs got %lld.\n", assumed_total_leafs, actual_total_leafs );
        } else {
            fprintf( stderr, "Initialized properly.\n" );
        }

        fprintf( stderr, "Initialized.\n" );
        library_initialized = 1;
    } while(0);

    if( ini ) {
        iniparser_freedict( ini );
    }

    if( !library_initialized ) {
        fprintf( stderr, "\nFailed.\n" );
        if( ctx.wordhash ) {
            free_wordhashes( ctx.wordhash );
        }
        for(int i=0;i<=5;i++) {
            lib1tquery_free_tree_context( ctx.grams[i] );
        }
    }

    return !library_initialized;
}

int32_t lib1tquery_dictionary( const char *s ) {
    assert( library_initialized );
    return lookup_wordhash( ctx.wordhash, s );
}

int lib1tquery_test_random_read(void) {
//    if( tarbind_random_read( ctx.tarbind_ctx, 2923 ) ) return 1;
    const int r = rand() % ctx.no_weighted_tree_ptrs;
    return sfbti_tar_random_read( ctx.leaf_weighted_tree_ptrs[r] );
}

void lib1tquery_debug_leafdump( int n) {
    struct lib1tquery_tree_context *g = ctx.grams[n];
    int maxindex = ipow( g->bins, g->n );
    for(int i=0;i<maxindex;i++) {
        tarbind_seek_to( g->rctxs[i].binding );
        sfbti_leafdump( &g->rctxs[i] );
    }
}


int64_t lib1tquery_lookup_ngram( int n, const int32_t* key ) {
    const struct lib1tquery_tree_context *t = ctx.grams[n];
    int ikey[5];

    if( n == 1 ) {
        return lib1tquery_lookup_1gram( key[0] );
    }

    assert( library_initialized );
    assert( t );

    uint32_t index = 0;
    for(int i=0;i<t->n;i++) {
        if( key[i] < 0 ) return 0;
        index *= t->bins;
        index += classify_uint32( t->bins, murmur_hash( (char*) &key[i], 4 ) );
        ikey[i] = key[i];
    }

    int64_t count;
    int rv;
    if( ctx.do_mmap ) {
        rv = sfbti_maptar_search( &t->rctxs[index], ikey, t->n, &count );
    } else {
        rv = sfbti_tar_search( &t->rctxs[index], ikey, t->n, &count );
    }

    if( rv ) {
        fprintf( stderr, "warning: lookup failed for" );
        for(int i=0;i<t->n;i++) {
            fprintf( stderr, "%d ", ikey[i] );
        }
        fprintf( stderr, "\n" );
        return 0;
    }

    return count;
}

int64_t lib1tquery_lookup_1gram( int32_t a ) {
    if( a < 0 ) return 0;
    return ctx.unigrams[a];
}

int64_t lib1tquery_lookup_2gram( int32_t a, int32_t b ) {
    const int32_t key[] = { a, b };
    return lib1tquery_lookup_ngram( 2, key );
}

int64_t lib1tquery_lookup_3gram( int32_t a, int32_t b, int32_t c ) {
    const int32_t key[] = { a, b, c };
    return lib1tquery_lookup_ngram( 3, key );
}

int64_t lib1tquery_lookup_4gram( int32_t a, int32_t b, int32_t c, int32_t d ) {
    const int32_t key[] = { a, b, c, d };
    return lib1tquery_lookup_ngram( 4, key );
}

int64_t lib1tquery_lookup_5gram( int32_t a, int32_t b, int32_t c, int32_t d, int32_t e ) {
    const int32_t key[] = { a, b, c, d, e };
    int64_t rv = lib1tquery_lookup_ngram( 5, key );
    return rv;
}

#ifdef TARBIND_LOG_SEEKS_DEBUG
void lib1tquery_log_seeks_to(FILE *f) {
    tarbind_log_seeks_to( f );
}
#endif
