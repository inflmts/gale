/*
 * galinst - The Gale Installer
 *
 * Copyright (c) 2025 Daniel Li
 *
 * This software is licensed under the MIT License.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
static const char *prefix_creating_symlink = "creating symlink: ";

static void enable_colors(void)
{
  prefix_warn = "\033[1;33mwarning:\033[0m ";
  prefix_err = "\033[1;31merror:\033[0m ";
  prefix_debug = "\033[1;36mdebug:\033[0m ";
  prefix_creating_symlink = "\033[1;32mcreating symlink:\033[0m ";
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
#define xresize(p, n, type) (p = (type*)xrealloc(p, (n) * sizeof(type)))

/*** AVL *************************************************************** {{{1 */

// Simple AVL tree implementation

struct avl_data
{
  const char *key;
  void *left;
  void *right;
  int bf;
};

struct avl
{
  void *head;
  ptrdiff_t offset;
  void **stack;
  unsigned int stksize;
  unsigned int stkcap;
};

static void avl_init_data(struct avl_data *ad, const char *key)
{
  ad->key = key;
  ad->left = NULL;
  ad->right = NULL;
  ad->bf = 0;
}

static void avl_init_impl(struct avl *avl, ptrdiff_t offset)
{
  avl->head = NULL;
  avl->offset = offset;
  avl->stack = xnewv(32, void*);
  avl->stksize = 0;
  avl->stkcap = 32;
}

#define avl_init(avl, type, ad) avl_init_impl(avl, offsetof(type, ad))

#define AVL_DATA(avl, node) ((struct avl_data*)((char*)(node) + (avl)->offset))
#define AVL_KEY(avl, node) AVL_DATA(avl, node)->key
#define AVL_LEFT(avl, node) AVL_DATA(avl, node)->left
#define AVL_RIGHT(avl, node) AVL_DATA(avl, node)->right
#define AVL_BF(avl, node) AVL_DATA(avl, node)->bf
#define AVL_STKPUSH(avl, node) do { \
  if ((avl)->stksize == (avl)->stkcap) \
    xresize((avl)->stack, (avl)->stkcap *= 2, void*); \
  (avl)->stack[(avl)->stksize++] = node; } while (0)
#define AVL_STKPOP(avl) avl->stack[--avl->stksize]

static void *avl_iter_helper(struct avl *avl, void *node)
{
  // https://en.wikipedia.org/wiki/Tree_traversal#In-order_implementation
  while (node) {
    AVL_STKPUSH(avl, node);
        fflush(stderr);
    node = AVL_LEFT(avl, node);
  }
  if (avl->stksize)
    return AVL_STKPOP(avl);
  return NULL;
}

#define AVL_ITER(avl, type, node) \
  for (type *node = ((avl)->stksize = 0, (avl)->head); \
       (node = avl_iter_helper(avl, node)); \
       node = AVL_RIGHT(avl, node))

// TODO: post-order traversal
#define AVL_DESTROY(avl, type, node) if (0)

//static void *avl_get(struct avl *avl, const char *key)
//{
//  void *node = avl->head;
//  while (node) {
//    int diff = strcmp(key, AVL_KEY(avl, node));
//    if (diff == 0)
//      return node;
//    if (diff < 0)
//      node = AVL_LEFT(avl, node);
//    else
//      node = AVL_RIGHT(avl, node);
//  }
//  return NULL;
//}

static void **avl_insert(struct avl *avl, const char *key)
{
  // https://en.wikipedia.org/wiki/AVL_tree#Insert
  void **slot = &avl->head;
  avl->stack[0] = slot;
  avl->stksize = 1;
  while (*slot) {
    int diff = strcmp(key, AVL_KEY(avl, *slot));
    if (diff == 0)
      return slot;
    if (diff < 0)
      slot = &AVL_LEFT(avl, *slot);
    else
      slot = &AVL_RIGHT(avl, *slot);
    AVL_STKPUSH(avl, slot);
  }
  // TODO: rebalance
  return slot;
}

