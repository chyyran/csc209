#define _GNU_SOURCE
#include "dynstr.h"

typedef struct dynstr_s
{
    char *raw;
    ssize_t len;
} dynstr_s;

//todo: panic_malloc

DynamicString *ds_new(ssize_t len)
{
    DynamicString *ptr = malloc(sizeof(DynamicString));
    ptr->raw = malloc(sizeof(char) * len);
    ptr->len = 0;
    return ptr;
}

DynamicString *ds_from_cstr(const char *s)
{
    ssize_t len = strlen(s);
    DynamicString *ptr = malloc(sizeof(DynamicString));
    ptr->raw = malloc(sizeof(char) * len);
    ptr->len = len;

    strcpy(ptr->raw, s);
    return ptr;
}

DynamicString *ds_append(DynamicString *ds, const char *s)
{
    char* new_ptr;
    if (asprintf(&new_ptr, "%s%s", ds->raw, s) == -1)
    {
        // if asprintf fails, then nreturn th previous string.
        return ds;
    } 

    free(ds->raw);
    ds->raw = new_ptr;
    ds->len = strlen(ds->raw);
    return ds;
}

ssize_t ds_len(DynamicString *ds)
{
    return ds->len;
}

char *ds_into_raw(DynamicString *ds)
{
    char* raw = ds->raw;
    free(ds);
    return raw;
}

void ds_free(DynamicString *ds)
{
    free(ds->raw);
    free(ds);
}