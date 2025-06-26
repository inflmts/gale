/*
 *  Gallade - The Gale Installer
 *
 *  Copyright (c) 2025 Daniel Li
 *
 *  This software is licensed under the MIT License.
 */

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#define GALLADE_CONFIG_READ_MAX 4096

static const char *log_file = ".data/gale/gallade.log";
static const char *config_root = ".gale";

static int dry_run = 0;
static int verbose = 0;
static int debug = 0;
static int need_update_log = 0;
static unsigned int new_outputs = 0;
static struct utsname sysinfo;

#define ISSPACE(c) ((c) == ' ')
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define ISUPPER(c) ((c) >= 'A' && (c) <= 'Z')
#define ISLOWER(c) ((c) >= 'a' && (c) <= 'z')
#define ISALNUM(c) (ISDIGIT(c) || ISUPPER(c) || ISLOWER(c))
#define ISPATH(c) (ISALNUM(c) || (c) == '.' || (c) == '-' || (c) == '_')

#ifdef __GNUC__
# define PRINTF(i, j) __attribute__((format(printf, i, j)))
#else
# define PRINTF(i, j)
#endif

PRINTF(2, 3)
static void msg(const char *prefix, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  if (prefix) fputs(prefix, stderr);
  vfprintf(stderr, format, ap);
  putc('\n', stderr);
  va_end(ap);
}

PRINTF(2, 3)
static void msg_sys(const char *prefix, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  if (prefix) fputs(prefix, stderr);
  vfprintf(stderr, format, ap);
  fprintf(stderr, ": %s\n", strerror(errno));
  va_end(ap);
}

static const char *prefix_warn = "warning: ";
static const char *prefix_err = "error: ";
static const char *prefix_debug = "debug: ";

static void enable_colors(void)
{
  prefix_warn = "\033[1;33mwarning:\033[0m ";
  prefix_err = "\033[1;31merror:\033[0m ";
  prefix_debug = "\033[1;36mdebug:\033[0m ";
}

#define MSG(...) msg(NULL, __VA_ARGS__)
#define WARN(...) msg(prefix_warn, __VA_ARGS__)
#define WARN_SYS(...) msg_sys(prefix_warn, __VA_ARGS__)
#define ERR(...) msg(prefix_err, __VA_ARGS__)
#define ERR_SYS(...) msg_sys(prefix_err, __VA_ARGS__)
#define DEBUG(...) msg(prefix_debug, __VA_ARGS__)

/*** OUTPUTS ******************************************************************/

struct output
{
  struct output *next;
  int logged;
  char path[PATH_MAX];
  char target[PATH_MAX];
};

static struct output *outputs = NULL;

static struct output *output_get(const char *path)
{
  struct output *output = outputs;
  while (output) {
    int diff = strcmp(path, output->path);
    if (diff < 0)
      return NULL;
    else if (diff == 0)
      return output;
    output = output->next;
  }
  return NULL;
}

static struct output *output_add(const char *path)
{
  struct output **slot = &outputs;
  while (*slot) {
    int diff = strcmp(path, (*slot)->path);
    if (diff < 0)
      break;
    else if (diff == 0)
      return NULL;
    slot = &(*slot)->next;
  }

  struct output *output = malloc(sizeof(struct output));
  output->next = *slot;
  output->logged = 0;
  strcpy(output->path, path);
  *slot = output;
  ++new_outputs;
  return output;
}

#define OUTPUT_ITER(item) \
  for (struct output *item = outputs; item; item = item->next)

/*** CONFIG PARSER ************************************************************/

struct config_parser
{
  const char *filename;
  unsigned int lineno;
  const char *cur;
  int match;
};

enum expr_type
{
  EXPR_NOT,
  EXPR_AND,
  EXPR_OR,
  EXPR_HOST
};

struct expr
{
  enum expr_type type;
  void *op1;
  void *op2;
};

#define s p->cur

