/*
 * gallade - The Gale Installer
 *
 * Copyright (c) 2025 Daniel Li
 *
 * This software is licensed under the MIT License.
 */

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define containerof(p, type, member) \
  ((type *)((char *)(p) - offsetof(type, member)))

/*** LOGGING *********************************************************** {{{1 */

#ifdef __GNUC__
# define UNUSED __attribute__((unused))
# define PRINTF(i, j) __attribute__((format(printf, i, j)))
#else
# define UNUSED
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
#ifndef NDEBUG
# define DEBUG(...) msg(prefix_debug, __VA_ARGS__)
#else
# define DEBUG(...) (void)0
#endif

UNUSED PRINTF(3, 4)
static void indentf(FILE *f, unsigned int indent, const char *format, ...)
{
  va_list ap;
  for (; indent; --indent)
    putc(' ', f);
  va_start(ap, format);
  vfprintf(f, format, ap);
  va_end(ap);
  putc('\n', f);
}

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

/*** AVL *************************************************************** {{{1 */

struct avl
{
  const char *key;
  struct avl *parent;
  struct avl *left;
  struct avl *right;
  int bf;
};

/*
 * Returns the minimum element of the tree rooted at the specified node.
 */
static struct avl *avl_min(struct avl *node)
{
  if (!node)
    return NULL;
  while (node->left)
    node = node->left;
  return node;
}

/*
 * Returns the inorder successor of the specified node.
 */
static struct avl *avl_next(struct avl *node)
{
  if (node->right)
    return avl_min(node->right);
  while (node->parent) {
    if (node->parent->left == node) {
      // coming in from left
      return node->parent;
    }
    node = node->parent;
  }
  return NULL;
}

/*
 * Returns the slot containing the node with the specified key,
 * or the slot where a new node would be inserted if the key is not found.
 * In any case, this never returns NULL.
 * If `pp` is not NULL, it will be set to the parent.
 */
static struct avl **avl_slot(struct avl **root, const char *key, struct avl **pp)
{
  struct avl *node;
  if (pp)
    *pp = NULL;
  while ((node = *root)) {
    int diff = strcmp(key, node->key);
    if (!diff)
      return root;
    if (pp)
      *pp = node;
    if (diff < 0)
      root = &node->left;
    else
      root = &node->right;
  }
  return root;
}

/*
 * Inserts a node into the tree. `node->key` must be set by the caller.
 * Returns `node` if it is successfully inserted or NULL if a node with this
 * key already exists.
 */
static struct avl *avl_insert(struct avl **root, struct avl *node)
{
  struct avl *parent;
  struct avl **slot = avl_slot(root, node->key, &parent);
  if (*slot)
    return NULL;
  node->parent = parent;
  node->left = NULL;
  node->right = NULL;
  node->bf = 0;
  // TODO: rebalance
  *slot = node;
  return node;
}

/*** CF TYPES ********************************************************** {{{1 */

enum cf_command_type
{
  CF_LINK
};

union cf_command;

struct cf_block
{
  union cf_command **cmds;
  unsigned int cmdcnt;
  unsigned int cmdcap;
};

struct cf_command_header
{
  enum cf_command_type type;
  unsigned int lineno;
};

struct cf_command_link
{
  struct cf_command_header h;
  char *path;
};

union cf_command
{
  struct cf_command_header h;
  struct cf_command_link link;
};

struct cf
{
  struct cf_block block;
};

/*** CF UTILITIES ****************************************************** {{{1 */

static void cf_init_block(struct cf_block *block)
{
  block->cmds = NULL;
  block->cmdcnt = 0;
  block->cmdcap = 0;
}

static void cf_free_block(struct cf_block *block)
{
  while (block->cmdcnt) {
    union cf_command *cmd = block->cmds[--block->cmdcnt];
    switch (cmd->h.type) {
      case CF_LINK:
        free(cmd->link.path);
        break;
    }
  }
  free(block->cmds);
}

static void cf_add_command(struct cf_block *block, union cf_command *cmd)
{
  if (block->cmdcnt >= block->cmdcap) {
    block->cmdcap = block->cmdcap ? block->cmdcap * 2 : 4;
    xresizev(block->cmds, block->cmdcap, union cf_command*);
  }
  block->cmds[block->cmdcnt++] = cmd;
}