//static struct node *avl_remove(struct avl *d, const char *key);

/*** TEMPLATE (unused) ************************************************* {{{1 */

///*-- template_expr --*/
//
//#define TEMPLATE_EXPR_FLAG 0x0
//#define TEMPLATE_EXPR_AND 0x1
//#define TEMPLATE_EXPR_OR 0x2
//#define TEMPLATE_EXPR_NOT 0x100
//
//struct template_expr
//{
//  int type;
//  const char *flag;
//  struct template_expr **operands;
//  size_t opcnt;
//};
//
//#define IS_IFTOKEN(c) ((c) == '-' || ((c) >= 'a' && (c) <= 'z'))
//
//static struct template_expr *template_new_expr(void)
//{
//  struct template_expr *expr = xnew(struct template_expr);
//  expr->type = type;
//  expr->operands = NULL;
//  expr->opcnt = 0;
//}
//
//static void template_destroy_expr(struct template_expr *expr)
//{
//  while (expr->opcnt)
//    template_destroy_expr(&expr->operands[--expr->opcnt]);
//  free(expr->operands);
//  free(expr);
//}
//
///*-- template_body --*/
//
//struct template_block
//{
//  int type;
//};
//
//struct template_body
//{
//  struct template_block **blocks;
//  size_t bcnt;
//  size_t bcap;
//};
//
//static void template_init_body(struct template_body *body)
//{
//  tmpl->blocks = xnew(struct template_block *);
//  tmpl->bcnt = 0;
//  tmpl->bcap = 1;
//}
//
//static void template_free_body(struct template_body *body)
//{
//  while (tmpl->bcnt)
//    template_destroy_block(&tmpl->blocks[--tmpl->bcnt]);
//  free(tmpl->blocks);
//}
//
//static void template_dump_body(struct template_body *body, FILE *stream, int indent)
//{
//  for (size_t i = 0; i < body->bcnt; i++)
//    template_dump_block(body->blocks[i], stream, indent);
//}
//
//static void template_add_block(struct template_body *body, struct template_block *block)
//{
//  if (body->bcnt >= body->bcap) {
//    body->bcap = (tpl->bcap + 1) * 2;
//    xresizei(body->blocks, body->bcap, struct template_block *);
//  }
//  body->blocks[body->bcnt++] = block;
//}
//
///*-- template_block --*/
//
//enum {
//  TEMPLATE_BLOCK_LITERAL,
//  TEMPLATE_BLOCK_IF,
//  TEMPLATE_BLOCK_ELIF,
//  TEMPLATE_BLOCK_ELSE
//};
//
//struct template_literal_block
//{
//  int type;
//  size_t begin;
//  size_t end;
//};
//
//struct template_if_block
//{
//  int type;
//  struct template_expr expr;
//  struct template_body body;
//};
//
//union template_block
//{
//  int type;
//  struct template_literal_block literal;
//  struct template_if_block cond;
//};
//
//static void template_destroy_literal_block(struct template_literal_block *block)
//{
//  free(block);
//}
//
//static void template_destroy_if_block(struct template_if_block *block)
//{
//  template_free_expr(&block->expr);
//  template_free_body(&block->body);
//  free(block);
//}
//
//static void template_destroy_block(struct template_block *block)
//{
//  switch (block->type) {
//    case TEMPLATE_BLOCK_LITERAL:
//      template_destroy_literal_block(block);
//      break;
//    case TEMPLATE_BLOCK_IF:
//    case TEMPLATE_BLOCK_ELIF:
//    case TEMPLATE_BLOCK_ELSE:
//      template_destroy_if_block(block);
//      break;
//  }
//}
//
//static void template_dump_block(union template_block *block, FILE *f, int indent)
//{
//  findent(stream, indent);
//  switch (block->type) {
//    case TEMPLATE_BLOCK_LITERAL:
//      fprintf(stream, "literal, begin=%zu, end=%zu\n",
//        block->literal.begin, block->literal.end);
//      break;
//    case TEMPLATE_BLOCK_IF:
//    case TEMPLATE_BLOCK_ELIF:
//    case TEMPLATE_BLOCK_ELSE:
//      fprintf(stream, "if:");
//      template_dump_body(&block->cond.body, stream, indent + 2);
//      break;
//}
//
///*-- template --*/
//
//struct template
//{
//  char *install_path;
//  struct template_body body;
//};
//
//static struct template *template_new()
//{
//  struct template *tmpl = xnew(struct template);
//  tmpl->install_path = NULL;
//  template_init_body(&tmpl->body);
//  return tmpl;
//}
//
//static void template_destroy(struct template *tmpl)
//{
//  free(tmpl->install_path);
//  template_free_body(&tmpl->body);
//  free(tmpl);
//}
//
///*-- template_parse --*/
//
//static char *template_parse_expr(char *s, struct template_expr *expr)
//{
//  char *token = NULL;
//
//begin:
//  if (*s == ' ') {
//    ++s;
//    goto begin;
//  }
//
//  if (!IS_IFTOKEN(*s))
//    goto fail;
//
//  char *t = s++;
//  while (IS_IFTOKEN(*s)) ++s;
//  free(token);
//  token = memcpy(xalloc(s - t + 1), t, s - t);
//  token[s - t] = '\0';
//
//  if (!strcmp(token, "not")) {
//    if (!s = template_parse_expr(s, expr))
//      goto fail;
//    expr->type ^= TEMPLATE_EXPR_NOT;
//    goto success;
//  }
//
//  if (!strcmp(token, "and")) {
//    expr->type = TEMPLATE_EXPR_AND;
//    expr->operands = xnew(struct template_expr);
//    expr->opcnt = 1;
//  }
//
//success:
//  free(token);
//  return s;
//
//fail:
//  free(token);
//  return NULL;
//}

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
    xresize(block->cmds, block->cmdcap, union cf_command*);
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