static int config_parse_error(struct config_parser *p, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s%s: line %u: ", prefix_err, p->filename, p->lineno);
  vfprintf(stderr, format, ap);
  putc('\n', stderr);
  va_end(ap);
  return -1;
}

static int config_parse_token(struct config_parser *p, char *token, size_t size)
{
  while (ISSPACE(*s))
    ++s;
  const char *begin = s;
  if (!(*s >= '!' && *s <= '~'))
    return -1;
  do ++s;
  while (*s >= '!' && *s <= '~');
  size_t len = s - begin;
  if (len >= size)
    return -1;
  memcpy(token, begin, len);
  token[len] = '\0';
  return 0;
}

static int config_parse_path(struct config_parser *p, char *path, size_t size)
{
  // path must begin with a dot
  if (*s != '.')
    goto fail;
  const char *begin = s;
  while (1) {
    while (*s == '.')
      ++s;
    if (!ISPATH(*s))
      goto fail;
    do ++s;
    while (ISPATH(*s));
    if (*s != '/')
      break;
    ++s;
  }
  size_t len = s - begin;
  if (len >= size)
    goto fail;
  memcpy(path, begin, len);
  path[len] = '\0';
  return 0;

fail:
  config_parse_error(p, "invalid path");
  return -1;
}

static int config_parse_eol(struct config_parser *p)
{
  while (ISSPACE(*s))
    ++s;
  if (*s != '\n')
    goto fail;
  ++s;
  ++p->lineno;
  return 0;

fail:
  config_parse_error(p, "expected end of line");
  return -1;
}

static int config_parse_host(struct config_parser *p)
{
  char token[256];
  p->match = 0;
  while (!config_parse_token(p, token, sizeof(token))) {
    if (token[0] == '!') {
      if (!fnmatch(token + 1, sysinfo.nodename, 0))
        p->match = 0;
    } else {
      if (!fnmatch(token, sysinfo.nodename, 0))
        p->match = 1;
    }
  }
  return config_parse_eol(p);
}

static int config_parse_symlink(struct config_parser *p)
{
  char path[PATH_MAX];
  if (config_parse_path(p, path, sizeof(path))) return -1;
  if (config_parse_eol(p)) return -1;

  if (!p->match)
    return 0;

  struct output *output = output_add(path);
  if (!output) {
    config_parse_error(p, "output already defined: %s", path);
    return -1;
  }

  // count the number of slashes in the path
  int dircnt = 0;
  char *t;
  for (t = path; *t; ++t)
    if (*t == '/')
      ++dircnt;

  // "../" * dircnt + srcpath
  size_t target_len = 3 * dircnt + strlen(p->filename);
  if (target_len >= sizeof(output->target)) {
    config_parse_error(p, "target too long");
    return -1;
  }
  t = output->target;
  while (dircnt) {
    *(t++) = '.';
    *(t++) = '.';
    *(t++) = '/';
    --dircnt;
  }
  strcpy(t, p->filename);
  return 0;
}

static int config_parse_directive(struct config_parser *p)
{
  if (*s == '~') {
    if (*(++s) != '/')
      goto invalid;
    ++s;
    return config_parse_symlink(p);
  }

  char token[256];
  if (config_parse_token(p, token, sizeof(token)))
    goto invalid;

  if (!strcmp(token, "host"))
    return config_parse_host(p);

invalid:
  config_parse_error(p, "invalid directive");
  return -1;
}

static int config_parse_main(struct config_parser *p)
{
  const char *prefix_start = s;
  const char *prefix_end;

before_block:
  // Find the start of the config block, a line ending in whitespace and "---".
  if (!*s)
    return 0;

  if (*s == '\n') {
    prefix_start = ++s;
    ++p->lineno;
    goto before_block;
  }

  if ( *(s++) != ' ' ||
      (*s     != '-' && *s != '+') ||
      (*(++s) != '-' && *s != '+') ||
      (*(++s) != '-' && *s != '+') ||
       *(++s) != '\n') goto before_block;

  prefix_end = (++s) - 4;
  ++p->lineno;

inside_block:
  // skip prefix
  const char *t = prefix_start;
  while (t < prefix_end) {
    if (*t != *s) {
      config_parse_error(p, "expected matching prefix");
      return -1;
    }
    ++t;
    ++s;
  }

  if (*s == '-' || *s == '+')
    return 0;
  if (config_parse_directive(p))
    return -1;
  goto inside_block;
}

