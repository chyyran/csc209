#ifndef DYNSTR_H
#define DYNSTR_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct dynstr_s DynamicString;

DynamicString *ds_new(ssize_t len);

DynamicString *ds_from_cstr(const char *s);

DynamicString *ds_append(DynamicString *ds, const char *s);

ssize_t ds_len(DynamicString *ds);

char *ds_into_raw(DynamicString *ds);
char *ds_into_raw_truncate(DynamicString *ds, int len);

void ds_free(DynamicString *ds);
#endif