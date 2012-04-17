#ifndef H_SFBT
#define H_SFBT

#include <stdint.h>
#include <stdio.h>

#include "tarbind.h"

#include "semifile.h"

#define USE_OS
#define USE_TAR
#define USE_MAPTAR

/* Single-file B-tree, integer variant.

   Tokens are sorted by integer value, so if alphabetical
   order is assumed anywhere else make sure the
   perfect hashes are assigned in order.
   Also note difference between:
        AAA BBB
        AAAAA BBB
    and
        1,3
        2,3
   
   Comments in sfbt.c also apply.
*/

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define KEYS_PER_RECORD 127

#define NGRAM_SIZE 5
#define TOKEN_SIZE 3
#define KEY_SIZE (NGRAM_SIZE*TOKEN_SIZE)
#define TOKEN_INT_MASK 0x00ffffff

#define OFFSET_SIZE 4
#define COUNT_SIZE 8
#define SUFFIX_SIZE MAX(COUNT_SIZE, OFFSET_SIZE)
#define SUFFIX_OFFSET NGRAM_SIZE*KEY_SIZE

#define MAX_SFBT_FILENAME_LEN 1024

#define FLAG_ENTRIES_ARE_LEAVES 0x80
#define FLAG_ENTRIES_ARE_CACHED 0x40

union sfbti_entry_suffix {
    int64_t count; // in leaf
    uint32_t offset; // in internal node
};

struct sfbti_record {
    // fixed size, no size field, no separate buffer

    unsigned char keyvals[KEYS_PER_RECORD][KEY_SIZE+SUFFIX_SIZE];

    uint8_t entries;
    uint8_t flags;
};

#ifdef USE_SEMIFILES
#define FSF_FPOS semifile_fpos_t
#define FSF_FGETPOS semifile_fgetpos
#define FSF_FSEEK semifile_fseek
#define FSF_FREAD semifile_fread
#define FSF_FWRITE semifile_fwrite
#define FSF_FSETPOS semifile_fsetpos
#define FSF_FTELL semifile_ftell
#define FSF_REWIND semifile_rewind
#define FSF_CLEARERR semifile_clearerr
#define FSF_FEOF semifile_feof
#define FSF_FCLOSE semifile_fclose
#else
#define FSF_FPOS fpos_t
#define FSF_FGETPOS fgetpos
#define FSF_FSEEK fseek
#define FSF_FREAD fread
#define FSF_FWRITE fwrite
#define FSF_FSETPOS fsetpos
#define FSF_FTELL ftell
#define FSF_REWIND rewind
#define FSF_CLEARERR clearerr
#define FSF_FEOF feof
#define FSF_FCLOSE fclose
#endif

struct sfbti_wctx {
#ifdef USE_SEMIFILES
    SEMIFILE* f;
#else
    FILE *f;
#endif
    char filename[ MAX_SFBT_FILENAME_LEN ];
    long current_pos;

    long current_generation_foffset;

    struct sfbti_record current_record;

    int collected_children;
    char child_first_key[ KEYS_PER_RECORD ][ KEY_SIZE ];
    uint32_t child_offset[ KEYS_PER_RECORD ];

    int ngram_size;

#ifdef DEBUG
    char debug_last_key[ MAX_ENTRY_SIZE ];
#endif
};


int sfbti_add_entry( struct sfbti_wctx*, const int *, const int, int64_t);
int sfbti_new_leaf_record( struct sfbti_wctx*);
int sfbti_flush_record( struct sfbti_wctx*);
int sfbti_finalize( struct sfbti_wctx*);
int sfbti_write_root( struct sfbti_wctx*);
int sfbti_write_parents( struct sfbti_wctx*);
int sfbti_collect_children( struct sfbti_wctx*,long);
int sfbti_write_collected_node( struct sfbti_wctx*);
int sfbti_check_last_child_gen_at( struct sfbti_wctx*, long);

struct sfbti_wctx *sfbti_new_wctx(const char *, int);
int sfbti_close_wctx(struct sfbti_wctx*);

struct sfbti_cached_record {
    struct sfbti_record data;
//    struct sfbti_cached_record *cached[ KEYS_PER_RECORD ];
};

struct sfbti_tar_rctx {
    struct tarbind_binding *binding;

    struct sfbti_cached_record *root;

    struct sfbti_record buffer;

    int ngram_size;

    int cached_bytes;
    int cached_internal_nodes;

    int leaf_read_phase; // 0, 1, 2
    uint64_t leaves_begin_at;
    int leaves_reachable_by_cache;

    char filename[ MAX_SFBT_FILENAME_LEN ];
};

int sfbti_tar_open_rctx(struct tarbind_context*, const char*, struct sfbti_tar_rctx*, int, int);
int sfbti_tar_close_rctx(struct sfbti_tar_rctx*);
struct sfbti_cached_record* sfbti_tar_cache_node(struct sfbti_tar_rctx*, int);
int sfbti_tar_readnode(struct sfbti_tar_rctx*, struct sfbti_record* );
int sfbti_tar_search(struct sfbti_tar_rctx*, const int*, const int, int64_t*);

int sfbti_maptar_search(struct sfbti_tar_rctx*, const int*, const int, int64_t*);

void sfbti_print_statistics(FILE *);
int sfbti_tar_estimate_ab1_size(struct sfbti_tar_rctx*, uint64_t *, int );

int sfbti_tar_cache_node_ab1(struct sfbti_tar_rctx*, struct sfbti_cached_record**);

int sfbti_tar_random_read(struct sfbti_tar_rctx*);

int sfbti_leafdump(struct sfbti_tar_rctx*);
#endif