static struct cf *cf_new()
{
  xdefine(cf, struct cf);
  cf_init_block(&cf->block);
  return cf;
}

static void cf_destroy(struct cf *cf)
{
  cf_free_block(&cf->block);
  free(cf);
}

/*** CF PARSER ********************************************************* {{{1 */

#define ISSPACE(c) ((c) == ' ')
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define ISUPPER(c) ((c) >= 'A' && (c) <= 'Z')
#define ISLOWER(c) ((c) >= 'a' && (c) <= 'z')
#define ISALNUM(c) (ISDIGIT(c) || ISUPPER(c) || ISLOWER(c))
#define ISDIRECTIVE(c) ISLOWER(c)
#define ISPATH(c) (ISALNUM(c) || (c) == '.' || (c) == '-' || (c) == '_')

struct cf_parser
{
  struct cf *cf;
  unsigned int lineno;
  struct cf_block *block;
  const char *errmsg;
  const char *cur;
  const char *end;
};

#define s ps->cur

static int cf_parse_path(struct cf_parser *ps, char **pathp)
{
  if (*s != '~' || *(++s) != '/')
    return -1;
  const char *begin = ++s;
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

static int cf_parse_space(struct cf_parser *ps)
{
  if (!ISSPACE(*s)) return -1;
  do ++s;
  while (ISSPACE(*s));
  return 0;
}

static int cf_parse_eol(struct cf_parser *ps)
{
  while (ISSPACE(*s))
    ++s;
  if (*s == '\n')
    goto newline;
  if (*s == '#') {
    do ++s;
    while (*s != '\n');
    goto newline;
  }
  return -1;
newline:
  ++s;
  ++ps->lineno;
  return 0;
}

static int cf_parse_link(struct cf_parser *ps)
{
  // gale::link <path> [if <cond>]
  // TODO: handle if expression

  unsigned int lineno = ps->lineno;
  char *path = NULL;
  if (cf_parse_space(ps)) goto fail;
  if (cf_parse_path(ps, &path)) goto fail;
  if (cf_parse_eol(ps)) goto fail;

  xdefine(cmd, struct cf_command_link);
  cmd->h.type = CF_LINK;
  cmd->h.lineno = lineno;
  cmd->path = path;
  cf_add_command(ps->block, (union cf_command *)cmd);
  return 0;

fail:
  free(path);
  return -1;
}

static int cf_parse_command(struct cf_parser *ps)
{
  if (!ISLOWER(*s))
    return -1;
  const char *name = s;
  do ++s;
  while (ISLOWER(*s));
  size_t len = s - name;

  if (len == 4 && !memcmp(name, "link", 4))
    return cf_parse_link(ps);

  ps->errmsg = "invalid command";
  return -1;
}

static int cf_parse_main(struct cf_parser *ps)
{
begin:
  ++ps->lineno;
  if (s >= ps->end)
    return 0;

mid:
  if (*s == '\n') {
    ++s;
    goto begin;
  }

  // find command string
  if (*(s++) != 'g') goto mid;
  if (*s     != 'a') goto mid;
  if (*(++s) != 'l') goto mid;
  if (*(++s) != 'e') goto mid;
  if (*(++s) != ':') goto mid;
  if (*(++s) != ':') goto mid;
  ++s;

  if (cf_parse_command(ps))
    return -1;
  goto begin;
}

static void cf_parse_init(struct cf_parser *ps)
{
  ps->cf = cf_new();
  ps->lineno = 0;
  ps->block = &ps->cf->block;
  ps->errmsg = "syntax error";
}

static void cf_parse_abort(struct cf_parser *ps)
{
  cf_destroy(ps->cf);
}

static int cf_parse(struct cf_parser *ps, const char *buf, size_t len)
{
  ps->cur = buf;
  ps->end = buf + len;

  if (cf_parse_main(ps)) {
    ERR("line %u: %s", ps->lineno, ps->errmsg);
    return -1;
  }
  return 0;
}

static struct cf *cf_parse_end(struct cf_parser *ps)
{
  return ps->cf;
}

#undef s

/*** CONFIG TYPES ****************************************************** {{{1 */

struct conffile
{
  size_t size;
  unsigned long long mtime;
  struct cf *cf;
};

#define CONFDIR_ROOT 0x1

struct confdir
{
  unsigned long long mtime;
  int flags;
  struct avl *entries;
};

enum confent_type
{
  CONFENT_NONE,
  CONFENT_FILE,
  CONFENT_DIR
};

struct confent
{
  enum confent_type type;
  char *path;
  char *name;
  struct avl node;
  union {
    struct conffile file;
    struct confdir dir;
  };
};

enum action_type {
  ACTION_LINK
};

struct action
{
  enum action_type type;
  char *path;
  struct avl node;
  struct {
    char *target;
  } link;
};

struct config
{
  struct confdir root;
  int real;
  int verbose;
  int bad;
  int need_update_cache;
  struct avl *actions;
  char *buf;
  size_t bufsize;
};

/*** CONFIG UTILITIES ************************************************** {{{1 */

static void conffile_free(struct conffile *file)
{
  if (file->cf)
    cf_destroy(file->cf);
}

static void confdir_init(struct confdir *dir)
{
  dir->mtime = 0;
  dir->flags = 0;
  dir->entries = NULL;
}

static struct confent *confdir_add(struct confdir *dir, const char *name)
{
  xdefine(ent, struct confent);
  ent->node.key = name;
  if (avl_insert(&dir->entries, &ent->node))
    return ent;
  free(ent);
  return NULL;
}

static inline struct confent *confent_from_node(struct avl *node)
{
  return node ? containerof(node, struct confent, node) : NULL;
}

#define CONFDIR_ITER(dir, ent) \
  for (struct confent *ent = confent_from_node(avl_min((dir)->entries)); \
       ent; ent = confent_from_node(avl_next(&ent->node)))

static void confent_free_data(struct confent *ent);

static void confdir_free(struct confdir *dir)
{
  // TODO
}

static void confent_free_data(struct confent *ent)
{
  switch (ent->type) {
    case CONFENT_NONE:
      break;
    case CONFENT_FILE:
      conffile_free(&ent->file);
      break;
    case CONFENT_DIR:
      confdir_free(&ent->dir);
      break;
  }
}

static void config_init(struct config *conf, int real, int verbose)
{
  confdir_init(&conf->root);
  conf->root.flags = CONFDIR_ROOT;
  conf->real = real;
  conf->verbose = verbose;
  conf->need_update_cache = 0;
  conf->bad = 0;
  conf->actions = NULL;
  conf->buf = xmalloc(4096);
  conf->bufsize = 4096;
}

static struct action *config_add_action(struct config *conf, const char *path)
{
  xdefine(act, struct action);
  act->node.key = path;
  if (avl_insert(&conf->actions, &act->node))
    return act;
  free(act);
  return NULL;
}

static inline struct action *action_from_node(struct avl *node)
{
  return node ? containerof(node, struct action, node) : NULL;
}

#define ACTION_ITER(conf, act) \
  for (struct action *act = action_from_node(avl_min((conf)->actions)); \
       act; act = action_from_node(avl_next(&act->node)))

//static void config_free(struct config *conf)
//{
//  confdir_free(&conf->root);
//}

/*** CONFIG LOADER ***************************************************** {{{1 */

static struct cf *load_file(struct config *conf, const char *path, size_t len)
{
  DEBUG("loading file: %s", path);

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    ERR_SYS("failed to open '%s'", path);
    return NULL;
  }

  struct cf *cf = NULL;
  char *buf = xmalloc(len + 1);
  char *p = buf;
  while (p < buf + len) {
    ssize_t readsize = read(fd, p, buf + len - p);
    if (readsize < 0) {
      ERR_SYS("failed to read '%s'", path);
      goto finish;
    }
    if (readsize == 0) {
      ERR("'%s' shrank", path);
      goto finish;
    }
    p += readsize;
  }

  close(fd);
  fd = -1;

  if (buf[len - 1] != '\n')
    buf[len++] = '\n';

  struct cf_parser ps;
  cf_parse_init(&ps);
  if (cf_parse(&ps, buf, len)) {
    cf_parse_abort(&ps);
    goto finish;
  }

  cf = cf_parse_end(&ps);

finish:
  free(buf);
  if (fd >= 0)
    close(fd);
  return cf;
}

