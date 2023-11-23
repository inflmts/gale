#ifndef COMMON_H
#define COMMON_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define fatal(fmt, ...) \
  do { fprintf(stderr, "fatal: " fmt "\n", ##__VA_ARGS__); exit(-1); } while (0)

#define error(fmt, ...) fprintf(stderr, "error: " fmt "\n", ##__VA_ARGS__)
#define error_sys(fmt, ...) error(fmt ": %s", ##__VA_ARGS__, strerror(errno))
#define warn(fmt, ...) fprintf(stderr, "warning: " fmt "\n", ##__VA_ARGS__)
#define warn_sys(fmt, ...) warn(fmt ": %s", ##__VA_ARGS__, strerror(errno))

#ifdef GALE_DEBUG
# include <assert.h>
# define GALE_ASSERT(expr) assert(expr)
# define GALE_TRACE(fmt, ...) \
  do { if (gale_do_trace) fprintf(stderr, "debug: " fmt "\n", ##__VA_ARGS__); } while (0)

extern bool gale_do_trace;
void gale_init_trace(void);
#else
# define GALE_ASSERT(expr) ((void)0)
# define GALE_TRACE(fmt, ...) ((void)0)
# define gale_init_trace() ((void)0)
#endif

#define NORETURN _Noreturn

#ifdef __GNUC__
# define FORMAT_PRINTF(f, i) __attribute__((format (printf, f, i)))
#else
# define FORMAT_PRINTF(f, i)
#endif

#define container_of(p, type, member) \
  ((type *)((char *)(p) - offsetof(type, member)))

void *xalloc(size_t size);
#define xnew(type) ((type *)xalloc(sizeof(type)))
void *xrealloc(void *p, size_t size);

char *xstrdup(const char *s);

char *xvstrfmt(const char *format, va_list ap) FORMAT_PRINTF(1, 0);
char *xstrfmt(const char *format, ...) FORMAT_PRINTF(1, 2);

// Get a string representing the parent directory of `path`. This does not
// modify `path`, and ignores trailing slashes. The returned string should be
// released using free().
char *get_dirname(const char *path);

// Similar to get_dirname() but returns NULL if the return value would be '/' or
// '.'. Otherwise, the returned string should be released using free().
char *get_dirname_null(const char *path);

#endif
