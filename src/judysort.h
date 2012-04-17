#ifndef H_SORTREAD
#define H_SORTREAD

#define MAX_SYMBOLS 5

#include <stdint.h>

struct judysort_context {
    void *pointer;
    int count;
};

void judysort_initialize(struct judysort_context*);
void judysort_insert(struct judysort_context*, const uint32_t*, int, int64_t );

void judysort_insert_test(struct judysort_context*, int,int,int,int,int, int64_t);

int judysort_dump_output_test(int *, int, int64_t, void*);
int judysort_dump_output_gzfile(int *, int, int64_t, void *);

long long judysort_dump_free(struct judysort_context*, int, int (*f)(int*,int,int64_t,void*), void* );

int judysort_get_count(struct judysort_context*);

#endif