static int update_file(struct config *conf, struct conffile *file,
                       size_t size, unsigned long long mtime, int init)
{
  if (init || file->size != size || file->mtime != mtime) {
    file->size = size;
    file->mtime = mtime;
    file->cf = load_file(conf, conf->buf, size);
    if (file->cf)
      conf->need_update_cache = 1;
    else
      // bad files don't trigger a cache write by themselves
      conf->bad = 1;
  }
  return 0;
}

static int load_dir(struct config *conf, struct confdir *dir, const char *path)
{
  DEBUG("config: loading directory: %s", conf->buf);

  // construct the full path
  size_t pathlen;
  if (path) {
    pathlen = strlen(path);
    if (conf->bufsize < pathlen + 7) {
      conf->bufsize = pathlen;
      xresize(conf->buf, conf->bufsize);
    }
    sprintf(conf->buf, ".gale/%s", path);
  } else {
    strcpy(conf->buf, ".gale");
  }

  struct dirent *dent;
  DIR *d = opendir(conf->buf);
  if (!d) {
    ERR_SYS("failed to open '%s'", conf->buf);
    return -1;
  }

  while (errno = 0, dent = readdir(d)) {
    const char *name = dent->d_name;

    // ignore files that begin with a dot
    if (*name == '.')
      continue;

    // skip entries that are definitely not files or directories
    if (dent->d_type != DT_UNKNOWN
        && dent->d_type != DT_DIR
        && (dir->flags & CONFDIR_ROOT || dent->d_type != DT_REG))
      continue;

    struct confent *ent = confdir_add(dir, name);
    if (!ent)
      continue;

    // construct the full path
    size_t namelen = strlen(name);
    if (path) {
      ent->path = xmalloc(pathlen + 1 + namelen + 1);
      sprintf(ent->path, "%s/%s", path, name);
      ent->name = ent->path + pathlen + 1;
    } else {
      ent->path = memcpy(xmalloc(namelen + 1), name, namelen + 1);
      ent->name = ent->path;
    }
    ent->node.key = ent->name;
  }

  if (errno) {
    ERR_SYS("readdir");
    closedir(d);
    return -1;
  }

  closedir(d);
  return 0;
}

