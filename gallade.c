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
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*** UTILITIES ****************************************************************/

#define GALLADE_CONFIG_READ_MAX 4096

#define containerof(p, type, member) \
  ((type *)((char *)(p) - offsetof(type, member)))

#define ISSPACE(c) ((c) == ' ')
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define ISUPPER(c) ((c) >= 'A' && (c) <= 'Z')
#define ISLOWER(c) ((c) >= 'a' && (c) <= 'z')
#define ISALNUM(c) (ISDIGIT(c) || ISUPPER(c) || ISLOWER(c))
#define ISDIRECTIVE(c) ISLOWER(c)
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

static void *xmalloc(size_t size)
{
  void *p = malloc(size);
  if (!p) {
    ERR("out of memory");
    exit(-1);
  }
  return p;
}

static void *xrealloc(void *p, size_t size)
{
  p = realloc(p, size);
  if (!p) {
    ERR("out of memory");
    exit(-1);
  }
  return p;
}

#define xnew(type) ((type*)xmalloc(sizeof(type)))
#define xnewv(n, type) ((type*)xmalloc((n) * sizeof(type)))
#define xdefine(p, type) type *p = xmalloc(sizeof(type))
#define xresize(p, n) (p = xrealloc(p, n))
#define xresizev(p, n, type) (p = (type*)xrealloc(p, (n) * sizeof(type)))

/*** CONFIG *******************************************************************/

enum output_type {
  OUTPUT_NONE,
  OUTPUT_SYMLINK
};

#define OUTPUT_LOGGED 0x1

struct output_symlink
{
  char *target;
};

struct output
{
  struct output *next;
  char *path;
  enum output_type type;
  unsigned int flags;
  union {
    struct output_symlink symlink;
  };
};

#define CONFIG_DRY (1 << 0)
#define CONFIG_VERBOSE (1 << 1)
#define CONFIG_NEED_UPDATE_LOG (1 << 2)
#define CONFIG_BAD (1 << 3)
#define CONFIG_DEBUG (1 << 4)

struct config_file;

struct config
{
  unsigned int flags;
  const char *srcroot;
  struct config_file *files;
  struct output *outputs;
};

/*** OUTPUTS ******************************************************************/

static struct output *output_get(struct config *config, const char *path)
{
  struct output *output = config->outputs;
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

static struct output *output_add(struct config *config, const char *path)
{
  struct output **slot = &config->outputs;
  while (*slot) {
    int diff = strcmp(path, (*slot)->path);
    if (diff < 0)
      break;
    else if (diff == 0)
      return NULL;
    slot = &(*slot)->next;
  }

