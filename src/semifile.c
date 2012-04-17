#include "semifile.h"

#include <stdio.h>
#include <errno.h>
#include <assert.h>

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

// #include <pthread.h> // TODO

struct semifile_ctx *semifile_list = 0;

struct semifile_ctx* semifile_fopen( const char* filename, int trunc ) {
    struct semifile_ctx* rv = malloc(sizeof *rv);
    do {
        const char *mode = trunc ? "wb+" : "rb+";
        FILE *f = fopen( filename, mode );
//        fprintf( stderr, "opening with trunc?=%d\n", trunc );
        if( !f ) break;
        fclose( f );

        assert( strlen(filename) < MAX_FILENAME_LENGTH );
        strcpy( rv->filename, filename );

        rv->fill = 0;
        rv->buffer_pos = 0;
        rv->offset = 0;

        rv->prev = 0;
        rv->next = semifile_list;
        if( rv->next ) {
            rv->next->prev = rv;
        }
        semifile_list = rv;

        rv->error = 0;
        rv->eof = 0;
        rv->errorno = 0;

        return rv;
    } while(0);
    if( rv ) free( rv );
    return 0;
}

int semifile_fclose( struct semifile_ctx* ctx ) {
    if( semifile_flush( ctx ) ) return 1;

    if( ctx->prev ) {
        ctx->prev->next = ctx->next;
    } else {
        assert( ctx == semifile_list );
        semifile_list = ctx->next;
    }

    if( ctx->next ) {
        ctx->next->prev = ctx->prev;
    }

    free( ctx );
    
    return 0;
}

int semifile_flush( struct semifile_ctx* ctx ) {
    if( !ctx->fill ) return 0;

//    fprintf( stderr, "semiflushing: ye %s\n", ctx->filename );

    FILE *f = fopen( ctx->filename, "rb+" );
    if( !f ) return 1;

    if( fseek( f, ctx->buffer_pos, SEEK_SET ) ) {
        fclose( f );
        ctx->error = 2;
        return 1;
    }

//    fprintf( stderr, "flush-writing to %08x [%d bytes, offset = %08x]\n", ctx->buffer_pos, ctx->fill, ctx->offset );

    int written = 0;
    while( written < ctx->fill ) {
        int rv = fwrite( &ctx->buffer[written], 1, ctx->fill - written, f );
        if( rv < 0 || ferror( f ) ) {
            fclose(f);
            ctx->error = 2;
            return 1;
        }
        written += rv;
    }
//    fprintf( stderr, "flush-wrote to %08x [%d bytes written, offset = %d]\n", ctx->buffer_pos, written, ctx->offset );


    ctx->buffer_pos += ctx->offset; // might not == written if seeks are involved
    ctx->fill = 0;
    ctx->offset = 0;
    memset( ctx->buffer, 0, BUFFERSIZE );

//    fprintf( stderr, "next offset is %08x\n", ctx->buffer_pos );

    ctx->error = ferror( f ) ? 1 : 0;
    ctx->eof = feof( f );
    ctx->errorno = errno;

    fclose(f);
    return 0;
}

long semifile_ftell( struct semifile_ctx* ctx) {
    return ctx->buffer_pos + ctx->offset;
}

int semifile_fgetpos( struct semifile_ctx* ctx, semifile_fpos_t* fpos) {
    *fpos = ctx->buffer_pos + ctx->offset;
    return 0;
}

int semifile_fsetpos( struct semifile_ctx* ctx, semifile_fpos_t* fpos ) {
    return semifile_fseek( ctx, *fpos, SEEK_SET );
}

int semifile_rewind( struct semifile_ctx* ctx ) {
    return semifile_fseek( ctx, 0, SEEK_SET );
}