static int cf_parse_path(struct cf_parser *ps, const char **path, size_t *len)
{
  *path = s;
  do {
    while (*s == '.')
      ++s;
    if (!ISPATH(*s))
      return -1;
    do ++s;
    while (ISPATH(*s));
  } while (*s == '/');
  *len = s - *path;
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

static int cf_parse_link_nospace(struct cf_parser *ps)
{
  // gale::link <path> [if <cond>]
  // TODO: handle if expression

  const char *path;
  size_t pathlen;
  if (cf_parse_path(ps, &path, &pathlen)) return -1;
  if (cf_parse_eol(ps)) return -1;

  char *pathcopy = memcpy(xmalloc(pathlen + 1), path, pathlen);
  pathcopy[pathlen] = '\0';
  xdefine(cmd, struct cf_command_link);
  cmd->h.type = CF_LINK;
  cmd->h.lineno = ps->lineno;
  cmd->path = pathcopy;
  cf_add_command(ps->block, (union cf_command *)cmd);
  return 0;
}

static int cf_parse_link(struct cf_parser *ps)
{
  return cf_parse_space(ps) || cf_parse_link_nospace(ps);
}

static int cf_parse_command(struct cf_parser *ps)
{
  if (*s == '~') {
    if (*(++s) != '/')
      return -1;
    ++s;
    return cf_parse_link_nospace(ps);
  }

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
  long long mtime;
  struct cf *cf;
};

#define CONFDIR_ROOT 0x1

struct confdir
{
  long long mtime;
  int flags;
  struct avl entries;
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
  char *name;
  struct avl_data ad;
  union {
    struct conffile file;
    struct confdir dir;
  };
};

struct config
{
  struct confdir root;
  int bad;
  int need_update_cache;
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
  avl_init(&dir->entries, struct confent, ad);
}

static void confdir_free(struct confdir *dir)
{
  AVL_ITER(&dir->entries, struct confent, ent) {
    free(ent->name);
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
}

static void config_init(struct config *conf)
{
  confdir_init(&conf->root);
  conf->root.flags = CONFDIR_ROOT;
  conf->need_update_cache = 0;
  conf->bad = 0;
}

//static void config_free(struct config *conf)
//{
//  confdir_free(&conf->root);
//}

/*** CONFIG LOADER ***************************************************** {{{1 */

struct load_state
{
  struct config *conf;
  char *path;
  char *pathbase;
  size_t pathlen;
  size_t pathcap;
};

static struct cf *load_file(const char *path, size_t len)
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

static int update_file(struct load_state *ls, struct conffile *file,
                       struct stat *st, enum confent_type old_type)
{
  if (old_type == CONFENT_FILE) {
    if (file->size == st->st_size && file->mtime == st->st_mtime) {
      // nothing changed, the data in cache is correct
      // ...although if the file is bad still cause the install to abort
      if (!file->cf)
        ls->conf->bad = 1;
      return 0;
    }
    if (file->cf)
      cf_destroy(file->cf);
  }

  file->size = st->st_size;
  file->mtime = st->st_mtime;
  file->cf = load_file(ls->path, st->st_size);
  if (file->cf || old_type != CONFENT_NONE)
    ls->conf->need_update_cache = 1;
  if (!file->cf)
    ls->conf->bad = 1;
  return 0;
}

static int load_dir(struct confdir *dir, const char *path)
{
  DEBUG("reading directory: %s", path);

  struct dirent *dent;
  DIR *d = opendir(path);
  if (!d) {
    ERR_SYS("failed to open '%s'", path);
    return -1;
  }

  while (errno = 0, dent = readdir(d)) {
    const char *name = dent->d_name;

    // ignore files that begin with a dot
    if (*name == '.')
      continue;

    // skip entries that are not files or directories
    if (dent->d_type != DT_UNKNOWN
        && dent->d_type != DT_DIR
        && (dir->flags & CONFDIR_ROOT || dent->d_type != DT_REG))
      continue;

    struct confent **slot = (struct confent**)avl_insert(&dir->entries, name);
    if (*slot)
      continue; // shouldn't happen

    xdefine(ent, struct confent);
    ent->type = CONFENT_NONE;
    ent->name = strdup(name);
    avl_init_data(&ent->ad, ent->name);
    *slot = ent;
  }

  if (errno) {
    ERR_SYS("readdir");
    closedir(d);
    return -1;
  }

  closedir(d);
  return 0;
}

static int update_dir(struct load_state *ls, struct confdir *dir,
                      struct stat *st, enum confent_type old_type)
{
  if (old_type != CONFENT_DIR || dir->mtime != st->st_mtime) {
    // update directory listing
    dir->mtime = st->st_mtime;
    ls->conf->need_update_cache = 1;
    if (load_dir(dir, ls->path))
      return -1;
  }

  int ret = 0;
  size_t pathlen = ls->pathlen;
  ls->path[pathlen] = '/';

  AVL_ITER(&dir->entries, struct confent, ent) {
    size_t namelen = strlen(ent->name);
    ls->pathlen = pathlen + 1 + namelen;
    if (ls->pathlen >= ls->pathcap)
      xresize(ls->path, (ls->pathcap *= 2), char);
    memcpy(ls->path + pathlen + 1, ent->name, namelen + 1);

    struct stat st;
    if (stat(ls->path, &st)) {
      ERR_SYS("failed to stat '%s'", ls->path);
      ret = -1;
      continue;
    }

    if (S_ISREG(st.st_mode)) {
      switch (ent->type) {
        case CONFENT_NONE:
          break;
        case CONFENT_FILE:
          break;
        case CONFENT_DIR:
          confdir_free(&ent->dir);
          break;
      }

      if (update_file(ls, &ent->file, &st, ent->type))
        ret = -1;
      ent->type = CONFENT_FILE;
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
      switch (ent->type) {
        case CONFENT_NONE:
          confdir_init(&ent->dir);
          break;
        case CONFENT_FILE:
          conffile_free(&ent->file);
          confdir_init(&ent->dir);
          break;
        case CONFENT_DIR:
          break;
      }

      if (update_dir(ls, &ent->dir, &st, ent->type))
        ret = -1;
      ent->type = CONFENT_DIR;
      continue;
    }

    // the file is some other type that we should ignore
    switch (ent->type) {
      case CONFENT_NONE:
        break;
      case CONFENT_FILE:
        conffile_free(&ent->file);
        break;
      case CONFENT_DIR:
        confdir_free(&ent->file);
        break;
    }
    ent->type = CONFENT_NONE;
  }

  ls->path[pathlen] = '\0';
  ls->pathlen = pathlen;
  return ret;
}

static int update_config(struct config *conf)
{
  struct stat st;
  if (stat(".gale", &st)) {
    ERR_SYS("failed to stat '%s'", ".gale");
    return -1;
  }

  struct load_state ls;
  ls.conf = conf;
  ls.path = memcpy(xmalloc(256), ".gale", 5+1);
  ls.pathbase = ls.path + 5+1;
  ls.pathlen = 5;
  ls.pathcap = 256;

  int ret = update_dir(&ls, &conf->root, &st, 1);
  free(ls.path);
  return ret;
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

static int cache_parse_mtime(struct cache_parser *ps, long long *mtimep)
{
  int neg = *s == '-';
  long long mtime = 0;
  if (neg)
    ++s;
  if (!ISDIGIT(*s))
    return -1;
  do mtime = mtime * 10 + *(s++) - '0';
  while (ISDIGIT(*s));
  if (neg)
    mtime = -mtime;
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

static int cache_parse_main(struct cache_parser *ps)
{
  int ret = -1;
  char *name = NULL;
  size_t size;
  long long mtime;
  struct confent **slot, *ent;

  struct confdir **dirstack = xnewv(8, struct confdir*);
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
  dirstack[dircnt++] = &ps->conf->root;

  if (cache_parse_space(ps)) goto fail;
  if (cache_parse_mtime(ps, &ps->conf->root.mtime)) goto fail;
  if (cache_parse_eol(ps)) goto fail;
  goto next;

none:
  if (cache_parse_space(ps)) goto fail;
  if (cache_parse_name(ps, &name)) goto fail;
  if (cache_parse_eol(ps)) goto fail;

  slot = (struct confent**)avl_insert(&dirstack[dircnt - 1]->entries, name);
  if (*slot) {
    ps->errmsg = "duplicate entry";
    goto fail;
  }

  ent = xnew(struct confent);
  ent->type = CONFENT_NONE;
  ent->name = name;
  avl_init_data(&ent->ad, name);
  *slot = ent;
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

  slot = (struct confent**)avl_insert(&dirstack[dircnt - 1]->entries, name);
  if (*slot) {
    ps->errmsg = "duplicate entry";
    goto fail;
  }

  ent = xnew(struct confent);
  ent->type = CONFENT_FILE;
  ent->name = name;
  avl_init_data(&ent->ad, name);
  *slot = ent;
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

  slot = (struct confent**)avl_insert(&dirstack[dircnt - 1]->entries, name);
  if (*slot) {
    ps->errmsg = "duplicate entry";
    goto fail;
  }

  ent = xnew(struct confent);
  ent->type = CONFENT_DIR;
  ent->name = name;
  avl_init_data(&ent->ad, name);
  *slot = ent;
  name = NULL;

  confdir_init(&ent->dir);
  ent->dir.mtime = mtime;

  // push directory
  if (dircnt == dircap)
    xresize(dirstack, dircap *= 2, struct confdir*);
  dirstack[dircnt++] = &ent->dir;
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

static void cache_write_file(FILE *f, struct conffile *file, struct cf_block *block)
{
  for (unsigned int i = 0; i < block->cmdcnt; i++) {
    union cf_command *cmd = block->cmds[i];
    fprintf(f, "%u ", cmd->h.lineno);
    switch (cmd->h.type) {
      case CF_LINK:
        fprintf(f, "link %s\n", cmd->link.path);
        break;
    }
  }
}

static void cache_write_dir(FILE *f, struct confdir *dir)
{
  AVL_ITER(&dir->entries, struct confent, ent) {
    switch (ent->type) {
      case CONFENT_NONE:
        // ignore
        break;
      case CONFENT_FILE:
        if (ent->file.cf) {
          fprintf(f, "f %s %zu %lld\n", ent->name, ent->file.size, ent->file.mtime);
          cache_write_file(f, &ent->file, &ent->file.cf->block);
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
  char tmpfile[PATH_MAX];
  size_t filelen = strlen(filename);
  memcpy(tmpfile, filename, filelen);
  strcpy(tmpfile + filelen, ".tmp");

  int fd = open(tmpfile, O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (fd < 0) {
    ERR_SYS("failed to open '%s'", tmpfile);
    return -1;
  }

  FILE *f = fdopen(fd, "w");
  if (!f) {
    ERR_SYS("fdopen");
    close(fd);
    return -1;
  }

  cache_write(f, conf);
  if (ferror(f)) {
    ERR("write error");
    fclose(f);
    return -1;
  }

  fclose(f);

  if (rename(tmpfile, filename)) {
    ERR_SYS("failed to rename '%s' to '%s'", tmpfile, filename);
    return -1;
  }

  return 0;
}

#undef s

/*** PLAN TYPES ******************************************************** {{{1 */

enum action_type {
  PLAN_LINK
};

struct action
{
  enum action_type type;
  char *path;
  struct avl_data ad;
  struct {
    char *target;
  } link;
};

struct plan
{
  struct avl actions;
};

/*** PLAN UTILITIES **************************************************** {{{1 */

static void plan_init(struct plan *plan)
{
  avl_init(&plan->actions, struct action, ad);
}

/*** PLAN RESOLVER ***************************************************** {{{1 */

static int resolve_cf_link(struct plan *plan, union cf_command *cmd)
{
  char *path = cmd->link.path;
  struct action **slot = (struct action **)avl_insert(&plan->actions, path);
  if (*slot) {
    ERR("multiple actions for '%s'", path);
    return -1;
  }
  path = strdup(path);
  xdefine(act, struct action);
  act->type = PLAN_LINK;
  act->path = path;
  avl_init_data(&act->ad, path);
  act->link.target = "something";
  *slot = act;
  return 0;
}

static int resolve_cf(struct plan *plan, struct cf_block *block)
{
  for (unsigned int i = 0; i < block->cmdcnt; i++) {
    union cf_command *cmd = block->cmds[i];
    switch (cmd->h.type) {
      case CF_LINK:
        if (resolve_cf_link(plan, cmd)) return -1;
        break;
    }
  }
  return 0;
}

static int resolve_dir(struct plan *plan, struct confdir *dir)
{
  int ret = 0;
  AVL_ITER(&dir->entries, struct confent, ent) {
    switch (ent->type) {
      case CONFENT_NONE:
        break;
      case CONFENT_FILE:
        if (resolve_cf(plan, &ent->file.cf->block))
          ret = -1;
        break;
      case CONFENT_DIR:
        if (resolve_dir(plan, &ent->dir))
          ret = -1;
        break;
    }
  }
  return ret;
}

static int resolve(struct plan *plan, struct config *conf)
{
  return resolve_dir(plan, &conf->root);
}

/*** PLAN EXECUTOR ***************************************************** {{{1 */

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
  }
  return 0;
}

static int execute_link(struct action *act, int real)
{
  char target[256];
  size_t target_len = strlen(act->link.target);
  if (target_len >= sizeof(target)) {
    ERR("target too long: %s", act->link.target);
    return -1;
  }
  memcpy(target, act->link.target, target_len + 1);

  char existing[256+1];
  ssize_t existing_len = readlink(act->path, existing, sizeof(existing));
  if (existing_len < 0) {
    if (errno == EINVAL) {
      ERR("refusing to replace %s -> %s", act->path, target);
      return -1;
    }
    if (errno != ENOENT) {
      ERR_SYS("failed to readlink '%s'", act->path);
      return -1;
    }
  } else if (existing_len == (ssize_t)target_len && !memcmp(existing, target, target_len)) {
    // nothing to do
    return 0;
  }

  msg(prefix_creating_symlink, "%s -> %s", act->path, target);

  if (!real)
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
  ERR_SYS("symlink failed");
  return -1;
}

static int execute(struct plan *plan, int real)
{
  int ret = 0;
  AVL_ITER(&plan->actions, struct action, act) {
    switch (act->type) {
      case PLAN_LINK:
        ret |= execute_link(act, real);
        break;
    }
  }
  return ret;
}

/*** GALEOPT *********************************************************** {{{1 */

// Screw getopt! This is a lot more painful to read and a lot more fun to use.

#define GALEOPT(argv) \
  for (char **_argv = (argv), *_arg, _s, _n; \
    (_arg = *_argv) && (( \
      _arg[0] != '-' || _arg[1] == '\0' \
      ? DIE(2, "invalid argument: '%s'", _arg) \
      : *(++_arg) == '-' \
      ? (_s = 0, ++_argv, 1) \
      : (_s = *_arg, *(_arg++) = '-', *_arg ? (++(*_argv), 1) : (++_argv, 1)) \
    ), _n = 1); \
    _n && (_s ? (DIE(2, "invalid option: -%c", _s), 1) \
               : DIE(2, "invalid option: --%s", _arg), 1))

#define OPT(s, l, code) \
  if (_s ? _s == s : !strcmp(_arg, l)) { code; _n = 0; continue; }

#define OPT_LONG(l, code) \
  if (!_s && !strcmp(_arg, l)) { code; _n = 0; continue; }

/*** MAIN ************************************************************** {{{1 */

static void help(FILE *f, int ret)
{
  static const char usage[] = "\
usage: galinst [options]\n\
\n\
Install Gale.\n\
\n\
Options:\n\
  -n, --dry-run     don't do anything, only show what would happen\n\
  -v, --verbose     be verbose\n\
  -h, --help        show this help and exit\n\
";
  fputs(usage, f);
  exit(ret);
}

int main(int argc, char **argv)
{
  if (isatty(2))
    enable_colors();

  UNUSED int real = 1;
  UNUSED int verbose = 0;

  GALEOPT(argv + 1) {
    OPT('h', "help", help(stdout, 0));
    OPT('n', "dry-run", real = 0);
    OPT('v', "verbose", verbose = 1);
  }

  const char *home = getenv("HOME");
  if (!home) {
    ERR("$HOME is not set");
    return 1;
  }
  if (chdir(home)) {
    ERR_SYS("could not chdir to '%s'", home);
    return 1;
  }

  // General Architecture
  // --------------------

  struct config conf;
  config_init(&conf);

  //
  // 1. Load the cache.
  //
  if (cache_load(&conf, ".gale.cache")) return 1;

  //
  // 2. Load the config. The cache is used to skip reading files and
  //    directories that haven't changed based on their stat() information.
  //
  if (update_config(&conf)) return 1;

  //
  // 3. If anything changed, write the updated cache.
  //
  if (conf.need_update_cache) {
    MSG("Updating cache...");
    if (cache_save(&conf, ".gale.cache")) return 1;
  }

  //
  // 4. Quit if the config files contain syntax errors. The cache is still
  //    updated with the files that were parsed correctly.
  //
  if (conf.bad) return 1;

  //
  // 5. Resolve the config into a plan.
  //
  struct plan plan;
  plan_init(&plan);
  if (resolve(&plan, &conf)) return 1;

  //
  // 6. Delete files that are no longer produced by any config file.
  //
  //if (prune(&conf)) return 1;

  //
  // 7. Write the updated log if any files were added or removed.
  //
  //if (log_write(&conf)) return 1;

  //
  // 8. Update the home directory.
  //
  if (execute(&plan, /* TODO: real */0)) return 1;

  return 0;
}

/*********************************************************************** }}}1 */

// vim:fo+=n:fdm=marker
