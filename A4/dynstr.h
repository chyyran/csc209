#ifndef DYNSTR_H
#define DYNSTR_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * A heap allocated, write-only monotonically growable
 * dynamically sized owned String.
 * 
 * The DynamicString type should be thought of as
 * having ownership over the contents of the string.
 * 
 * Once written to, it can not be read back until 
 * it is turned back into a C-style string, consuming
 * itself in the process and giving up ownership of
 * the contents.
 */
typedef struct dynstr_s DynamicString;

/**
 * Creates a new, empty DynamicString.
 */
DynamicString *ds_new();

/**
 * Creates a new DynamicString from a null-terminated,
 * C-style string.
 * 
 * Note that this operation does not consume the 
 * given string, and makes a copu instead.
 */
DynamicString *ds_from_cstr(const char *s);

/**
 * Appends the given null-terminated C-style string to 
 * this DynamicString.
 */
DynamicString *ds_append(DynamicString *ds, const char *s);

/**
 * Returns the length of this DynamicString.
 */ 
ssize_t ds_len(DynamicString *ds);

/**
 * Consumes this DynamicString and returns a pointer to 
 * a C-style string that is the contents of this DynamicString.
 * 
 * The string returned is guaranteed to be null-terminated.
 * 
 * This operation consumes the DynamicString, and thus the
 * pointer passed into this function will no longer be valid 
 * after.
 * 
 * The pointer returned must be freed manually afterwards.
 */
char *ds_into_raw(DynamicString *ds);

/**
 * Consumes this DynamicString and returns a pointer to 
 * a C-style string that is the contents of this DynamicString.
 * 
 * The string returned is guaranteed to be null-terminated, and
 * will be at most len characters long. 
 * 
 * This operation consumes the DynamicString, and thus the
 * pointer passed into this function will no longer be valid 
 * after.
 * 
 * The pointer returned must be freed manually afterwards.
 */
char *ds_into_raw_truncate(DynamicString *ds, int len);

/**
 * Frees the DynamicString, including its contents. Once
 * freed, the given pointer becomes invalid.
 */
void ds_free(DynamicString *ds);
#endif