#ifndef H_LIB1TQUERY
#define H_LIB1TQUERY

#include <stdint.h>

void lib1tquery_quit(void);
int lib1tquery_init(const char*);

int32_t lib1tquery_dictionary( const char* );
int64_t lib1tquery_lookup_ngram( int, const int32_t* );

int64_t lib1tquery_lookup_1gram( int32_t );
int64_t lib1tquery_lookup_2gram( int32_t, int32_t );
int64_t lib1tquery_lookup_3gram( int32_t, int32_t, int32_t );
int64_t lib1tquery_lookup_4gram( int32_t, int32_t, int32_t, int32_t );
int64_t lib1tquery_lookup_5gram( int32_t, int32_t, int32_t, int32_t, int32_t );

#include <stdio.h>
int lib1tquery_print_cache_stats(FILE *);
void lib1tquery_reopen_tar(void);
void lib1tquery_suspend_tar(void);

int lib1tquery_test_random_read(void);
void lib1tquery_debug_leafdump( int);

#ifdef TARBIND_LOG_SEEKS_DEBUG
void lib1tquery_log_seeks_to(FILE*);
#endif

#endif