static int update_ent(struct config *conf, struct confent *ent);

static int update_dir(struct config *conf, struct confdir *dir,
                      unsigned long long mtime, int init, const char *path)
{
  if (init)
    confdir_init(dir);

  if (init || dir->mtime != mtime) {
    // update directory listing
    dir->mtime = mtime;
    conf->need_update_cache = 1;
    if (load_dir(conf, dir, path))
      return -1;
  }

  int ret = 0;
  CONFDIR_ITER(dir, ent) {
    if (update_ent(conf, ent))
      ret = -1;
  }
  return ret;
}

static int update_ent(struct config *conf, struct confent *ent)
{
  struct stat st;

  size_t pathlen = strlen(ent->path);
  if (conf->bufsize < pathlen + 7) {
    conf->bufsize = pathlen;
    xresize(conf->buf, conf->bufsize);
  }

  sprintf(conf->buf, ".gale/%s", ent->path);

  if (lstat(conf->buf, &st)) {
    ERR_SYS("failed to lstat '%s'", conf->buf);
    return -1;
  }

  if (S_ISREG(st.st_mode)) {
    int init = ent->type != CONFENT_FILE;
    if (init) {
      confent_free_data(ent);
      ent->type = CONFENT_FILE;
    }
    return update_file(conf, &ent->file, st.st_size, st.st_mtime, init);
  }

  if (S_ISDIR(st.st_mode)) {
    int init = ent->type != CONFENT_DIR;
    if (init) {
      confent_free_data(ent);
      ent->type = CONFENT_DIR;
    }
    return update_dir(conf, &ent->dir, st.st_mtime, init, ent->path);
  }

  // the file is some other type that we should ignore
  switch (ent->type) {
    case CONFENT_NONE:
      break;
    case CONFENT_FILE:
      conffile_free(&ent->file);
      break;
    case CONFENT_DIR:
      confdir_free(&ent->dir);
      break;
  }
  ent->type = CONFENT_NONE;
  conf->need_update_cache = 1;
  return 0;
}

static int update_config(struct config *conf)
{
  static const char *const gale_root = ".gale";

  struct stat st;
  if (stat(gale_root, &st)) {
    ERR_SYS("failed to stat '%s'", gale_root);
    return -1;
  }

  return update_dir(conf, &conf->root, st.st_mtime, 0, NULL);
}

/*** CACHE PARSER ****************************************************** {{{1 */

