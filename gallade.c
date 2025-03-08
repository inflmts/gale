#if 0
#-------------------------------------------------------------------------------
#
#   gallade - The Gale Installer
#
#   Copyright (c) 2025 Daniel Li
#
#   This software is licensed under the MIT License.
#
#-------------------------------------------------------------------------------
case ${1-} in
  --debug) flags="-DGALLADE_DEBUG -g" ;;
  '')      flags="-O2" ;;
  *) echo >&2 "usage: $0 [--debug]"; exit 2 ;;
esac
mkdir -p ~/.local/bin
exec gcc -std=c99 -Wall $flags -o ~/.local/bin/gallade "$0"
#endif

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h> /* PATH_MAX */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define GALLADE_CONFIG_READ_MAX 4096
#define GALLADE_SRC_PATH_MAX 128

/*** UTILITIES ********************************************************* {{{1 */

#define containerof(p, type, member) \
  ((type *)((char *)(p) - offsetof(type, member)))

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
#define ERR(...) msg(prefix_err, __VA_ARGS__)
#define ERR_SYS(...) msg_sys(prefix_err, __VA_ARGS__)
#define DIE(n, ...) (msg(prefix_err, __VA_ARGS__), exit(n))
#define DIE_SYS(n, ...) (msg_sys(prefix_err, __VA_ARGS__), exit(n))
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

#define ISSPACE(c) ((c) == ' ')
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define ISUPPER(c) ((c) >= 'A' && (c) <= 'Z')
#define ISLOWER(c) ((c) >= 'a' && (c) <= 'z')
#define ISALNUM(c) (ISDIGIT(c) || ISUPPER(c) || ISLOWER(c))
#define ISDIRECTIVE(c) ISLOWER(c)
#define ISPATH(c) (ISALNUM(c) || (c) == '.' || (c) == '-' || (c) == '_')

struct config_parser
{
  struct config_file *file;
  unsigned int lineno;
  const char *errmsg;
  const char *cur;
  const char *end;
};

#define s p->cur

static int config_parse_path(struct config_parser *p, char **pathp)
{
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
  // ~/PATH

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
  if (*s == '~') {
    if (*(++s) != '/')
      return -1;
    ++s;
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
    }
    ++s;
  }
  return 0;
}

static int config_parse(struct config_file *file, const char *buf, size_t size)
{
  struct config_parser p;
  p.file = file;
  p.lineno = 0;
  p.errmsg = "invalid syntax";
  p.cur = buf;
  p.end = buf + size;

  if (config_parse_main(&p)) {
    ERR("%s: line %u: %s", file->path, p.lineno, p.errmsg);
    return -1;
  }

  if (!file->head)
    WARN("%s: no directives found", file->path);

  return 0;
}

#undef s

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

  // pad with an invalid character
  buf[size++] = '\0';

  ret = config_parse(file, buf, size);

finish:
  if (fd >= 0)
    close(fd);
  return ret;
}

