#define _GNU_SOURCE
#ifndef PANIC_H
#define PANIC_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void *panic_malloc(size_t size, const char* caller);
void *panic_realloc(void *__ptr, size_t size, const char* caller);
void *panic_calloc(size_t num, size_t size, const char* caller);
int panic_asprintf(const char *caller, char **strp, const char *fmt, ...);

#define pamalloc(size) panic_malloc(size, __func__)
#define parealloc(ptr, size) panic_realloc(ptr, size, __func__)
#define pacalloc(num, size) panic_calloc(num, size, __func__)
#define paasprintf(strp, fmt, ...) panic_asprintf(__func__, strp, fmt, __VA_ARGS__)
#endif