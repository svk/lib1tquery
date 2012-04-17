#ifndef H_NGREAD
#define H_NGREAD

#define MAX_LINE_LENGTH 1023
#define LINE_BUFFER_SIZE (MAX_LINE_LENGTH+1)
#define MAX_COLUMNS 16

#include <zlib.h>

struct ngr_file {
    gzFile input;

    char buffer[ LINE_BUFFER_SIZE ];
    char *col[MAX_COLUMNS];

    int fill;
    int linesize;
    int newlines;
    int columns;
};

struct ngr_file* ngr_open(const char *);
int ngr_next(struct ngr_file*);
void ngr_free(struct ngr_file*);
int ngr_columns(struct ngr_file*);
long long ngr_ll_col(struct ngr_file*, int);
const char* ngr_s_col(struct ngr_file*, int);

#endif
