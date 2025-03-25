#define _ /*
#-------------------------------------------------------------------------------
#
#   gallade - The Gale Installer
#
#   Copyright (c) 2025 Daniel Li
#
#   This software is licensed under the MIT License.
#
#-------------------------------------------------------------------------------
set -ue
flags="-O2"
while [ $# -gt 0 ]; do
  case $1 in
    --debug) flags="-DGALLADE_DEBUG -g" ;;
    --) shift; break ;;
    *) echo >&2 "usage: sh $0 [--debug] -- [options]"; exit 2 ;;
  esac
  shift
done
bin=~/.local/bin/gallade
mkdir -p ~/.local/bin
gcc -std=c99 -Wall $flags -o "$bin.tmp" "$0"
mv "$bin.tmp" "$bin"
exec "$bin" "$@"
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
#include <sys/wait.h>
#include <unistd.h>

/*** UTILITIES ********************************************************* {{{1 */

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

/*** LOGGING *********************************************************** {{{1 */

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
#ifdef GALLADE_DEBUG
# define DEBUG(...) msg(prefix_debug, __VA_ARGS__)
#else
# define DEBUG(...) (void)0
#endif

/*** MEMORY ************************************************************ {{{1 */

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

/*** STATE ************************************************************* {{{1 */

enum output_type {
  //OUTPUT_NONE,
  OUTPUT_SYMLINK = 1
};

#define OUTPUT_LOGGED 0x1

struct output
{
  struct output *next;
  enum output_type type;
  unsigned int flags;
  char *path;
  struct {
    char *target;
  } symlink;
};

#define STATE_DRY 0x1
#define STATE_VERBOSE 0x2
#define STATE_NEED_UPDATE_LOG 0x4

struct config_file;

struct state
{
  unsigned int flags;
  const char *srcroot;
  struct config_file *files;
  struct output *outputs;
  unsigned int outputcnt;
};

/*** STATE / UTILITIES ************************************************* {{{1 */

static struct output *output_get(struct state *state, const char *path)
{
  struct output *output = state->outputs;
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

static struct output *output_add(struct state *state, const char *path)
{
  struct output **slot = &state->outputs;
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
  *slot = output;
  ++state->outputcnt;
  return output;
}

#define OUTPUT_ITER(state, item) \
  for (struct output *item = (state)->outputs; item; item = item->next)

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
//static void state_free(struct state *state)
//{
//  struct output *output = state->outputs, *next;
//  while (output) {
//    next = output->next;
//    output_destroy(output);
//    output = next;
//  }
//}

/*** CONFIG ************************************************************ {{{1 */

enum config_directive_type
{
  CONFIG_SYMLINK
};

struct config_directive
{
  struct config_directive *next;
  enum config_directive_type type;
  unsigned int lineno;
  union {
    struct {
      char *path;
    } symlink;
  };
};

struct config_file
{
  struct config_file *next;
  char *path;
  struct config_directive *head;
  struct config_directive **tail;
};

/*** CONFIG / UTILITIES ************************************************ {{{1 */

static struct config_directive *config_add_directive(struct config_file *file)
{
  xdefine(directive, struct config_directive);
  directive->next = NULL;
  *file->tail = directive;
  file->tail = &directive->next;
  return directive;
}

/*** CONFIG / PARSER *************************************************** {{{1 */

#define CONFIG_HAS_DIRECTIVES 0x1

struct config_parser
{
  struct config_file *file;
  unsigned int lineno;
  unsigned int flags;
  const char *errmsg;
  const char *cur;
  const char *end;
};

#define s p->cur

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

//static int config_parse_space(struct config_parser *p)
//{
//  if (!ISSPACE(*s))
//    return -1;
//  do ++s;
//  while (ISSPACE(*s));
//  return 0;
//}

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
  unsigned int lineno = p->lineno;
  char *path = NULL;
  if (config_parse_path(p, &path)) goto fail;
  if (config_parse_eol(p)) goto fail;

  struct config_directive *directive = config_add_directive(p->file);
  directive->type = CONFIG_SYMLINK;
  directive->lineno = lineno;
  directive->symlink.path = path;
  return 0;

fail:
  free(path);
  return -1;
}

static int config_parse_directive(struct config_parser *p)
{
  if (s[0] == '~' && s[1] == '/') {
    s += 2;
    return config_parse_symlink(p);
  }

  //if (!ISLOWER(*s))
  //  return -1;
  //const char *name = s;
  //do ++s;
  //while (ISLOWER(*s));
  //size_t len = s - name;

  p->errmsg = "invalid command";
  return -1;
}