struct cache_parser
{
  struct config *conf;
  const char *cur;
  const char *end;
  unsigned int lineno;
  const char *errmsg;
};

#define s ps->cur

static int cache_parse_eol(struct cache_parser *ps)
{
  // skip trailing space
  while (ISSPACE(*s))
    ++s;
  if (*s == '\n') {
    ++s;
    return 0;
  }
  if (*s == '#') {
    // comment
    do ++s;
    while (*s != '\n');
    ++s;
    return 0;
  }
  return -1;
}

static int cache_parse_space(struct cache_parser *ps)
{
  if (!ISSPACE(*s))
    return -1;
  do ++s;
  while (ISSPACE(*s));
  return 0;
}

static int cache_parse_name(struct cache_parser *ps, char **namep)
{
  const char *begin = s;
  while (*s == '.')
    ++s;
  if (!ISPATH(*s))
    return -1;
  do ++s;
  while (ISPATH(*s));
  size_t len = s - begin;
  char *name = memcpy(xmalloc(len + 1), begin, len);
  name[len] = '\0';
  *namep = name;
  return 0;
}

static int cache_parse_size(struct cache_parser *ps, size_t *sizep)
{
  size_t size = 0;
  if (!ISDIGIT(*s))
    return -1;
  do size = size * 10 + *(s++) - '0';
  while (ISDIGIT(*s));
  *sizep = size;
  return 0;
}

static int cache_parse_mtime(struct cache_parser *ps, unsigned long long *mtimep)
{
  unsigned long long mtime = 0;
  if (!ISDIGIT(*s))
    return -1;
  do mtime = mtime * 10 + *(s++) - '0';
  while (ISDIGIT(*s));
  *mtimep = mtime;
  return 0;
}

static int cache_parse_cf(struct cache_parser *ps, struct cf **cfp)
{
  size_t cf_lineno;
  struct cf_parser cps;
  cf_parse_init(&cps);

next:
  if (cache_parse_size(ps, &cf_lineno)) goto done;
  ++ps->lineno;
  if (cache_parse_space(ps)) goto fail;

  // synchronize parsers
  cps.cur = s;
  cps.end = ps->end;
  cps.lineno = cf_lineno;
  if (cf_parse_command(&cps)) {
    s = cps.cur;
    ps->errmsg = cps.errmsg;
    goto fail;
  }

  s = cps.cur;
  goto next;

done:
  if (!(*cfp = cf_parse_end(&cps)))
    return -1;
  return 0;

fail:
  cf_parse_abort(&cps);
  return -1;
}

static struct confent *cache_prepare_entry(
    struct cache_parser *ps, struct confent *dir,
    enum confent_type type, char *name)
{
  struct confent *ent = confdir_add(dir ? &dir->dir : &ps->conf->root, name);
  if (!ent) {
    ps->errmsg = "duplicate entry";
    free(name);
    return NULL;
  }

  size_t namelen = strlen(name);
  size_t pathlen = namelen;
  char *path = name;
  if (dir) {
    size_t dirlen = strlen(dir->path);
    pathlen = dirlen + 1 + namelen;
    path = xmalloc(pathlen + 1);
    strcpy(path, dir->path);
    path[dirlen] = '/';
    memcpy(path + dirlen + 1, name, namelen + 1);
    free(name);
    name = path + dirlen + 1;
  }
  ent->type = type;
  ent->path = path;
  ent->name = name;
  ent->node.key = name;
  return ent;
}

