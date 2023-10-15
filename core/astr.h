/*
 * astr - automatic strings
 */
#ifndef ASTR_H
#define ASTR_H

#include <stddef.h>

#define ASTR_INITIAL_CAP 8

struct astr
{
  size_t cap;
  size_t len;
  char *buf;
};

/* Initialize `as` to an empty string. */
void astr_init(struct astr *as);

/* Allocate `amount` bytes in the string. */
void astr_grow(struct astr *as, size_t amount);

/* Append a character to the string. */
void astr_append_char(struct astr *as, int ch);

/* Return a pointer to a string that can be released using free(). This also
 * frees `as`. */
char *astr_release(struct astr *as);

/* Free `as`. */
void astr_free(struct astr *as);

#endif