static int config_parse_main(struct config_parser *p)
{
  while (s < p->end) {
    ++p->lineno;

    while (*s != '\n') {
      // find directive signature (four colons, bar, and whitespace)
      if (*(s++) != ':' ||
          *s     != ':' ||
          *(++s) != ':' ||
          *(++s) != ':' ||
          *(++s) != '|' ||
          !ISSPACE(*(++s))) continue;

      do ++s;
      while (ISSPACE(*s));
      if (config_parse_directive(p))
        return -1;
      p->flags |= CONFIG_HAS_DIRECTIVES;
    }
    ++s;
  }
  return 0;
}

#undef s

static int config_parse(struct config_file *file, const char *buf, size_t size)
{
  struct config_parser p;
  p.file = file;
  p.lineno = 0;
  p.flags = 0;
  p.errmsg = "invalid syntax";
  p.cur = buf;
  p.end = buf + size;

  if (config_parse_main(&p)) {
    ERR("%s: line %u: %s", file->path, p.lineno, p.errmsg);
    return -1;
  }

  if (!(p.flags & CONFIG_HAS_DIRECTIVES))
    WARN("%s: no directives found", file->path);

  return 0;
}

/*** CONFIG / LOADER *************************************************** {{{1 */

struct config_loader
{
  struct state *state;
  char *path;
  size_t pathlen;
  size_t pathcap;
  size_t prefixlen;
};

static int config_load_alloc_path(struct config_loader *loader)
{
  if (loader->pathlen >= loader->pathcap) {
    loader->pathcap = loader->pathlen + 1;
    xresize(loader->path, loader->pathcap);
  }
  return 0;
}

static int config_load_file(struct config_loader *loader, struct config_file *file)
{
  DEBUG("%s: %s", __func__, loader->path);

  int fd = open(loader->path, O_RDONLY);
  if (fd < 0) {
    ERR_SYS("failed to open '%s'", loader->path);
    return -1;
  }

  int ret = -1;
  char buf[GALLADE_CONFIG_READ_MAX];
  ssize_t size = read(fd, buf, sizeof(buf) - 1);
  if (size < 0) {
    ERR_SYS("read");
    goto finish;
  }
  if (size == 0) {
    // empty file
    goto finish;
  }

  close(fd);
  fd = -1;

  // pad with a newline
  buf[size++] = '\n';

  ret = config_parse(file, buf, size);

finish:
  if (fd >= 0)
    close(fd);
  return ret;
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

static int config_load_dir(struct config_loader *loader,
                           struct config_file ***slotp, int root)
{
  DEBUG("%s: %s", __func__, loader->path);

  size_t pathlen = loader->pathlen;
  loader->pathlen += 9;
  config_load_alloc_path(loader);
  memcpy(loader->path + pathlen, "/.disable", 10);

  // check for disable file
  struct stat st;
  if (!lstat(loader->path, &st)) {
    return 0;
  } else if (errno != ENOENT) {
    ERR_SYS("failed to lstat '%s'", loader->path);
    return -1;
  }

  loader->path[pathlen] = '\0';

  struct dirent *dent;
  DIR *d = opendir(loader->path);
  if (!d) {
    ERR_SYS("failed to open '%s'", loader->path);
    return -1;
  }

  struct config_file *end = **slotp;
  loader->path[pathlen] = '/';

  while (errno = 0, dent = readdir(d)) {
    const char *name = dent->d_name;

    // ignore invalid filenames
    if (!config_check_name(name))
      continue;

    int type = dent->d_type;

    // skip entries that are not directories or non-toplevel files
    if (type != DT_UNKNOWN && type != DT_DIR && (root || type != DT_REG))
      continue;

    // construct pathname
    size_t namelen = strlen(name);
    loader->pathlen = pathlen + 1 + namelen;
    config_load_alloc_path(loader);
    memcpy(loader->path + pathlen + 1, name, namelen + 1);

    if (type == DT_UNKNOWN) {
      // determine type using lstat()
      if (lstat(loader->path, &st)) {
        ERR_SYS("failed to lstat '%s'", loader->path);
        return -1;
      }
      if (S_ISDIR(st.st_mode))
        type = DT_DIR;
      else if (!root && S_ISREG(st.st_mode))
        type = DT_REG;
      else
        continue;
    }

    struct config_file **slot = *slotp;
    while (*slot != end) {
      int diff = strcmp(name, (*slot)->path + pathlen + 1 - loader->prefixlen);
      if (diff < 0) {
        break;
      } else if (diff == 0) {
        ERR("readdir returned duplicate entry '%s'", name);
        return -1;
      }
      slot = &(*slot)->next;
    }

    // construct pathname
    size_t basesize = pathlen - loader->prefixlen + 1 + namelen + 1;
    char *base = memcpy(xmalloc(basesize), loader->path + loader->prefixlen, basesize);

    // insert file
    xdefine(file, struct config_file);
    file->next = *slot;
    file->path = base;
    file->head = NULL;
    file->tail = type == DT_DIR ? NULL : &file->head;
    *slot = file;
  }

  if (errno) {
    ERR_SYS("readdir");
    closedir(d);
    return -1;
  }

  closedir(d);
  d = NULL;

  struct config_file *file;
  while ((file = **slotp) != end) {
    // construct pathname
    const char *name = file->path + pathlen + 1 - loader->prefixlen;
    size_t namelen = strlen(name);
    loader->pathlen = pathlen + 1 + namelen;
    config_load_alloc_path(loader);
    memcpy(loader->path + pathlen + 1, name, namelen + 1);

    if (file->tail) {
      // file
      config_load_file(loader, file);
      *slotp = &file->next;
    } else {
      // directory
      **slotp = file->next;
      free(file->path);
      free(file);
      config_load_dir(loader, slotp, 0);
    }
  }

  loader->path[pathlen] = '\0';
  loader->pathlen = pathlen;

  return 0;
}

static int config_load(struct state *state)
{
  struct stat st;
  if (stat(state->srcroot, &st)) {
    ERR_SYS("failed to stat '%s'", state->srcroot);
    return -1;
  }

  size_t rootlen = strlen(state->srcroot);

  struct config_loader loader;
  loader.state = state;
  loader.path = memcpy(xmalloc(4096), state->srcroot, rootlen + 1);
  loader.pathlen = rootlen;
  loader.pathcap = 4096;
  loader.prefixlen = rootlen + 1;

  struct config_file **slot = &state->files;
  int ret = config_load_dir(&loader, &slot, 1);
  free(loader.path);
  return ret;
}

/*** LOG / PARSER ****************************************************** {{{1 */

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

static int log_parse(struct state *state, const char *s, size_t len)
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

    struct output *output = output_get(state, path);
    if (output) {
      if (output->flags & OUTPUT_LOGGED) {
        state->flags |= STATE_NEED_UPDATE_LOG;
      } else {
        output->flags |= OUTPUT_LOGGED;
        --state->outputcnt;
      }
      goto next;
    }

    // remove
    state->flags |= STATE_NEED_UPDATE_LOG;
    MSG("Removing: %s", path);
    if (!(state->flags & STATE_DRY))
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

/*** LOG / READER ****************************************************** {{{1 */

static int log_read(struct state *state, const char *filename)
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

  ret = log_parse(state, buf, size);

finish:
  if (fd >= 0)
    close(fd);
  free(buf);
  return ret;
}