  xdefine(output, struct output);
  output->next = *slot;
  output->path = strdup(path);
  output->type = OUTPUT_NONE;
  output->flags = 0;
  *slot = output;
  return output;
}

#define OUTPUT_ITER(config, item) \
  for (struct output *item = (config)->outputs; item; item = item->next)

//static void output_destroy(struct output *output)
//{
//  free(output->path);
//  switch (output->type) {
//    case OUTPUT_SYMLINK:
//      free(output->symlink.target);
//      break;
//  }
//}
//
//static void config_free(struct config *config)
//{
//  struct output *output = config->outputs, *next;
//  while (output) {
//    next = output->next;
//    output_destroy(output);
//    output = next;
//  }
//}

/*** CONFIG *******************************************************************/

struct config_file
{
  struct config_file *next;
  char *path;
  struct config_directive *head;
  struct config_directive **tail;
};

/*** CONFIG PARSER ************************************************************/

struct config_parser
{
  struct config *config;
  const char *filename;
  unsigned int lineno;
  const char *cur;
  const char *end;
};

#define s p->cur

static void config_parse_error(struct config_parser *p, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s%s: line %u: ", prefix_err, p->filename, p->lineno);
  vfprintf(stderr, format, ap);
  putc('\n', stderr);
  va_end(ap);
}

static int config_parse_path(struct config_parser *p, char **pathp)
{
  // path must begin with a dot
  if (*s != '.')
    return -1;
  const char *begin = s;
  while (1) {
    while (*s == '.')
      ++s;
    if (!ISPATH(*s))
      return -1;
    do ++s;
    while (ISPATH(*s));
    if (*s != '/')
      break;
    ++s;
  }
  size_t len = s - begin;
  *pathp = memcpy(xmalloc(len + 1), begin, len);
  (*pathp)[len] = '\0';
  return 0;
}

static int config_parse_eol(struct config_parser *p)
{
  while (ISSPACE(*s))
    ++s;
  if (*s != '\n')
    return -1;
  ++s;
  ++p->lineno;
  return 0;
}

static int config_parse_symlink(struct config_parser *p)
{
  char *path = NULL;
  if (config_parse_path(p, &path)) goto fail;
  if (config_parse_eol(p)) goto fail;

  struct output *output = output_add(p->config, path);
  if (!output) {
    config_parse_error(p, "output already defined: %s", path);
    goto fail;
  }

  // count the number of slashes in the path
  int dircnt = 0;
  char *t;
  for (t = path; *t; ++t)
    if (*t == '/')
      ++dircnt;

  // "../" * dircnt + srcroot + "/" + srcpath
  size_t target_len = 3 * dircnt + strlen(p->config->srcroot) + 1 + strlen(p->filename);
  char *target = xmalloc(target_len + 1);
  t = target;
  while (dircnt) {
    *(t++) = '.';
    *(t++) = '.';
    *(t++) = '/';
    --dircnt;
  }
  sprintf(t, "%s/%s", p->config->srcroot, p->filename);

  output->type = OUTPUT_SYMLINK;
  output->flags = 0;
  output->path = path;
  output->symlink.target = target;
  return 0;

fail:
  free(path);
  return -1;
}

static int config_parse_directive(struct config_parser *p)
{
  // Currently, only the symlink directive is supported.
  if (*s == '~') {
    if (*(++s) != '/')
      goto invalid;
    ++s;
    return config_parse_symlink(p);
  }

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

static int config_parse(struct config *config, const char *filename, const char *buf)
{
  struct config_parser p;
  p.config = config;
  p.filename = filename;
  p.lineno = 0;
  p.cur = buf;
  return config_parse_main(&p);
}

/*** CONFIG LOADER ************************************************************/

static void config_load_file(struct config *config, const char *path)
{
  if (config->flags & CONFIG_DEBUG)
    DEBUG("config: %s", path);

  char *fullpath = xmalloc(strlen(config->srcroot) + 1 + strlen(path) + 1);
  sprintf(fullpath, "%s/%s", config->srcroot, path);

  int fd = open(fullpath, O_RDONLY);
  if (fd < 0) {
    ERR_SYS("failed to open '%s'", fullpath);
    free(fullpath);
    config->flags |= CONFIG_BAD;
    return;
  }
  free(fullpath);

  char buf[GALLADE_CONFIG_READ_MAX];
  ssize_t size = read(fd, buf, sizeof(buf) - 1);
  if (size < 0) {
    ERR_SYS("read");
    close(fd);
    return;
  }
  close(fd);

  buf[size] = 0;
  if (config_parse(config, path, buf))
    config->flags |= CONFIG_BAD;
}

static int config_check_name(const char *name)
{
  // ignore names that begin with a dot
  if (*name == '.')
    return 0;
  while (*name) {
    if (!(ISALNUM(*name) || *name == '.' || *name == '-'))
      return 0;
    ++name;
  }
  return 1;
}

static int qsort_strcmp(const void *lhs, const void *rhs)
{
  return strcmp(*(const char **)lhs, *(const char **)rhs);
}

static void config_load_dir(struct config *config, const char *path)
{
  if (path && config->flags & CONFIG_DEBUG)
    DEBUG("config: %s/", path);

  char *fullpath;
  size_t pathlen;
  if (path) {
    pathlen = strlen(path);
    fullpath = xmalloc(strlen(config->srcroot) + 1 + pathlen + 1);
    sprintf(fullpath, "%s/%s", config->srcroot, path);
  } else {
    fullpath = strdup(config->srcroot);
  }

  struct dirent *dent;
  DIR *dir = opendir(fullpath);
  if (!dir) {
    ERR_SYS("failed to open '%s'", fullpath);
    free(fullpath);
    config->flags |= CONFIG_BAD;
    return;
  }
  free(fullpath);

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
      xresizev(entries, entrycap, char *);
    }

    size_t namesize = strlen(name) + 1;
    entries[entrycnt++] = memcpy(xmalloc(namesize), name, namesize);
  }

