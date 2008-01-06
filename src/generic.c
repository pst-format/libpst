

#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generic.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif




void pDIE( char *fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
        //fprintf( stderr, "Fatal error (will segfault): ");
    vfprintf( stderr, fmt, ap );
    fprintf( stderr, "\n" );
    va_end(ap);
        raise( SIGSEGV );
}

void pWARN( char *fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
        fprintf( stderr, "WARNING: ");
    vfprintf( stderr, fmt, ap );
    fprintf( stderr, "\n" );
    va_end(ap);
}

void *F_MALLOC( size_t size )
{
    void *result;

    result = malloc( size );
    ASSERT( NULL != result, "malloc() failure." );

    return result;
}

void *F_REALLOC( void *p, size_t size )
{
    void *result;

    //if( NULL != p ) hexdump((char*)p - 128, 0, 128, 1 );
    if(!p) {
        ASSERT( NULL != ( result = malloc( size ) ), "malloc() failure." );
    }
    else {
        ASSERT( NULL != ( result = realloc( p, size ) ), "realloc() failure." );
    }

    //hexdump((char*)result - 128, 0, 128, 1 );
    fflush(stderr);
    return result;
}



int DEBUG_LEVEL = DB_INFO;

void db_default( char *file, int line, int level, char *fmt, ... )
{
    va_list ap;
    if( level <= DEBUG_LEVEL ) {
        switch( level ) {
            case DB_CRASH:
                fprintf(stderr, "CRASH");
                break;
            case DB_ERR:
                fprintf(stderr, "ERROR");
                break;
            case DB_WARN:
                fprintf(stderr, "WARNING");
                break;
            case DB_INFO:
            case DB_VERB:
                break;
            default:
                fprintf(stderr, "DEBUG(%d)", level );
        }

        if( level <= DB_WARN )
            fprintf(stderr, " (%s:%d)", file, line );

        if( DB_INFO != level && DB_VERB != level )
            fprintf(stderr, ": ");

        va_start( ap, fmt );
        vfprintf(stderr, fmt, ap );
        fprintf(stderr, "\n" );
        va_end( ap );
    }
}

void (*dbfunc)(char *file, int line, int level, char *fmt, ...) = &db_default;