static int cache_parse_main(struct cache_parser *ps)
{
  int ret = -1;
  char *name = NULL;
  size_t size;
  unsigned long long mtime;
  struct confent *ent;

  struct confent **dirstack = xnewv(8, struct confent*);
  unsigned int dircnt = 0;
  unsigned int dircap = 8;

next:
  ++ps->lineno;
  if (s >= ps->end) {
    ps->errmsg = "unexpected end of file";
    goto fail;
  }
  if (dircnt) switch (*s) {
    case '?': ++s; goto none;
    case 'f': ++s; goto file;
    case 'd': ++s; goto dir;
    case '.': ++s; goto up;
  } else switch (*s) {
    case 'r': ++s; goto root;
  }
  if (cache_parse_eol(ps)) {
    ps->errmsg = "invalid command";
    goto fail;
  }
  goto next;

root:
  dirstack[dircnt++] = NULL;

  if (cache_parse_space(ps)) goto fail;
  if (cache_parse_mtime(ps, &ps->conf->root.mtime)) goto fail;
  if (cache_parse_eol(ps)) goto fail;
  goto next;

none:
  if (cache_parse_space(ps)) goto fail;
  if (cache_parse_name(ps, &name)) goto fail;
  if (cache_parse_eol(ps)) goto fail;

  if (!(ent = cache_prepare_entry(ps, dirstack[dircnt - 1], CONFENT_NONE, name)))
    goto fail;
  name = NULL;
  goto next;

file:
  if (cache_parse_space(ps)) goto fail;
  if (cache_parse_name(ps, &name)) goto fail;
  if (cache_parse_space(ps)) goto fail;
  if (cache_parse_size(ps, &size)) goto fail;
  if (cache_parse_space(ps)) goto fail;
  if (cache_parse_mtime(ps, &mtime)) goto fail;
  if (cache_parse_eol(ps)) goto fail;

  if (!(ent = cache_prepare_entry(ps, dirstack[dircnt - 1], CONFENT_FILE, name)))
    goto fail;
  name = NULL;

  ent->file.size = size;
  ent->file.mtime = mtime;
  ent->file.cf = NULL;

  if (cache_parse_cf(ps, &ent->file.cf))
    goto fail;
  goto next;

dir:
  if (cache_parse_space(ps)) goto fail;
  if (cache_parse_name(ps, &name)) goto fail;
  if (cache_parse_space(ps)) goto fail;
  if (cache_parse_mtime(ps, &mtime)) goto fail;
  if (cache_parse_eol(ps)) goto fail;

  if (!(ent = cache_prepare_entry(ps, dirstack[dircnt - 1], CONFENT_DIR, name)))
    goto fail;
  name = NULL;

  confdir_init(&ent->dir);
  ent->dir.mtime = mtime;

  // push directory
  if (dircnt == dircap)
    xresizev(dirstack, dircap *= 2, struct confent*);
  dirstack[dircnt++] = ent;
  goto next;

up:
  if (cache_parse_eol(ps)) goto fail;

  // pop directory
  if (--dircnt)
    goto next;

  // that was the root
  if (s < ps->end)
    ps->errmsg = "expected end of file";
  else
    ret = 0;

fail:
  free(name);
  free(dirstack);
  return ret;
}

static int cache_parse(struct config *conf, const char *buf, size_t len)
{
  struct cache_parser ps;
  ps.conf = conf;
  ps.cur = buf;
  ps.end = buf + len;
  ps.lineno = 0;
  ps.errmsg = "invalid value";

  if (cache_parse_main(&ps)) {
    ERR("cache: line %u: %s", ps.lineno, ps.errmsg);
    return -1;
  }
  return 0;
}

static int cache_load(struct config *conf, const char *filename)
{
  struct stat st;
  if (stat(filename, &st)) {
    if (errno == ENOENT)
      return 0;
    ERR_SYS("failed to stat '%s'", filename);
    return -1;
  }

  size_t len = st.st_size;
  if (len == 0)
    return 0;

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    ERR_SYS("failed to open '%s'", filename);
    return -1;
  }

  int ret = -1;
  char *buf = xmalloc(len + 1);
  char *p = buf;
  while (p < buf + len) {
    ssize_t readsize = read(fd, p, buf + len - p);
    if (readsize < 0) {
      ERR_SYS("read");
      goto finish;
    }
    if (readsize == 0) {
      ERR("cache file shrank");
      goto finish;
    }
    p += readsize;
  }

  close(fd);
  fd = -1;

  if (buf[len - 1] != '\n')
    buf[len++] = '\n';

  ret = cache_parse(conf, buf, len);

finish:
  free(buf);
  if (fd >= 0)
    close(fd);
  return ret;
}

/*** CACHE WRITER ****************************************************** {{{1 */

static void cache_write_cf(FILE *f, struct cf_block *block)
{
  for (unsigned int i = 0; i < block->cmdcnt; i++) {
    union cf_command *cmd = block->cmds[i];
    fprintf(f, "%u ", cmd->h.lineno);
    switch (cmd->h.type) {
      case CF_LINK:
        fprintf(f, "link ~/%s\n", cmd->link.path);
        break;
    }
  }
}

