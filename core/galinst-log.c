#include <fcntl.h> /* open */
#include <sys/stat.h> /* mkdir */
#include <unistd.h>
#include "common.h"
#include "astr.h"
#include "galinst-base.h"
#include "galinst-manifest.h"
#include "uthash.h"

#define PARSE_BUFFER_SIZE 1024
#define PARSE_EOF -2

/*****************************************************************************
 * PARSER
 ****************************************************************************/

struct parser
{
  struct manifest *man;
  int fd;
  unsigned int line;
  unsigned int col;
  char *cur;
  char *end;
  char buf[PARSE_BUFFER_SIZE];
};

FORMAT_PRINTF(2, 0)
static void set_errorvf(struct parser *p, const char *format, va_list ap)
{
  free(p->man->error);
  p->man->error = xvstrfmt(format, ap);
}

FORMAT_PRINTF(2, 3)
static void set_errorf(struct parser *p, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  set_errorvf(p, format, ap);
  va_end(ap);
}

FORMAT_PRINTF(2, 0)
static void set_syntax_errorvf(struct parser *p, const char *format, va_list ap)
{
  char *message = xvstrfmt(format, ap);
  set_errorf(p, "syntax error at line %u col %u: %s", p->line, p->col, message);
  free(message);
}

FORMAT_PRINTF(2, 3)
static void set_syntax_errorf(struct parser *p, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  set_syntax_errorvf(p, format, ap);
  va_end(ap);
}

static struct manifest_entry *need_dir(struct parser *p, const char *path)
{
  struct manifest_entry *ent = manifest_get_entry(p->man, path);

  if (ent) {
    if (ent->oldtype == MANIFEST_ENTRY_LINK) {
      set_errorf(p, "duplicate entries: %s", path);
      return NULL;
    }

    ent->oldtype = MANIFEST_ENTRY_DIR;
    return ent;
  }

  char *pathcopy = xstrdup(path);
  char *slash = strrchr(pathcopy, '/');
  struct manifest_entry *parent = NULL;

  if (slash) {
    *slash = '\0';
    parent = need_dir(p, pathcopy);
    if (!parent) {
      free(pathcopy);
      return NULL;
    }
    *slash = '/';
  }

  ent = xnew(struct manifest_entry);
  ent->path = pathcopy;
  ent->type = MANIFEST_ENTRY_NONE;
  ent->oldtype = MANIFEST_ENTRY_DIR;
  ent->parent = parent;
  ent->old.dir.refcount = 0;

  manifest_add_entry(p->man, ent);
  p->man->need_update_log = true;
  return ent;
}

static int add_entry(struct parser *p, const char *path)
{
  struct manifest_entry *ent = manifest_get_entry(p->man, path);

  if (ent) {
    if (ent->oldtype != MANIFEST_ENTRY_NONE) {
      set_errorf(p, "duplicate entries: %s", path);
      return -1;
    }

    ent->oldtype = MANIFEST_ENTRY_LINK;
    return 0;
  }

  char *pathcopy = xstrdup(path);
  char *slash = strrchr(pathcopy, '/');
  struct manifest_entry *parent = NULL;

  if (slash) {
    *slash = '\0';
    parent = need_dir(p, pathcopy);
    if (!parent) {
      free(pathcopy);
      return -1;
    }
    ++parent->old.dir.refcount;
    *slash = '/';
  }

  ent = xnew(struct manifest_entry);
  ent->path = pathcopy;
  ent->type = MANIFEST_ENTRY_NONE;
  ent->oldtype = MANIFEST_ENTRY_LINK;
  ent->parent = parent;
  ent->old.link.next = NULL;

  *p->man->oldlinks_tail = ent;
  p->man->oldlinks_tail = &ent->old.link.next;
  manifest_add_entry(p->man, ent);
  return 0;
}

static int cur_char(struct parser *p)
{
  if (p->cur)
    return *p->cur;
  else
    return PARSE_EOF;
}

static int next_char(struct parser *p)
{
  if ((++p->cur) >= p->end) {
    ssize_t size = read(p->fd, p->buf, PARSE_BUFFER_SIZE);
    if (size == -1) {
      set_errorf(p, "read: %s", strerror(errno));
      return -1;
    }
    if (size == 0) {
      p->line++;
      p->col = 0;
      p->cur = NULL;
      return PARSE_EOF;
    }
    p->cur = p->buf;
    p->end = p->buf + size;
  }

  int c = *p->cur;
  if (c == '\n') {
    p->line++;
    p->col = 0;
  } else {
    p->col++;
  }
  return c;
}

static int parse_entry(struct parser *p)
{
  int c = cur_char(p);

  struct astr path_as;
  astr_init(&path_as);
  astr_append_char(&path_as, c);

  for (;;) {
    if ((c = next_char(p)) == -1) {
      astr_free(&path_as);
      return -1;
    }
    if (c == PARSE_EOF) {
      break;
    }
    if (c == '\0') {
      set_syntax_errorf(p, "expected path");
      astr_free(&path_as);
      return -1;
    }
    if (c == '\n') {
      if ((c = next_char(p)) == -1) {
        astr_free(&path_as);
        return -1;
      }
      break;
    }
    astr_append_char(&path_as, c);
  }

  int err = add_entry(p, path_as.buf);
  astr_free(&path_as);
  return err;
}

static int parse_log(struct parser *p, struct manifest *man, int fd)
{
  p->man = man;
  p->fd = fd;
  p->line = 1U;
  p->col = 1U;
  p->cur = p->buf;
  p->end = p->buf + 1;

  int c;
  if ((c = next_char(p)) == -1)
    return -1;

  for (;;) {
    c = cur_char(p);

    if (c == PARSE_EOF)
      break;

    if (c == '\0') {
      set_syntax_errorf(p, "expected path");
      return -1;
    }

    if (c == '\n') {
      if ((c = next_char(p)) == -1)
        return -1;
      continue;
    }

    if (parse_entry(p)) {
      return -1;
    }
  }

  return 0;
}

void manifest_init_log(struct manifest *man)
{
  man->oldlinks = NULL;
  man->oldlinks_tail = &man->oldlinks;
  man->need_update_log = false;
  man->error = NULL;
}

int manifest_load_log(struct manifest *man, const char *filename)
{
  GALE_TRACE("loading log from '%s'", filename);

  manifest_init_log(man);

  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    if (errno == ENOENT)
      return 0;

    man->error = xstrfmt("failed to open '%s': %s", filename, strerror(errno));
    return -1;
  }

  struct parser p;
  int err = parse_log(&p, man, fd);
  close(fd);
  return err;
}