int semifile_fwrite( const void* data, size_t size, size_t nmemb, struct semifile_ctx* ctx) {
    size_t total = size * nmemb;

    for(int i=0;i<2;i++) {
        if( (BUFFERSIZE - ctx->offset) >= total ) {
            memcpy( &ctx->buffer[ctx->offset], data, total );
            ctx->offset += total;
            ctx->fill = MAX( ctx->fill, ctx->offset );
            return nmemb;
        }
        semifile_flush( ctx );
    }

    FILE *f = fopen( ctx->filename, "rb+" );
    if( !f ) {
        ctx->error = 2;
        return 1;
    }

    long realpos = semifile_ftell( ctx );
    if( fseek( f, realpos, SEEK_SET ) ) {
        ctx->error = 2;
        ctx->eof = 0;
        ctx->errorno = errno;
        fclose(f);
        return 1;
    }
//    fprintf( stderr, "write-writing to %08x [%d bytes]\n", realpos, size * nmemb );

    int rv = fwrite( data, size, nmemb, f );
    if( rv > 0 ) {
        ctx->buffer_pos += size * rv;
//        fprintf( stderr, "write-writing changed buffer_pos to %08x\n", ctx->buffer_pos );
    }

    ctx->error = ferror( f ) ? 1 : 0;
    ctx->eof = feof( f );
    ctx->errorno = errno;

    fclose(f);

    return rv;
}

int semifile_fread(void* data, size_t size, size_t nmemb, struct semifile_ctx* ctx) {
    if( semifile_flush(ctx) ) return 1;

    FILE *f = fopen( ctx->filename, "rb+" );
    if( !f ) {
        ctx->error = 2;
        return 1;
    }

    long realpos = semifile_ftell( ctx );
        if( fseek( f, realpos, SEEK_SET ) ) {
            ctx->error = 2;
            ctx->eof = 0;
            ctx->errorno = errno;
            fclose(f);
            return 1;
        }

    int rv = fread( data, size, nmemb, f );
    if( rv > 0 ) {
        ctx->buffer_pos += size * rv;
//        fprintf( stderr, "read-reading changed buffer_pos to %08x\n", ctx->buffer_pos );
    }

    ctx->error = ferror( f ) ? 1 : 0;
    ctx->eof = feof( f );
    ctx->errorno = errno;

    fclose(f);

    return rv;
}

int semifile_clearerr( struct semifile_ctx* ctx ) {
    ctx->error = ctx->eof = ctx->errorno = 0;
}

int semifile_ferror( struct semifile_ctx* ctx ) {
    return ctx->error;
}

int semifile_fseek( struct semifile_ctx* ctx, long offset, int whence ) {
//    fprintf( stderr, "seeking %ld %d\n", offset, whence );
    switch( whence ) {
        case SEEK_SET:
//            fprintf( stderr, "desire SEEK_SET to %08x\n", offset);

            if( ctx->fill > 0 && semifile_flush( ctx ) ) return 1;
            ctx->buffer_pos = offset;
            ctx->offset = 0;
            return 0;

            // simplify
//            offset -= ctx->buffer_pos + ctx->offset;
        case SEEK_CUR:
            // XXX does not work correctly -- unused
            fprintf( stderr, "desire ~ SEEK_CUR to %08x\n", offset);
            {
                long noffset = offset + ctx->offset;
                fprintf( stderr, "offset %ld noffset %ld bufsize %ld\n", offset, noffset, BUFFERSIZE );
                if( noffset < 0 || noffset >= BUFFERSIZE ) {
                    fprintf( stderr, "[semiflushing]\n" );
                    if( semifile_flush( ctx ) ) return 1;
                    fprintf( stderr, "[semiflushed]\n" );

                    ctx->buffer_pos += offset;
                    
                    fprintf( stderr, "buffer_pos now %d\n", ctx->buffer_pos );
                } else {
                    ctx->offset = noffset;
                }
                return 0;
            }
        case SEEK_END:
            {
                if( semifile_flush( ctx ) ) return 1;

                FILE *f = fopen( ctx->filename, "rb+" );
                if( !f ) {
                    ctx->error = 2;
                    return 1;
                }
                if( fseek( f, offset, SEEK_END ) ) {
                    fclose( f );
                    ctx->error = 2;
                    return 1;
                }
                ctx->buffer_pos = ftell( f );

                fclose(f);

                return 0;
            }
        default:
            ctx->error = 2;
            return 1;
    }
}

int semifile_feof( struct semifile_ctx* ctx ) {
    return ctx->eof;
}