#undef s

static int config_parse(const char *filename, const char *buf)
{
  struct config_parser p;
  p.filename = filename;
  p.lineno = 0;
  p.cur = buf;
  p.match = 1;
  return config_parse_main(&p);
}

/*** CONFIG LOADER ************************************************************/

static int config_check_name(const char *name)
{
  // ignore names that begin with a dot
  if (*name == '.')
    return 0;
  while (ISPATH(*name))
    ++name;
  return *name == '\0';
}

static int qsort_strcmp(const void *lhs, const void *rhs)
{
  return strcmp(*(const char **)lhs, *(const char **)rhs);
}

static int config_load_file(const char *path)
{
  if (debug)
    DEBUG("config: %s", path);

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    ERR_SYS("failed to open '%s'", path);
    return -1;
  }

  char buf[GALLADE_CONFIG_READ_MAX];
  ssize_t size = read(fd, buf, sizeof(buf) - 1);
  if (size < 0) {
    ERR_SYS("read");
    close(fd);
    return -1;
  }
  close(fd);

  buf[size] = 0;
  return config_parse(path, buf);
}

static int config_load_dir(const char *path)
{
  if (debug)
    DEBUG("config: %s/", path);

  struct dirent *dent;
  DIR *dir = opendir(path);
  if (!dir) {
    ERR_SYS("failed to open '%s'", path);
    return -1;
  }

  int ret = 0;
  char **entries = NULL;
  size_t entrycnt = 0;
  size_t entrycap = 0;

  while (errno = 0, dent = readdir(dir)) {
    const char *name = dent->d_name;

    // ignore invalid filenames
    if (!config_check_name(name))
      continue;

    if (entrycnt == entrycap) {
      if (entrycap)
        entrycap *= 2;
      else
        entrycap = 4;
      entries = realloc(entries, entrycap * sizeof(*entries));
    }

    entries[entrycnt++] = strdup(name);
  }

  if (errno)
    ERR_SYS("readdir");
  closedir(dir);
  if (errno)
    goto finish;

  qsort(entries, entrycnt, sizeof(char *), qsort_strcmp);

  size_t pathlen = strlen(path);
  size_t subpathlen;
  char subpath[PATH_MAX];

  for (size_t i = 0; i < entrycnt; i++) {
    char *name = entries[i];
    size_t namelen = strlen(name);

    // construct pathname
    subpathlen = pathlen + 1 + namelen;
    if (subpathlen >= sizeof(subpath))
      continue;
    sprintf(subpath, "%s/%s", path, name);

    struct stat st;
    if (lstat(subpath, &st)) {
      ERR_SYS("failed to lstat '%s'", subpath);
      ret = -1;
    } else if (S_ISDIR(st.st_mode)) {
      if (config_load_dir(subpath))
        ret = -1;
    } else if (path && S_ISREG(st.st_mode)) {
      if (config_load_file(subpath))
        ret = -1;
    }
  }

finish:
  while (entrycnt)
    free(entries[--entrycnt]);
  free(entries);
  return ret;
}

/*** LOG READER ***************************************************************/

/*
 * Make a best-effort attempt to remove the parent directories of the specified
 * path. No error is reported if this fails. The path must be well-formed and
 * may be modified.
 */
static void remove_parent_directories(char *path)
{
  char *end = path;
  while (*end)
    ++end;
  while (end > path) {
    if (*end == '/') {
      *end = '\0';
      if (rmdir(path) && errno != ENOENT)
        return;
    }
    --end;
  }
}

/*
 * Make a best-effort attempt to remove the specified path and its parent
 * directories. The path must be well-formed and may be modified.
 */
