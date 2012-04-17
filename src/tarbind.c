#include "tarbind.h"

#include <Judy.h>

#include <string.h>
#include <assert.h>

#include <sys/mman.h>

#define TAR_PATH_MAX 100

#define MAX(a,b) (((a)<(b))?(b):(a))
#define MIN(a,b) (((a)>(b))?(b):(a))

int tarbind_prepare_mmap( struct tarbind_context *context ) {
    struct stat info;
    if( fstat( context->descriptor, &info ) ) {
        fprintf( stderr, "fatal: fstat() error [32-bit machine? TODO]\n" );
        return 0;
    }

    context->length = info.st_size;

    fprintf( stderr, "[tarbind] mmap'ing %llu bytes\n", (uint64_t) info.st_size );

    void *rv = mmap(0, context->length, PROT_READ, MAP_PRIVATE, context->descriptor, 0 );

    if( rv == MAP_FAILED ) {
        fprintf( stderr, "[tarbind] mmap failed!\n" );
        return 1;
    }
    fprintf( stderr, "[tarbind] success, memory mapped\n" );

    context->data = rv;

    char index[TAR_PATH_MAX+1];
    PWord_t judyValue;
    Word_t bytes;

    strcpy( index, "" );

    JSLF( judyValue, context->bindingMap, index );
    while( judyValue != NULL ) {
        struct tarbind_binding *binding = (void*) *judyValue;
        binding->data = (unsigned char *const) &context->data[ binding->offset ];

        JSLN( judyValue, context->bindingMap, index );
    }

    return 0;
}

#ifdef TARBIND_LOG_SEEKS_DEBUG

static FILE * tarbind_logfile = 0;

void tarbind_log_seeks_to(FILE *f) {
    tarbind_logfile = f;
}

void tarbind_register_seek(off_t x) {
    uint64_t y = (uint64_t) x;
    if( tarbind_logfile ) {
        fprintf( tarbind_logfile, "%llu\n", y );
    }
}
#endif

int tarbind_seek_into( struct tarbind_binding* binding, tarbind_offset offset ) {
    // todo error checking?
#ifdef TARBIND_LOG_SEEKS_DEBUG
    tarbind_register_seek( binding->offset + offset );
#endif
    int rv = lseek( binding->context->descriptor, binding->offset + offset, SEEK_SET );
    return 0;
}

int tarbind_random_read( struct tarbind_context *context, int size ) {
    uint64_t r = (((uint64_t)rand())<<32) | rand();
    off_t r0 = r % (context->length - size + 1);
    int rv = lseek( context->descriptor, r0, SEEK_SET );
    static int8_t buffer[32768];
    assert( size < sizeof buffer );
    int rv2 = read( context->descriptor, buffer, size );
    return !( (rv == 0) && (rv2 == 0) );
}

int tarbind_seek_to( struct tarbind_binding* binding ) {
    int rv = lseek( binding->context->descriptor, binding->offset, SEEK_SET );
    return 0;
}

int tarbind_read_at( struct tarbind_binding* binding, void *buffer, tarbind_offset offset, int length) {
    length = MAX( length, binding->length - offset );
    if( length < 0 ) return -1;
    int rv = lseek( binding->context->descriptor, offset + binding->offset, SEEK_SET );
    if( rv == -1 ) return -1;
    return read( binding->context->descriptor, buffer, length );
}

struct tarbind_binding * tarbind_get_binding( struct tarbind_context* ctx, const char* name ) {
    PWord_t judyValue;

    JSLG( judyValue, ctx->bindingMap, name );
    if( judyValue == 0 || judyValue == PJERR ) {
        return 0;
    }

    return (void*) *judyValue;
}

static int tarbind_add_binding( struct tarbind_context* context, const char *name, tarbind_offset offset, tarbind_offset length ) {
    PWord_t judyValue;

    if( strlen( name ) >= TAR_PATH_MAX ) {
        return 1;
    }

    JSLI( judyValue, context->bindingMap, name );
    if( judyValue == PJERR ) {
        return 1;
    }
    struct tarbind_binding* binding = malloc(sizeof *binding);
    (*(void**)judyValue) = (void*) binding;

    if( !binding ) {
        return 1;
    }

    binding->context = context;
    binding->offset = offset;
    binding->length = length;
    binding->data = 0;
    
    return 0;
}

