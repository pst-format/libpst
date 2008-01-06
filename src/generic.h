/* {{{ Generic.h - thigns every program does:
 *
 * - user output (log, debug, etc)
 * - crash and burn
 * - allocate memory (or explode)
 * }}} */
#ifndef GENERIC_H
#define GENERIC_H
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
/***************************************************/

#define LOAD_DEBUG 1

#define DIE(...) { fprintf(stderr, "Fatal Error at %s,%d: ", __FILE__, __LINE__); pDIE(__VA_ARGS__); }

//#define WARN(...) { fprintf(stderr, "WARN: %s,%d: ", __FILE__, __LINE__); pWARN(__VA_ARGS__); }
void pDIE( char *fmt, ... );
//void pWARN( char *fmt, ... );

#define WARN(...) DB( DB_WARN, __VA_ARGS__ )
#define ASSERT(x,...) { if( !(x) ) DIE( __VA_ARGS__ ); }

void *F_MALLOC( size_t size );
void *F_REALLOC( void *p, size_t size );

#define DO_DEBUG 0
#define DEBUG(x) if( DO_DEBUG ) { x; }
#define STUPID_CR "\r\n"

#define DB_CRASH   0 // crashing
#define DB_ERR     1 // error
#define DB_WARN    2 // warning
#define DB_INFO    3 // normal, but significant, condition
#define DB_VERB	   4 // verbose information
#define DB_0       5 // debug-level message
#define DB_1       6 // debug-level message
#define DB_2       7 // debug-level message

extern int DEBUG_LEVEL;
extern void (*dbfunc)(char *file, int line, int level, char *fmt, ...);

#define DB(...) { dbfunc( __FILE__, __LINE__, __VA_ARGS__ ); }

int set_db_function( void (*func)( char *file, int line, int level, char *fmt, ...) );

#endif
