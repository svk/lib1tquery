#ifndef H_TARBIND
#define H_TARBIND

/* This is a module to read files quickly 
 *
 * "File names" are used (instead of e.g. ID numbers) for transitional
 * reasons; as of this writing the file system is used for this in working code.
 *
 * Note that mappings from file names are done at INITIALIZATION time
 * in our application, so this is not a relevant performance issue.
 * They use a Judy array merely because it's convenient (is already a
 * dependency), not because the performance matters here.
 *
 * We use .tar because it's a suitable archive format, is simple, and 
 * is already widely supported, which is practical.
*/

#include <Judy.h>
#include <stdint.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef int64_t tarbind_offset;

struct tarbind_binding {
    struct tarbind_context *context;
    tarbind_offset offset;
    tarbind_offset length;
    const unsigned char *data;
};

struct tarbind_context {
    int descriptor;

    Pvoid_t bindingMap;

    size_t length;
    const unsigned char *data;
};

struct tarbind_context* tarbind_create( const char* );
void tarbind_free( struct tarbind_context* );

struct tarbind_binding * tarbind_get_binding( struct tarbind_context*, const char* );
int tarbind_read_at( struct tarbind_binding*, void *, tarbind_offset, int );
int tarbind_seek_to( struct tarbind_binding* );
int tarbind_seek_into( struct tarbind_binding*, tarbind_offset );

int tarbind_prepare_mmap( struct tarbind_context * );

int tarbind_random_read( struct tarbind_context *, int );

#ifdef TARBIND_LOG_SEEKS_DEBUG
#include <stdio.h>

void tarbind_log_seeks_to(FILE*);
#endif

#endif