static void cache_write_dir(FILE *f, struct confdir *dir)
{
  CONFDIR_ITER(dir, ent) {
    switch (ent->type) {
      case CONFENT_NONE:
        // ignore
        break;
      case CONFENT_FILE:
        if (ent->file.cf) {
          fprintf(f, "f %s %zu %lld\n", ent->name, ent->file.size, ent->file.mtime);
          cache_write_cf(f, &ent->file.cf->block);
        } else {
          // this will force update_file to read it again
          fprintf(f, "? %s\n", ent->name);
        }
        break;
      case CONFENT_DIR:
        fprintf(f, "d %s %lld\n", ent->name, ent->dir.mtime);
        cache_write_dir(f, &ent->dir);
        break;
    }
  }
  fprintf(f, ".\n");
}

static void cache_write(FILE *f, struct config *conf)
{
  fprintf(f, "r %lld\n", conf->root.mtime);
  cache_write_dir(f, &conf->root);
}

static int cache_save(struct config *conf, const char *filename)
{
  int ret = -1;
  FILE *f = NULL;

  size_t filelen = strlen(filename);
  char *tmpfile = malloc(filelen + 5);
  sprintf(tmpfile, "%s.tmp", filename);

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

  cache_write(f, conf);
  if (ferror(f)) {
    ERR("write error");
    return -1;
  }

  fclose(f);
  f = NULL;

  if (rename(tmpfile, filename)) {
    ERR_SYS("failed to rename '%s' to '%s'", tmpfile, filename);
    goto finish;
  }

  ret = 0;

finish:
  free(tmpfile);
  if (f)
    fclose(f);
  return ret;
}

#undef s

/*** RESOLVER ********************************************************** {{{1 */

static int resolve_cf_link(struct config *conf, struct confent *ent, union cf_command *cmd)
{
  char *path = cmd->link.path;
  struct action *act = config_add_action(conf, path);
  if (!act) {
    ERR("multiple actions for '%s'", path);
    return -1;
  }
  path = strdup(path);
  act->type = ACTION_LINK;
  act->path = path;
  act->node.key = path;
  act->link.target = ent->path;
  return 0;
}

static int resolve_cf(struct config *conf, struct confent *ent, struct cf_block *block)
{
  for (unsigned int i = 0; i < block->cmdcnt; i++) {
    union cf_command *cmd = block->cmds[i];
    switch (cmd->h.type) {
      case CF_LINK:
        if (resolve_cf_link(conf, ent, cmd)) return -1;
        break;
    }
  }
  return 0;
}

static int resolve_dir(struct config *conf, struct confdir *dir)
{
  int ret = 0;
  CONFDIR_ITER(dir, ent) {
    switch (ent->type) {
      case CONFENT_NONE:
        break;
      case CONFENT_FILE:
        if (resolve_cf(conf, ent, &ent->file.cf->block))
          ret = -1;
        break;
      case CONFENT_DIR:
        if (resolve_dir(conf, &ent->dir))
          ret = -1;
        break;
    }
  }
  return ret;
}

