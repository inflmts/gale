#include "common.h"
#include "astr.h"

static char *empty_buf = "";

void astr_init(struct astr *as)
{
  as->cap = 0;
  as->len = 0;
  as->buf = empty_buf;
}

void astr_grow(struct astr *as, size_t amount)
{
  if (!as->cap) {
    as->cap = amount + 1;
    as->len = amount;
    as->buf = xalloc(as->cap);
  } else {
    as->len += amount;
    if (as->cap <= as->len) {
      as->cap *= 2;
      while (as->cap <= as->len)
        as->cap *= 2;
      as->buf = xrealloc(as->buf, as->cap);
    }
  }
}

void astr_append_char(struct astr *as, int ch)
{
  astr_grow(as, 1);
  as->buf[as->len - 1] = ch;
  as->buf[as->len] = '\0';
}

char *astr_release(struct astr *as)
{
  char *buf;
  if (as->cap) {
    buf = as->buf;
  } else {
    buf = xalloc(1);
    *buf = '\0';
  }
  return buf;
}

void astr_free(struct astr *as)
{
  if (as->cap)
    free(as->buf);
}
