#include <fcntl.h> /* open */
#include <unistd.h> /* close, read */
#include "common.h"
#include "astr.h"
#include "config.h"
#include "uthash.h"

#define IS_LOWER(c) ((c) >= 'a' && (c) <= 'z')
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_KEY_BEGIN_CHAR(c) (IS_LOWER(c) || IS_DIGIT(c))
#define IS_KEY_CHAR(c) (IS_LOWER(c) || IS_DIGIT(c) || (c) == '-' || (c) == '.')

bool config_is_valid_key(const char *s)
{
  if (!IS_KEY_BEGIN_CHAR(*s))
    return false;
  while (*(++s))
    if (!IS_KEY_CHAR(*s))
      return false;
  return true;
}

static void config_init(struct config *conf)
{
  conf->head = NULL;
}

static void config_add_entry(struct config *conf, struct config_entry *ent)
{
  HASH_ADD_KEYPTR(hh, conf->head, ent->key, strlen(ent->key), ent);
}

static void config_delete_entry(struct config *conf, struct config_entry *ent)
{
  HASH_DEL(conf->head, ent);
}

static struct config_entry *config_get_entry(struct config *conf, const char *key)
{
  struct config_entry *ent;
  HASH_FIND(hh, conf->head, key, strlen(key), ent);
  return ent;
}

char *config_get(struct config *conf, const char *key)
{
  struct config_entry *ent = config_get_entry(conf, key);
  return ent ? ent->value : NULL;
}

void config_set(struct config *conf, const char *key, const char *value)
{
  struct config_entry *ent = config_get_entry(conf, key);
  if (ent) {
    free(ent->value);
    ent->value = xstrdup(value);
  } else {
    ent = xalloc(sizeof(struct config_entry));
    ent->key = xstrdup(key);
    ent->value = xstrdup(value);
    config_add_entry(conf, ent);
  }
}

bool config_unset(struct config *conf, const char *key)
{
  struct config_entry *ent = config_get_entry(conf, key);
  if (!ent)
    return false;

  config_delete_entry(conf, ent);
  free(ent->key);
  free(ent->value);
  free(ent);
  return true;
}

void config_free(struct config *conf)
{
  struct config_entry *ent, *tmp;
  HASH_ITER(hh, conf->head, ent, tmp) {
    config_delete_entry(conf, ent);
    free(ent->key);
    free(ent->value);
    free(ent);
  }
}

/*****************************************************************************
 * PARSER
 ****************************************************************************/

#define CONFIG_PARSE_BUFFER_SIZE 1024

struct config_parse_state
{
  struct config *conf;
  int fd;
  unsigned int line;
  unsigned int col;
  char *cur;
  char *end;
  char buf[CONFIG_PARSE_BUFFER_SIZE];
  char *errmsg;
};

static inline int config_parse_eof(struct config_parse_state *cp)
{
  return cp->cur == NULL;
}

// Returns 0 if the function succeeds or -1 if an error occurs.
static int config_parse_next(struct config_parse_state *cp)
{
  if ((++cp->cur) >= cp->end) {
    ssize_t size = read(cp->fd, cp->buf, sizeof(cp->buf));
    if (size == -1) {
      free(cp->errmsg);
      cp->errmsg = xstrdup("read error");
      return -1;
    }
    if (size == 0) {
      cp->cur = NULL;
      return 0;
    }
    cp->cur = cp->buf;
    cp->end = cp->buf + size;
  }
  if (*cp->cur == '\n') {
    cp->line++;
    cp->col = 0;
  } else {
    cp->col++;
  }
  return 0;
}

static void config_parse_set_syntax_error(
    struct config_parse_state *cp,
    const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  free(cp->errmsg);
  cp->errmsg = xstrfmt("syntax error at line %u: %s", cp->line, xvstrfmt(format, ap));
  va_end(ap);
}

static int config_parse_key(struct config_parse_state *cp, struct astr *key)
{
  astr_init(key);
  astr_append_char(key, *cp->cur);

  if (config_parse_next(cp))
    return -1;

  for (;;) {
    if (!config_parse_eof(cp)) {
      if (*cp->cur == '=') {
        if (config_parse_next(cp))
          return -1;
        break;
      }
      if (IS_KEY_CHAR(*cp->cur)) {
        astr_append_char(key, *cp->cur);
        if (config_parse_next(cp))
          return -1;
        continue;
      }
    }
    config_parse_set_syntax_error(cp, "expected '=' to terminate key");
    return -1;
  }

  return 0;
}

static int config_parse_value(struct config_parse_state *cp, struct astr *value)
{
  astr_init(value);

  for (;;) {
    if (config_parse_eof(cp))
      break;

    if (*cp->cur == '\0') {
      config_parse_set_syntax_error(cp, "invalid character");
      return -1;
    }

    if (*cp->cur == '\n') {
      if (config_parse_next(cp))
        return -1;
      break;
    }

    astr_append_char(value, *cp->cur);
    if (config_parse_next(cp))
      return -1;
  }

  return 0;
}

// Parse a configuration entry. `cp->cur` must point to a character for which
// IS_KEY_BEGIN_CHAR() returns true.
static int config_parse_entry(struct config_parse_state *cp)
{
  struct astr key;
  struct astr value;

  if (config_parse_key(cp, &key)) {
    astr_free(&key);
    return -1;
  }

  if (config_parse_value(cp, &value)) {
    astr_free(&key);
    astr_free(&value);
    return -1;
  }

  config_set(cp->conf, key.buf, value.buf);
  astr_free(&key);
  astr_free(&value);
  return 0;
}

static int config_parse_config(struct config_parse_state *cp)
{
  if (config_parse_next(cp))
    return -1;

  for (;;) {
    if (config_parse_eof(cp)) {
      return 0;
    } else if (*cp->cur == '\n') {
      if (config_parse_next(cp))
        return -1;
    } else if (IS_KEY_BEGIN_CHAR(*cp->cur)) {
      if (config_parse_entry(cp))
        return -1;
    } else {
      config_parse_set_syntax_error(cp, "expected key");
      return -1;
    }
  }
}

int config_parse(struct config *conf, int fd, char **errmsg)
{
  struct config_parse_state cp;
  cp.conf = conf;
  cp.fd = fd;
  cp.line = 1U;
  cp.col = 0U;
  cp.cur = (char *)NULL - 1;
  cp.end = (char *)NULL;
  cp.errmsg = NULL;

  config_init(conf);

  if (config_parse_config(&cp)) {
    *errmsg = cp.errmsg;
    config_free(conf);
    return -1;
  }

  return 0;
}

int config_load(struct config *conf, const char *filename, char **errmsg)
{
  int fd = open(filename, O_RDONLY);

  if (fd == -1) {
    if (errno == ENOENT) {
      config_init(conf);
      return 0;
    } else {
      *errmsg = xstrfmt("failed to open '%s'", filename);
      return -1;
    }
  }

  int ret = config_parse(conf, fd, errmsg);
  close(fd);
  return ret;
}