static void remove_output_safe(char *path)
{
  struct stat st;
  if (lstat(path, &st)) {
    if (errno != ENOENT)
      WARN_SYS("failed to lstat '%s'", path);
    return;
  }
  if (!S_ISLNK(st.st_mode)) {
    WARN("refusing to remove non-symlink '%s'", path);
    return;
  }
  if (unlink(path)) {
    WARN_SYS("failed to unlink '%s'", path);
    return;
  }
  remove_parent_directories(path);
}

static int log_check_path(const char *s)
{
begin:
  while (*s == '.')
    ++s;
  // path component cannot contain only dots
  if (!ISPATH(*s))
    return 0;
  do ++s;
  while (ISPATH(*s));
  if (*s == '/') {
    ++s;
    goto begin;
  }
  return *s == '\0';
}

static int log_process_path(char *path)
{
  if (!log_check_path(path)) {
    ERR("log: invalid path '%s'", path);
    return -1;
  }

  struct output *output = output_get(path);
  if (!output) {
    need_update_log = 1;
    MSG("Removing: %s", path);
    if (!dry_run)
      remove_output_safe(path);
  } else if (!output->logged) {
    output->logged = 1;
    --new_outputs;
  }
  return 0;
}

static int log_load(const char *filename)
{
  char buf[4096];

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    if (errno == ENOENT)
      return 0;
    ERR_SYS("failed to open '%s'", filename);
    return -1;
  }

  char *start = buf;
  char *end;
  ssize_t size = sizeof(buf);
  while ((size = read(fd, start, size)) != 0) {
    if (size < 0) {
      ERR_SYS("read");
      goto fail;
    }
    start = buf;
    while ((end = memchr(start, '\n', size)) != NULL) {
      *end = '\0';
      if (log_process_path(start))
        goto fail;
      size -= end + 1 - start;
      start = end + 1;
    }
    if (start == buf) {
      ERR("log: path too long");
      goto fail;
    }
    memmove(buf, start, size);
    start = buf + size;
    size = sizeof(buf) - size;
  }

  close(fd);
  return 0;

fail:
  close(fd);
  return -1;
}

/*** LOG WRITER ***************************************************************/