/*** LOG / WRITER ****************************************************** {{{1 */

static int log_write(struct state *state, const char *filename)
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
  OUTPUT_ITER(state, output) {
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

/*** RESOLVER ********************************************************** {{{1 */

static int resolve_symlink(struct state *state, struct config_file *file,
                           struct config_directive *directive)
{
  char *path = directive->symlink.path;
  struct output *output = output_add(state, path);
  if (!output) {
    ERR("multiple definitions for '%s'", path);
    return -1;
  }

  // count the number of slashes in the path
  int dircnt = 0;
  char *s;
  for (s = path; *s; ++s)
    if (*s == '/')
      ++dircnt;

  // "../" * dircnt + srcroot + "/" + srcpath
  size_t target_len = 3 * dircnt + strlen(state->srcroot) + 1 + strlen(file->path);
  char *target = xmalloc(target_len + 1);
  s = target;
  while (dircnt) {
    *(s++) = '.';
    *(s++) = '.';
    *(s++) = '/';
    --dircnt;
  }
  sprintf(s, "%s/%s", state->srcroot, file->path);

  path = strdup(path);
  output->type = OUTPUT_SYMLINK;
  output->flags = 0;
  output->path = path;
  output->symlink.target = target;
  return 0;
}

static int resolve_file(struct state *state, struct config_file *file)
{
  struct config_directive *directive = file->head;
  while (directive) {
    switch (directive->type) {
      case CONFIG_SYMLINK:
        if (resolve_symlink(state, file, directive))
          return -1;
        break;
    }
    directive = directive->next;
  }
  return 0;
}

static int resolve(struct state *state)
{
  struct config_file *file = state->files;
  while (file) {
    if (resolve_file(state, file))
      return -1;
    file = file->next;
  }
  return 0;
}

/*** EXECUTOR ********************************************************** {{{1 */

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

static int execute_symlink(struct state *state, struct output *output)
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
    if (state->flags & STATE_VERBOSE)
      MSG("Skipping symlink: %s -> %s", output->path, target);
    return 0;
  } else {
    MSG("Replacing symlink: %s -> %s", output->path, target);
    if (!(state->flags & STATE_DRY) && unlink(output->path)) {
      ERR_SYS("failed to unlink '%s'", output->path);
      return -1;
    }
  }

  if (state->flags & STATE_DRY)
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