static int tarbind_scan_tar( struct tarbind_context *ctx ) {
    const int header_size = 512,
              filename_offset = 0,
              filename_size = 100,
              filesize_offset = 124,
              filesize_size = 12,
              checksum_offset = 148,
              checksum_size = 8;
    unsigned char buffer[ header_size ];
    signed char *sbuffer = (signed char*) buffer;
    unsigned char numberbuffer[ header_size + 1];
    char filename[filename_size+1];
    int all_zeroes_counter = 0;
    tarbind_offset current = 0;

    while( all_zeroes_counter < 2 ) {
        int bytes_read = 0;
        while( bytes_read < header_size ) {
            int rv = read( ctx->descriptor, &buffer[bytes_read], header_size - bytes_read ); 
            if( rv <= 0 ) {
                fprintf( stderr, "[tarbind] fatal: read error\n" );
                return 1;
            }
            bytes_read += rv;
        }

        current += header_size;

        int all_zeroes = 1;
        for(int i=0;i<header_size;i++) {
            if( buffer[i] ) {
                all_zeroes = 0;
                break;
            }
        }
        if( all_zeroes ) {
            all_zeroes_counter++;
        } else {
            memset( numberbuffer, 0, sizeof numberbuffer );
            memcpy( numberbuffer, &buffer[checksum_offset], checksum_size );
            memset( &buffer[checksum_offset], ' ', checksum_size );

            long int checksum = strtol( numberbuffer, 0, 0 );
            long int goodsum1 = 0, goodsum2 = 0;

            memset( filename, 0, sizeof filename );
            memcpy( filename, &buffer[filename_offset], filename_size );

            for(int i=0;i<header_size;i++) {
                goodsum1 += buffer[i];
                goodsum2 += sbuffer[i];
            }
            if( checksum != goodsum1 && checksum != goodsum2 ) {
                fprintf( stderr, "[tarbind] fatal: tar checksum mismatch (%d (\"%s\") vs %d or %d), filename might be \"%s\"\n", checksum, numberbuffer, goodsum1, goodsum2, filename );
                return 1;
            }

            memset( numberbuffer, 0, sizeof numberbuffer );
            memcpy( numberbuffer, &buffer[filesize_offset], filesize_size );
            tarbind_offset file_size = strtol( numberbuffer, 0, 8 );

            if( tarbind_add_binding( ctx, filename, current, file_size ) ) {
                return 1;
            }

#ifdef EXCESSIVELY_VERBOSE
            fprintf( stderr, "[tarbind] on %016lx : %s (%ld)\n", (int64_t) current, filename, (int64_t) file_size );
#endif

            long long int blocks = (file_size + 512 - 1) / 512;

            off_t rv = lseek( ctx->descriptor, blocks * 512, SEEK_CUR );
            if( rv == -1 ) {
                fprintf( stderr, "[tarbind] fatal: seek error\n" );
                return 1;
            }

            current += 512 * blocks;
        }
    }

    return 0;
}

struct tarbind_context* tarbind_create(const char *filename) {
    struct tarbind_context * rv = malloc(sizeof *rv);
    if( rv ) {
        rv->descriptor = open( filename, O_RDONLY );
        if( rv->descriptor == -1 ) {
            free( rv );
            return 0;
        }

        rv->data = 0;
        rv->length = 0;

        rv->bindingMap = NULL;

        if( tarbind_scan_tar( rv ) ) {
            tarbind_free( rv );
            return 0;
        }

        struct stat info;
        if( fstat( rv->descriptor, &info ) ) {
            fprintf( stderr, "fatal: fstat() error [32-bit machine? TODO]\n" );
            return 0;
        }
        rv->length = info.st_size;
    }
    return rv;
}

void tarbind_free( struct tarbind_context* context) {
    char index[TAR_PATH_MAX+1];
    PWord_t judyValue;
    Word_t bytes;

    if( context->data ) {
        munmap( (void*) context->data, context->length );
        context->data = 0;
    }

    strcpy( index, "" );

    JSLF( judyValue, context->bindingMap, index );
    while( judyValue != NULL ) {
        struct tarbind_binding *binding = (void*) *judyValue;
        if( binding ) {
            free( binding );
        }
        JSLN( judyValue, context->bindingMap, index );
    }
    JSLFA( bytes, context->bindingMap );

    close( context->descriptor );

    free( context );
}
