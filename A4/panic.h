
/**
 * Various utility wrappers around system calls that will
 * immediately panic and exit the application on failure.
 * 
 * It is recommended to use the provided set of pa* macros
 * instead of directly calling the panic_* functions. This
 * will allow any error messages in the event of a panic to
 * display the function wherein the system call was made.
 */

#define _GNU_SOURCE
#ifndef PANIC_H
#define PANIC_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/**
 * A wrapper function for malloc that panics on error (failure to allocate).
 */ 
void *panic_malloc(size_t size, const char* caller);

/**
 * A wrapper function for realloc that panics on error (failure to reallocate).
 */ 
void *panic_realloc(void *__ptr, size_t size, const char* caller);

/**
 * A wrapper function for calloc that panics on error (failure to allocate).
 */ 
void *panic_calloc(size_t num, size_t size, const char* caller);


/**
 * A wrapper function for asprintf that panics on error (failure to allocate).
 */ 
int panic_asprintf(const char *caller, char **strp, const char *fmt, ...);

/**
 * Convienience macro for panic_malloc
 */
#define pamalloc(size) panic_malloc(size, __func__)


/**
 * Convienience macro for panic_realloc
 */
#define parealloc(ptr, size) panic_realloc(ptr, size, __func__)


/**
 * Convienience macro for panic_calloc
 */
#define pacalloc(num, size) panic_calloc(num, size, __func__)


/**
 * Convienience macro for panic_asprintf
 */
#define paasprintf(strp, fmt, ...) panic_asprintf(__func__, strp, fmt, __VA_ARGS__)
#endif