static int execute(struct state *state)
{
  int ret = 0;
  OUTPUT_ITER(state, output) {
    switch (output->type) {
      case OUTPUT_SYMLINK:
        if (execute_symlink(state, output))
          ret = -1;
        break;
    }
  }
  return ret;
}

/*** AUTOCOMPILE ******************************************************* {{{1 */

/*
 * Because the Gale configuration language is closely tied to the Gallade
 * implementation, Gallade will attempt to recompile itself if it detects it is
 * out of date.
 */
static void autocompile(int argc, char **argv)
{
  static char *const srcfile = ".gale/gallade.c";
  static char *const exefile = ".local/bin/gallade";

  if (argc > 27) {
    WARN("too many arguments");
    return;
  }

  struct stat st;
  long long srctime, exetime;
  if (stat(srcfile, &st)) {
    WARN_SYS("failed to stat '%s'", srcfile);
    return;
  }
  srctime = st.st_mtime;
  if (stat(exefile, &st)) {
    WARN_SYS("failed to stat '%s'", exefile);
    return;
  }
  exetime = st.st_mtime;
  if (srctime <= exetime)
    return;

  MSG("Recompiling...");

  char *exec_argv[32];
  int i = 0;
  exec_argv[i++] = "sh";
  exec_argv[i++] = srcfile;
#ifdef GALLADE_DEBUG
  exec_argv[i++] = "--debug";
#endif
  exec_argv[i++] = "--";
  for (int j = 0; j < argc; j++)
    exec_argv[i++] = argv[j];
  exec_argv[i] = NULL;

  execv("/bin/sh", exec_argv);
  ERR_SYS("execv");
  exit(-1);
}

/*** OPTION PARSER ***************************************************** {{{1 */

// Screw getopt! This is a lot more painful to read and a lot more fun to use.

#define OPTBEGIN(argv) \
  for (char **_argv = (argv), *_clump = NULL, *_arg, _s;;) { \
    if (_clump) { \
      _s = *_clump; \
      if (!*(++_clump)) \
        _clump = NULL; \
    } else if ((_arg = *_argv)) { \
      ++_argv; \
      if (_arg[0] != '-' || _arg[1] == 0) \
        ERR("invalid argument: '%s'", _arg), exit(2); \
      ++_arg; _s = *_arg; ++_arg; \
      if (_s == '-') _s = 0; \
      else if (*_arg) _clump = _arg; \
    } else break; \
    if (0)

#define OPT(s, l) \
  } else if (_s ? _s == s : !strcmp(_arg, l)) {

#define OPTLONG(l) \
  } else if (!_s && !strcmp(_arg, l)) {

#define OPTEND \
  else { \
    if (_s) ERR("invalid option: -%c", _s); \
    else ERR("invalid option: --%s", _arg); \
    exit(2); \
  }}

/*** MAIN ************************************************************** {{{1 */

#ifdef GALLADE_DEBUG
# define GALLADE_DEBUG_INDICATOR " (debug build)"
#else
# define GALLADE_DEBUG_INDICATOR
#endif

static const char usage[] = "\
usage: gallade [options]" GALLADE_DEBUG_INDICATOR "\n\
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

  autocompile(argc - 1, argv + 1);

  unsigned int flags = 0;

  OPTBEGIN(argv + 1) {
    OPT('h', "help") fputs(usage, stdout); return 0;
    OPT('n', "dry-run") flags |= STATE_DRY;
    OPT('v', "verbose") flags |= STATE_VERBOSE;
  } OPTEND

  //= Putting it all together: =================================================

  struct state state;
  state.flags = flags;
  state.srcroot = ".gale";
  state.files = NULL;
  state.outputs = NULL;
  state.outputcnt = 0;

  // 1. Load the config. -------------------------------------------------------

  if (config_load(&state))
    return 1;

  // 2. Resolve the config. ----------------------------------------------------

  if (resolve(&state))
    return 1;

  // 3. Load the log and remove stale entries. ---------------------------------

  if (log_read(&state, ".gale.log"))
    return 1;

  // 4. Update the log if necessary. -------------------------------------------

  if (state.outputcnt || state.flags & STATE_NEED_UPDATE_LOG) {
    MSG("Updating log...");
    if (!(state.flags & STATE_DRY) && log_write(&state, ".gale.log"))
      return 1;
  }

  // 5. Update the home directory. ---------------------------------------------

  if (execute(&state))
    return 1;

  // ...maybe clean up one day. ------------------------------------------------

  return 0;
}

/*********************************************************************** }}}1 */

// vim:fo+=n:fdm=marker