  if (errno) {
    ERR_SYS("readdir");
    config->flags |= CONFIG_BAD;
    goto finish;
  }

  closedir(dir);
  dir = NULL;

  qsort(entries, entrycnt, sizeof(char *), qsort_strcmp);

  char *subpath;
  size_t subpathlen;

  for (size_t i = 0; i < entrycnt; i++) {
    char *name = entries[i];
    size_t namelen = strlen(name);

    // construct pathname
    if (path) {
      subpathlen = pathlen + 1 + namelen;
      subpath = xmalloc(subpathlen + 1);
      sprintf(subpath, "%s/%s", path, name);
    } else {
      subpathlen = namelen;
      subpath = name;
    }

    fullpath = xmalloc(strlen(config->srcroot) + 1 + subpathlen + 1);
    sprintf(fullpath, "%s/%s", config->srcroot, subpath);

    struct stat st;
    if (lstat(fullpath, &st)) {
      ERR_SYS("failed to lstat '%s'", fullpath);
      free(fullpath);
      if (subpath != name)
        free(subpath);
      config->flags |= CONFIG_BAD;
      continue;
    }
    free(fullpath);

    if (S_ISDIR(st.st_mode))
      config_load_dir(config, subpath);
    else if (path && S_ISREG(st.st_mode))
      config_load_file(config, subpath);

    if (subpath != name)
      free(subpath);
  }

finish:
  while (entrycnt)
    free(entries[--entrycnt]);
  free(entries);
  if (dir)
    closedir(dir);
}

static int config_load(struct config *config)
{
  config_load_dir(config, NULL);
  return config->flags & CONFIG_BAD;
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

static int log_parse(struct config *config, const char *s, size_t len)
{
  const char *end = s + len;
  unsigned int lineno = 1;
  char path[PATH_MAX];

  while (s < end) {
    const char *begin = s;
    // path must begin with a dot
    if (*s != '.')
      goto invalid_path;
    while (1) {
      while (*s == '.')
        ++s;
      // path component cannot contain only dots
      if (!ISPATH(*s))
        goto invalid_path;
      do ++s;
      while (ISPATH(*s));
      if (*s != '/')
        break;
      ++s;
    }
    if (*s != '\n')
      goto invalid_path;

    size_t len = s - begin;
    if (len >= sizeof(path)) {
      ERR("log: path too long at line %u", lineno);
      return -1;
    }
    memcpy(path, begin, len);
    path[len] = '\0';

    struct output *output = output_get(config, path);
    if (output) {
      if (output->flags & OUTPUT_LOGGED) {
        config->flags |= CONFIG_NEED_UPDATE_LOG;
      } else {
        output->flags |= OUTPUT_LOGGED;
      }
      goto next;
    }

    // remove
    config->flags |= CONFIG_NEED_UPDATE_LOG;
    MSG("Removing: %s", path);
    if (!(config->flags & CONFIG_DRY))
      remove_output_safe(path);
    goto next;

invalid_path:
    while (*s != '\n')
      ++s;
    ERR("log: invalid path at line %u", lineno);
    return -1;

next:
    ++s;
    ++lineno;
  }

  return 0;
}

static int log_read(struct config *config, const char *filename)
{
  struct stat st;
  if (stat(filename, &st)) {
    if (errno == ENOENT)
      return 0;
    ERR_SYS("failed to stat '%s'", filename);
    return -1;
  }

  size_t size = st.st_size;
  if (size == 0)
    // empty log file
    return 0;

  // allocate extra byte for terminating newline
  char *buf = malloc(size + 1);
  if (!buf) {
    ERR("log file too large");
    return -1;
  }

  int ret = -1;
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    ERR_SYS("failed to open '%s'", filename);
    goto finish;
  }

  char *cur = buf;
  char *end = buf + size;
  while (cur < end) {
    ssize_t readsize = read(fd, cur, end - cur);
    if (readsize < 0) {
      ERR_SYS("read");
      goto finish;
    }
    if (readsize == 0) {
      // log file shrank?
      size = cur - buf;
      break;
    }
    cur += readsize;
  }

  close(fd);
  fd = -1;

  if (buf[size - 1] != '\n')
    buf[size++] = '\n';

  ret = log_parse(config, buf, size);

finish:
  if (fd >= 0)
    close(fd);
  free(buf);
  return ret;
}

