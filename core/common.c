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

void optinit(
    struct optstate *s,
    const struct optspec *options,
    int count,
    char **args)
{
  s->options = options;
  s->index = 0;
  s->count = count;
  s->args = args;
  s->shortopt = NULL;
}

static int optparse_short(struct optstate *s)
{
  const struct optspec *o;
  for (o = s->options; o->value; o++) {
    if ((int)*s->shortopt == o->value) {
      s->shortopt++;
      if (o->arg) {
        if (*s->shortopt != '\0') {
          s->optarg = s->shortopt;
        } else if (s->index < s->count) {
          s->optarg = s->args[s->index++];
        } else {
          error("option requires an argument: -%c", o->value);
          exit(1);
        }
        s->shortopt = NULL;
      } else {
        if (*s->shortopt == '\0') {
          s->shortopt = NULL;
        }
      }
      return o->value;
    }
  }

  error("invalid option: -%c", (int)*s->shortopt);
  exit(1);
}

int optparse(struct optstate *s)
{
  if (s->shortopt) {
    return optparse_short(s);
  }

  /* End of arguments */
  if (s->index >= s->count) {
    return -1;
  }

  char *cur = s->args[s->index];

  /* Positional argument (includes -) */
  if (cur[0] != '-' || cur[1] == '\0') {
    return -1;
  }

  /* Short option */
  if (cur[1] != '-') {
    s->shortopt = cur + 1;
    s->index++;
    return optparse_short(s);
  }

  /* End of options (--) */
  if (cur[2] == '\0') {
    s->index++;
    return -1;
  }

  /* Long option */

  /* Loop until we find a matching long option */
  const struct optspec *o;
  for (o = s->options; o->value; o++) {
    if (!o->name) /* no long option */
      continue;

    char *oc = o->name;
    char *ac = cur + 2;

    for (;;) {
      if (*oc == '\0') {
        if (o->arg) {
          if (*ac == '\0') {
            error("option requires an argument: --%s", o->name);
            exit(1);
          } else if (*ac == '=') {
            s->optarg = ac + 1;
          } else {
            break;
          }
        } else if (*ac == '=') {
          error("option accepts no argument: --%s", o->name);
          exit(1);
        } else if (*ac != '\0') {
          break;
        }
        s->index++;
        return o->value;
      }
      if (*ac != *oc) {
        break;
      }
      oc++;
      ac++;
    }
  }

  error("invalid option: %s", cur);
  exit(1);
}