static int log_write(const char *filename)
{
  int ret = -1;
  FILE *f = NULL;

  char tmpfile[PATH_MAX];
  if (snprintf(tmpfile, sizeof(tmpfile), "%s.tmp", filename) >= sizeof(tmpfile)) {
    ERR("log filename too long");
    goto finish;
  }

  int fd = open(tmpfile, O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (fd < 0) {
    ERR_SYS("failed to open '%s'", tmpfile);
    goto finish;
  }

  f = fdopen(fd, "w");
  if (!f) {
    ERR_SYS("fdopen");
    close(fd);
    goto finish;
  }

  // write outputs
  OUTPUT_ITER(output) {
    fprintf(f, "%s\n", output->path);
  }

  if (ferror(f)) {
    ERR("write error");
    goto finish;
  }

  fclose(f);
  f = NULL;

  if (rename(tmpfile, filename)) {
    ERR_SYS("failed to rename '%s' to '%s'", tmpfile, filename);
    goto finish;
  }

  ret = 0;

finish:
  if (f)
    fclose(f);
  return ret;
}

/*** EXECUTOR *****************************************************************/

static int create_parent_directories(char *path)
{
  char *slash = path;
  while ((slash = strchr(slash, '/'))) {
    *slash = '\0';
    if (mkdir(path, 0777) && errno != EEXIST) {
      ERR_SYS("failed to mkdir '%s'", path);
      *slash = '/';
      return -1;
    }
    *slash = '/';
    ++slash;
  }
  return 0;
}

static int execute_symlink(struct output *output)
{
  char existing[PATH_MAX];
  ssize_t existing_len = readlink(output->path, existing, sizeof(existing));

  if (existing_len < 0) {
    if (errno == EINVAL) {
      ERR("refusing to replace '%s'", output->path);
      return -1;
    }
    if (errno == ENOENT) {
      MSG("Creating symlink: %s -> %s", output->path, output->target);
      goto create;
    }
    ERR_SYS("failed to readlink '%s'", output->path);
    return -1;
  }

  size_t target_len = strlen(output->target);
  if (existing_len == target_len && !memcmp(existing, output->target, target_len)) {
    if (verbose)
      MSG("Skipping symlink: %s -> %s", output->path, output->target);
    return 0;
  }

  MSG("Replacing symlink: %s -> %s", output->path, output->target);
  if (!dry_run && unlink(output->path)) {
    ERR_SYS("failed to unlink '%s'", output->path);
    return -1;
  }

create:
  if (dry_run)
    return 0;

  if (!symlink(output->target, output->path))
    return 0;

  if (errno != ENOENT)
    goto symlink_failed;

  // create parent directories
  if (create_parent_directories(output->path))
    return -1;

  // try again
  if (!symlink(output->target, output->path))
    return 0;

symlink_failed:
  ERR_SYS("symlink");
  return -1;
}

static int execute(void)
{
  int ret = 0;
  OUTPUT_ITER(output) {
    if (execute_symlink(output))
      ret = -1;
  }
  return ret;
}

/*** OPTPARSE *****************************************************************/

struct optspec
{
  int value;
  const char *name;
};

struct optstate
{
  char **args;
  char *clump;
  int index;
  int count;
};

#define OPTINIT(args, count) { (args), NULL, 0, (count) }

int optparse(struct optstate *o, const struct optspec *options)
{
  if (!o->clump) {
    if (o->index >= o->count)
      return 0;
    char *arg = o->args[o->index++];
    if (*arg != '-' || !*(++arg)) {
      ERR("invalid argument: '%s'", arg);
      return -1;
    }
    if (*arg == '-') {
      ++arg;
      for (const struct optspec *option = options; option->value; ++option)
        if (!strcmp(option->name, arg))
          return option->value;
      ERR("invalid option: --%s", arg);
      return -1;
    }
    o->clump = arg;
  }
  int value = *o->clump;
  if (!*(++o->clump))
    o->clump = NULL;
  for (const struct optspec *option = options; option->value; ++option)
    if (option->value == value)
      return value;
  ERR("invalid option: -%c", value);
  return -1;
}

/*** MAIN *********************************************************************/

static const char usage[] = "\
usage: gallade [options]\n\
\n\
Options:\n\
  -h, --help        show this help and exit\n\
  -n, --dry-run     don't do anything, only show what would happen\n\
  -v, --verbose     be verbose (-vv for debug)\n\
";

int main(int argc, char **argv)
{
  if (isatty(2))
    enable_colors();

  int loglevel = 0;
  struct optstate optstate = OPTINIT(argv + 1, argc - 1);
  int opt;

  static const struct optspec options[] = {
    { 'h', "help" },
    { 'n', "dry-run" },
    { 'v', "verbose" },
    { 0 }
  };

  while ((opt = optparse(&optstate, options)) != 0) {
    switch (opt) {
      case 'h': fputs(usage, stdout); return 0;
      case 'n': dry_run = 1; break;
      case 'v': ++loglevel; break;
      default:  return 2;
    }
  }

  verbose = loglevel >= 1;
  debug = loglevel >= 2;

  // change to home directory
  const char *home = getenv("HOME");
  if (!home) {
    ERR("$HOME is not set");
    return 1;
  }
  if (chdir(home)) {
    ERR_SYS("could not chdir to '%s'", home);
    return 1;
  }

  // get hostname
  if (uname(&sysinfo)) {
    ERR_SYS("uname");
    return 1;
  }
  if (debug)
    DEBUG("hostname: %s", sysinfo.nodename);

  if (config_load_dir(config_root))
    return 1;

  if (log_load(log_file))
    return 1;

  if (need_update_log || new_outputs) {
    MSG("Updating log...");
    if (!dry_run)
      if (log_write(log_file))
        return 1;
  }

  if (execute())
    return 1;

  return 0;
}
