#ifndef H_SEMIFILE
#define H_SEMIFILE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILENAME_LENGTH 1024
#define BUFFERSIZE 32758

/* This gets around problems with too many open files by using
   fixed-size buffers so we need to flush less often.
   Crucially, the files are not kept open, but are only
   opened and closed for flush operations.

   The mode is always wb+ or rb+, indicated by the trunc flag.
   When implicitly reopened the mode is always rb+.

   Only optimized for writing.
*/

struct semifile_ctx {
    char filename[ MAX_FILENAME_LENGTH ];

    char buffer[ BUFFERSIZE ];
    int fill;

    long buffer_pos; // position of buffer in file
    long offset; // current rw pointer offset into buffer

    int error, eof, errorno;

    struct semifile_ctx *prev, *next;
};

typedef struct semifile_ctx SEMIFILE;
typedef long semifile_fpos_t;

struct semifile_ctx* semifile_fopen( const char*, int );
int semifile_fclose( struct semifile_ctx* );
int semifile_flush( struct semifile_ctx* );

int semifile_fwrite( const void*, size_t, size_t, struct semifile_ctx* );
int semifile_fread( void*, size_t, size_t, struct semifile_ctx* );

long semifile_ftell( struct semifile_ctx*);
int semifile_fseek( struct semifile_ctx*, long, int );
int semifile_fgetpos( struct semifile_ctx*, semifile_fpos_t*  );
int semifile_fsetpos( struct semifile_ctx*, semifile_fpos_t*  );
int semifile_rewind( struct semifile_ctx* );

int semifile_ferror( struct semifile_ctx* );
int semifile_feof( struct semifile_ctx* );
int semifile_clearerr( struct semifile_ctx* );

#endif
