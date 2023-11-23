#include "common.h"

#ifdef GALE_DEBUG
bool gale_do_trace = false;

void gale_init_trace(void)
{
  const char *var = getenv("GALE_DEBUG");
  if (var && *var)
    gale_do_trace = true;
}
#endif

void *xalloc(size_t size)
{
  void *p = malloc(size);
  if (!p)
    fatal("out of memory");
  return p;
}

void *xrealloc(void *p, size_t size)
{
  p = realloc(p, size);
  if (!p)
    fatal("out of memory");
  return p;
}

char *xstrdup(const char *s)
{
  size_t size = strlen(s) + 1;
  char *new_str = xalloc(size);
  memcpy(new_str, s, size);
  return new_str;
}

char *xvstrfmt(const char *format, va_list ap)
{
  size_t size;

  va_list cp;
  va_copy(cp, ap);
  size = (size_t)vsnprintf(NULL, 0, format, cp) + 1;
  va_end(cp);

  char *s = xalloc(size);
  vsnprintf(s, size, format, ap);
  return s;
}

char *xstrfmt(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  char *s = xvstrfmt(format, ap);
  va_end(ap);
  return s;
}

char *get_dirname(const char *path)
{
  const char *end = NULL;
  const char *lastend;
  int ended = 0;
  for (const char *c = path; *c != '\0'; c++) {
    if (*c == '/') {
      if (!ended) {
        lastend = end;
        end = c;
        ended = 1;
      }
    } else {
      ended = 0;
    }
  }
  if (end == path) {
    return xstrdup("/");
  }
  if (ended) {
    end = lastend;
  }
  if (!end) {
    return xstrdup(".");
  }
  size_t dir_len = end - path;
  char *dir = xalloc(dir_len + 1);
  memcpy(dir, path, dir_len);
  dir[dir_len] = '\0';
  return dir;
}

char *get_dirname_null(const char *path)
{
  const char *end = NULL;
  const char *lastend;
  int ended = 0;
  for (const char *c = path; *c != '\0'; c++) {
    if (*c == '/') {
      if (!ended) {
        lastend = end;
        end = c;
        ended = 1;
      }
    } else {
      ended = 0;
    }
  }
  if (end == path) {
    return NULL;
  }
  if (ended) {
    // trailing slashes
    end = lastend;
  }
  if (!end) {
    // no slashes at all
    return NULL;
  }
  size_t dir_len = end - path;
  char *dir = xalloc(dir_len + 1);
  memcpy(dir, path, dir_len);
  dir[dir_len] = '\0';
  return dir;
}