/*** LOG WRITER ***************************************************************/

static int log_write(struct config *config, const char *filename)
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
  OUTPUT_ITER(config, output) {
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

static int execute_symlink(struct config *config, struct output *output)
{
  char existing[PATH_MAX];
  const char *target = output->symlink.target;
  size_t target_len = strlen(target);
  ssize_t existing_len = readlink(output->path, existing, target_len + 1);
  if (existing_len < 0) {
    if (errno == EINVAL) {
      ERR("refusing to replace '%s'", output->path);
      return -1;
    }
    if (errno != ENOENT) {
      ERR_SYS("failed to readlink '%s'", output->path);
      return -1;
    }
    MSG("Creating symlink: %s -> %s", output->path, target);
  } else if (existing_len == target_len && !memcmp(existing, target, target_len)) {
    if (config->flags & CONFIG_VERBOSE)
      MSG("Skipping symlink: %s -> %s", output->path, target);
    return 0;
  } else {
    MSG("Replacing symlink: %s -> %s", output->path, target);
    if (!(config->flags & CONFIG_DRY) && unlink(output->path)) {
      ERR_SYS("failed to unlink '%s'", output->path);
      return -1;
    }
  }

  if (config->flags & CONFIG_DRY)
    return 0;

  if (!symlink(target, output->path))
    return 0;

  if (errno != ENOENT)
    goto symlink_failed;

  // create parent directories
  if (create_parent_directories(output->path))
    return -1;

  // try again
  if (!symlink(target, output->path))
    return 0;

symlink_failed:
  ERR_SYS("symlink");
  return -1;
}

static int execute(struct config *config)
{
  int ret = 0;
  OUTPUT_ITER(config, output) {
    switch (output->type) {
      case OUTPUT_NONE:
        break;
      case OUTPUT_SYMLINK:
        if (execute_symlink(config, output))
          ret = -1;
        break;
    }
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
  -v, --verbose     be verbose\n\
";

int main(int argc, char **argv)
{
  if (isatty(2))
    enable_colors();

  const char *home = getenv("HOME");
  if (!home) {
    ERR("$HOME is not set");
    return 1;
  }
  if (chdir(home)) {
    ERR_SYS("could not chdir to '%s'", home);
    return 1;
  }

  unsigned int flags = 0;
  struct optstate optstate = OPTINIT(argv + 1, argc - 1);
  int opt;

  static const struct optspec options[] = {
    { 'h', "help" },
    { 'n', "dry-run" },
    { 'v', "verbose" },
    { 'd', "debug" },
    { 0 }
  };

  while ((opt = optparse(&optstate, options)) != 0) {
    switch (opt) {
      case 'h': fputs(usage, stdout); return 0;
      case 'n': flags |= CONFIG_DRY; break;
      case 'v': flags |= CONFIG_VERBOSE; break;
      case 'd': flags |= CONFIG_DEBUG; break;
      default:  return 2;
    }
  }

  struct config config;
  config.flags = flags;
  config.srcroot = ".gale";
  config.files = NULL;
  config.outputs = NULL;

  if (config_load(&config))
    return 1;

  if (log_read(&config, ".gale.log"))
    return 1;

  if (config.flags & CONFIG_NEED_UPDATE_LOG) {
    MSG("Updating log...");
    if (!(config.flags & CONFIG_DRY) && log_write(&config, ".gale.log"))
      return 1;
  }

  if (execute(&config))
    return 1;

  return 0;
}