static int resolve(struct config *conf)
{
  return resolve_dir(conf, &conf->root);
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

static int execute_link(struct config *conf, struct action *act)
{
  int dircnt = 0;
  char *s;
  for (s = act->path; *s; ++s)
    if (*s == '/')
      ++dircnt;
  size_t baselen = strlen(act->link.target);
  size_t target_len = dircnt * 3 + 6 + baselen;
  size_t bufsize = 2 * (target_len + 1);

  // get a big enough buffer
  if (conf->bufsize <= bufsize) {
    do conf->bufsize <<= 1;
    while (conf->bufsize <= bufsize);
    xresize(conf->buf, conf->bufsize);
  }

  // generate the target
  char *target = conf->buf;
  char *existing = conf->buf + target_len + 1;
  s = target;
  while (dircnt) {
    memcpy(s, "../", 3);
    s += 3;
    --dircnt;
  }
  memcpy(s, ".gale/", 6);
  s += 6;
  memcpy(s, act->link.target, baselen + 1);

  ssize_t existing_len = readlink(act->path, existing, target_len + 1);
  if (existing_len < 0) {
    if (errno == EINVAL) {
      ERR("refusing to replace '%s'", act->path);
      return -1;
    }
    if (errno != ENOENT) {
      ERR_SYS("failed to readlink '%s'", act->path);
      return -1;
    }
    MSG("Creating symlink: %s -> %s", act->path, target);
  } else if (existing_len == target_len && !memcmp(existing, target, target_len)) {
    if (conf->verbose)
      MSG("Skipping symlink: %s -> %s", act->path, target);
    return 0;
  } else {
    MSG("Replacing symlink: %s -> %s", act->path, target);
    if (conf->real && unlink(act->path)) {
      ERR_SYS("failed to unlink '%s'", act->path);
      return -1;
    }
  }

  if (!conf->real)
    return 0;

  if (!symlink(target, act->path))
    return 0;

  if (errno != ENOENT)
    goto symlink_failed;

  // create parent directories
  if (create_parent_directories(act->path))
    return -1;

  // try again
  if (!symlink(target, act->path))
    return 0;

symlink_failed:
  ERR_SYS("symlink");
  return -1;
}

static int execute(struct config *conf)
{
  int ret = 0;
  ACTION_ITER(conf, act) {
    switch (act->type) {
      case ACTION_LINK:
        ret |= execute_link(conf, act);
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

/*** GALEOPT *********************************************************** {{{1 */

// Screw getopt! This is a lot more painful to read and a lot more fun to use.

#define GALEOPT(argv) \
  for (char **_argv = (argv), *_arg, _s, _n; \
    (_arg = *_argv) && (( \
      _arg[0] != '-' || _arg[1] == '\0' \
      ? DIE(2, "invalid argument: '%s'", _arg) \
      : *(++_arg) == '-' \
      ? (_s = 0, ++_arg, ++_argv, 1) \
      : (_s = *_arg, *(_arg++) = '-', *_arg ? (++(*_argv), 1) : (++_argv, 1)) \
    ), _n = 1); \
    _n && (_s ? (DIE(2, "invalid option: -%c", _s), 1) \
               : DIE(2, "invalid option: --%s", _arg), 1))

#define OPT(s, l, code) \
  if (_s ? _s == s : !strcmp(_arg, l)) { code; _n = 0; continue; }

#define OPTLONG(l, code) \
  if (!_s && !strcmp(_arg, l)) { code; _n = 0; continue; }

/*** MAIN ************************************************************** {{{1 */

static void help(FILE *f, int ret)
{
  static const char usage[] = "\
usage: gallade [options]\n\
\n\
Options:\n\
  -h, --help        show this help and exit\n\
  -n, --dry-run     don't do anything, only show what would happen\n\
  -v, --verbose     be verbose\n\
  -R, --recompile   recompile gallade\n\
      --no-cache    don't read the cache\n\
";
  fputs(usage, f);
  exit(ret);
}

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

  int real = 1;
  int verbose = 0;
  int use_cache = 1;

  GALEOPT(argv + 1) {
    OPT('h', "help", help(stdout, 0));
    OPT('n', "dry-run", real = 0);
    OPT('v', "verbose", verbose = 1);
    OPT('R', "recompile", recompile());
    OPTLONG( "no-cache", use_cache = 0);
  }

  // General Architecture
  // --------------------

  struct config conf;
  config_init(&conf, real, verbose);

  //
  // 1. Load the cache.
  //
  if (use_cache && cache_load(&conf, ".gale.cache"))
    return 1;

  //
  // 2. Load the config. The cache is used to skip reading files and
  //    directories that haven't changed based on their stat() information.
  //
  if (update_config(&conf))
    return 1;

  //
  // 3. If anything changed, write the updated cache.
  //
  if (conf.need_update_cache) {
    MSG("Updating cache...");
    if (cache_save(&conf, ".gale.cache"))
      return 1;
  }

  //
  // 4. Quit if the config files contain syntax errors. The cache is still
  //    updated with the files that were parsed correctly.
  //
  if (conf.bad)
    return 1;

  //
  // 5. Resolve the config.
  //
  if (resolve(&conf))
    return 1;

  //
  // 6. Delete files that are no longer produced by any config file.
  //
  //if (prune(&conf))
  //  return 1;

  //
  // 7. Write the updated log if any files were added or removed.
  //
  //if (log_write(&conf))
  //  return 1;

  //
  // 8. Update the home directory.
  //
  if (execute(&conf))
    return 1;

  return 0;
}

/*********************************************************************** }}}1 */

// vim:fo+=n:fdm=marker
