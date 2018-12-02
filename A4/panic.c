#include "panic.h"
#include <stdarg.h>

/**
 * A panic_malloc that panics and quits on on ENOMEM 
 */
void *panic_malloc(size_t size, const char *caller)
{
    void *ptr;
    if ((ptr = malloc(size)) == NULL)
    {
        fprintf(stderr, "malloc - %s : %s\n", strerror(errno), caller);
        exit(1);
    }
    return ptr;
}

/**
 * A panic_realloc that panics and quits on on ENOMEM 
 */
void *panic_realloc(void *__ptr, size_t size, const char *caller)
{
    void *ptr;
    if ((ptr = realloc(__ptr, size)) == NULL)
    {
        fprintf(stderr, "malloc - %s : %s\n", strerror(errno), caller);
        exit(1);
    }
    return ptr;
}

/**
 * A panic_calloc that panics and quits on on ENOMEM 
 */
void *panic_calloc(size_t num, size_t size, const char *caller)
{
    void *ptr;
    if ((ptr = calloc(num, size)) == NULL)
    {
        fprintf(stderr, "calloc - %s : %s\n", strerror(errno), caller);
        exit(1);
    }
    return ptr;
}

int panic_asprintf(const char *caller, char **strp, const char *fmt, ...)
{
    va_list args;

    /* Initialise the va_list variable with the ... after fmt */

    va_start(args, fmt);
    int count = -1;
    if ((count = vasprintf(strp, fmt, args)) == -1)
    {
        fprintf(stderr, "vasprintf - %s : %s\n", strerror(errno), caller);
        exit(1);
    }

    va_end(args);
    return count;
}
