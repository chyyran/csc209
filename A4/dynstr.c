#define _GNU_SOURCE
#include "dynstr.h"
#include "panic.h"

typedef struct dynstr_s
{
    char *raw;
    ssize_t len;
} dynstr_s;

DynamicString *ds_new(ssize_t len)
{
    DynamicString *ptr = pacalloc(1, sizeof(DynamicString));
    ptr->raw = pacalloc(len + 1, sizeof(char));
    ptr->len = 0;
    return ptr;
}

DynamicString *ds_from_cstr(const char *s)
{
    ssize_t len = strlen(s);
    DynamicString *ptr = pacalloc(1, sizeof(DynamicString));
    ptr->raw = pacalloc(len + 1, sizeof(char));
    ptr->len = len;

    strcpy(ptr->raw, s);
    return ptr;
}

DynamicString *ds_append(DynamicString *ds, const char *s)
{
    char *new_ptr;
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
    return ds->len + 1;
}

char *ds_into_raw(DynamicString *ds)
{
    char *raw = ds->raw;
    free(ds);
    return raw;
}

char *ds_into_raw_truncate(DynamicString *ds, int len)
{
    if (len < ds_len(ds))
    {
        char *raw = ds->raw;
        raw[len] = '\0';
        free(ds);
        return raw;
    }
    else
    {
        return ds_into_raw(ds);
    }
}

void ds_free(DynamicString *ds)
{
    free(ds->raw);
    free(ds);
}