static int config_load_dir(struct config_loader *loader, struct config_file ***slotp)
{
  DEBUG("%s: %s", __func__, loader->path);

  struct config_file *end = **slotp;

  struct dirent *dent;
  DIR *d = opendir(loader->path);
  if (!d) {
    ERR_SYS("failed to open '%s'", loader->path);
    return -1;
  }

  size_t pathlen = loader->pathlen;
  loader->path[pathlen] = '/';

  while (errno = 0, dent = readdir(d)) {
    const char *name = dent->d_name;

    // ignore files that begin with a dot
    if (*name == '.')
      continue;

    int type = dent->d_type;

    // skip entries that are not files or directories
    switch (type) {
      case DT_UNKNOWN:
      case DT_REG:
      case DT_DIR:
        break;
      default:
        continue;
    }

    // construct pathname
    size_t namelen = strlen(name);
    loader->pathlen = pathlen + 1 + namelen;
    config_load_alloc_path(loader);
    memcpy(loader->path + pathlen + 1, name, namelen + 1);

    if (type == DT_UNKNOWN) {
      // determine type using lstat()
      struct stat st;
      if (lstat(loader->path, &st)) {
        ERR_SYS("failed to lstat '%s'", loader->path);
        return -1;
      }
      switch (st.st_mode & S_IFMT) {
        case S_IFREG: type = DT_REG; break;
        case S_IFDIR: type = DT_DIR; break;
        default: continue;
      }
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
      config_load_dir(loader, slotp);
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
  int ret = config_load_dir(&loader, &slot);
  free(loader.path);
  return ret;
}

/*** LOG / PARSER ****************************************************** {{{1 */

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
      ERR("log: path too long at line %u: '%.*s'", lineno, (int)len, begin);
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

    struct stat st;
    if (lstat(path, &st)) {
      if (errno == ENOENT)
        // no longer exists
        goto next;
      ERR_SYS("failed to lstat '%s'", path);
      return -1;
    }
    if (!S_ISLNK(st.st_mode)) {
      WARN("refusing to remove non-symlink '%s'", path);
      goto next;
    }
    MSG("Removing: %s", path);
    if (state->flags & STATE_DRY)
      goto next;
    if (unlink(path)) {
      ERR_SYS("failed to unlink '%s'", path);
      return -1;
    }
    goto next;

invalid_path:
    while (*s != '\n')
      ++s;
    ERR("log: invalid path at line %u: '%.*s'", lineno, (int)(s - begin), begin);
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

/*** RECOMPILE ********************************************************* {{{1 */

static void recompile(void)
{
  static char *const srcfile = ".gale/gallade.c";
  static char *const tmpfile = ".local/bin/gallade.tmp";
  static char *const exefile = ".local/bin/gallade";
  static char *const gcc_args[] = {
    "gcc", "-Wall", "-std=c99", "-g", "-o", tmpfile, srcfile, (char*)NULL
  };

  struct stat st;
  long long srctime, exetime;
  if (stat(srcfile, &st)) {
    ERR_SYS("failed to stat '%s'", srcfile);
    exit(1);
  }
  srctime = st.st_mtime;
  if (stat(exefile, &st)) {
    if (errno != ENOENT) {
      ERR_SYS("failed to stat '%s'", exefile);
      exit(1);
    }
    exetime = srctime - 1;
  } else {
    exetime = st.st_mtime;
  }
  if (srctime <= exetime)
    exit(0);

  MSG("Recompiling...");

  pid_t pid = fork();
  if (pid < 0) {
    ERR_SYS("fork");
    exit(1);
  }
  if (pid == 0) {
    execvp("gcc", gcc_args);
    ERR_SYS("execvp");
    exit(-1);
  }
  int wstatus;
  if (waitpid(pid, &wstatus, 0) < 0) {
    ERR_SYS("waitpid");
    exit(1);
  }
  if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
    exit(1);
  if (rename(tmpfile, exefile)) {
    ERR_SYS("failed to rename '%s' to '%s'", tmpfile, exefile);
    exit(1);
  }
  exit(0);
}

/*** OPTION PARSER ***************************************************** {{{1 */

// Screw getopt! This is a lot more painful to read and a lot more fun to use.

#define OPTBEGIN(argv) { \
  char **_argv = (argv), *_arg, _s; \
  while ((_arg = *_argv)) { \
    if (_arg[0] != '-' || _arg[1] == 0) \
      DIE(2, "invalid argument: '%s'", _arg); \
    if (*(++_arg) == '-') { \
      _s = 0; \
      ++_arg; \
      ++_argv; \
    } else { \
      _s = *_arg; \
      *(_arg++) = '-'; \
      if (*_arg) ++(*_argv); \
      else ++_argv; \
    } \
    if (0)

#define OPTEND \
  else if (_s) DIE(2, "invalid option: -%c", _s); \
  else DIE(2, "invalid option: --%s", _arg); }}

#define OPT(s, l) \
  else if (_s ? _s == s : !strcmp(_arg, l))

#define OPTLONG(l) \
  if (!_s && !strcmp(_arg, l))

/*** MAIN ************************************************************** {{{1 */

static const char usage[] = "usage: gallade [options]"
#ifdef GALLADE_DEBUG
" (debug build)"
#endif
"\n\nOptions:\n\
  -h, --help        show this help and exit\n\
  -n, --dry-run     don't do anything, only show what would happen\n\
  -v, --verbose     be verbose\n\
";

int main(int argc, char **argv)
{
  if (isatty(2))
    enable_colors();

  unsigned int flags = 0;

  OPTBEGIN(argv + 1);
  OPT('h', "help") {
    fputs(usage, stdout);
    return 0;
  }
  OPT('n', "dry-run") {
    flags |= STATE_DRY;
  }
  OPT('v', "verbose") {
    flags |= STATE_VERBOSE;
  }
  OPTEND;

  const char *home = getenv("HOME");
  if (!home) {
    ERR("$HOME is not set");
    return 1;
  }
  if (chdir(home)) {
    ERR_SYS("could not chdir to '%s'", home);
    return 1;
  }

  //- Putting it all together: -------------------------------------------------

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

  // 3. Load the log against the config. ---------------------------------------

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

  // maybe clean up one day
  return 0;
}

/*********************************************************************** }}}1 */

// vim:fo+=n:fdm=marker
