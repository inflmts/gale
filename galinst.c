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

typedef long long gale_mtime_t;

/*******************************************************************************
 * LOGGING
 ******************************************************************************/

#ifdef __GNUC__
# define PRINTF(i, j) __attribute__((format(printf, i, j)))
#else
# define PRINTF(i, j)
#endif

static void vmsg(const char *prefix, const char *format, va_list ap)
{
  fputs(prefix, stderr);
  vfprintf(stderr, format, ap);
  putc('\n', stderr);
}

static void vmsg_sys(const char *prefix, const char *format, va_list ap)
{
  fputs(prefix, stderr);
  vfprintf(stderr, format, ap);
  fprintf(stderr, ": %s\n", strerror(errno));
}

PRINTF(2, 3)
static void msg(const char *prefix, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vmsg(prefix, format, ap);
  va_end(ap);
}

PRINTF(2, 3)
static void msg_sys(const char *prefix, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vmsg_sys(prefix, format, ap);
  va_end(ap);
}

#define MSG(...) msg("", __VA_ARGS__)
#define WARN(...) msg("warning: ", __VA_ARGS__)
#define ERR(...) msg("error: ", __VA_ARGS__)
#define ERR_SYS(...) msg_sys("error: ", __VA_ARGS__)
#define DIE(n, ...) (msg("error: ", __VA_ARGS__), exit(n))
#define DIE_SYS(n, ...) (msg_sys("error: ", __VA_ARGS__), exit(n))
#ifndef NDEBUG
# define DEBUG(...) msg("debug: ", __VA_ARGS__)
#else
# define DEBUG(...) (void)0
#endif

PRINTF(3, 4)
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

/*******************************************************************************
 * MEMORY
 ******************************************************************************/

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
#define xnewv(n, type) ((type*)xmalloc(n * sizeof(type)))
#define xdefine(p, type) type *p = xmalloc(sizeof(type))
#define xresize(p, n, type) (p = (type*)xrealloc(p, n * sizeof(type)))

/*******************************************************************************
 * AVL
 ******************************************************************************/

// TODO: make this an actual avl tree

struct avl_node
{
  const char *key;
  void *next;
};

struct avl
{
  void *head;
  ptrdiff_t offset;
};

static void avl_init_offset(struct avl *avl, ptrdiff_t offset)
{
  avl->head = NULL;
  avl->offset = offset;
}

#define avl_init(avl, type, member) avl_init_offset(avl, offsetof(type, member))

#define AVL_ITER(avl, type, c, n) \
  for (type *c = (avl)->head, *n; \
       c && (n = ((struct avl_node*)((char*)(c) + (avl)->offset))->next, c); \
       c = n)

#define AVL_NODE(item) ((struct avl_node*)((char*)(item) + avl->offset))

static void *avl_get(struct avl *avl, const char *key)
{
  void *item = avl->head;
  while (item) {
    if (!strcmp(key, AVL_NODE(item)->key))
      return item;
    item = AVL_NODE(item)->next;
  }
  return NULL;
}

static void *avl_insert(struct avl *avl, void *item)
{
  const char *key = AVL_NODE(item)->key;
  void **slot = &avl->head;
  while (*slot) {
    int diff = strcmp(key, AVL_NODE(*slot)->key);
    if (diff == 0)
      return *slot;
    if (diff < 0)
      break;
    slot = &AVL_NODE(*slot)->next;
  }
  AVL_NODE(item)->next = *slot;
  *slot = item;
  return item;
}

#undef AVL_NODE

//static struct node *avl_remove(struct avl *d, const char *key);

/*******************************************************************************
 * TEMPLATE
 ******************************************************************************/

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

/*******************************************************************************
 * CONFIG FILE
 ******************************************************************************/

enum {
  CF_LINK
};

struct cf_command;

struct cf_block
{
  struct cf_command **cmds;
  unsigned int cmdcnt;
  unsigned int cmdcap;
};

struct cf_command
{
  int type;
};

struct cf
{
  struct cf_block block;
};

/*-- cf: utility functions --*/

static void cf_init_block(struct cf_block *block)
{
  block->cmds = xnewv(4, struct cf_command*);
  block->cmdcnt = 0;
  block->cmdcap = 4;
}

static void cf_free_block(struct cf_block *block)
{
  free(block->cmds);
}

static void cf_add_command(struct cf_block *block, struct cf_command *cmd)
{
  if (block->cmdcnt >= block->cmdcap) {
    block->cmdcap <<= 1;
    xresize(block->cmds, block->cmdcap, struct cf_command*);
  }
  block->cmds[block->cmdcnt++] = cmd;
}

struct cf_link_command
{
  int type;
  char *path;
};

static void cf_add_link(struct cf_block *block, char *path)
{
  xdefine(cmd, struct cf_link_command);
  cmd->type = CF_LINK;
  cmd->path = path;
  cf_add_command(block, (struct cf_command*)cmd);
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

/*-- cf: parser --*/

#define ISSPACE(c) ((c) == ' ')
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define ISUPPER(c) ((c) >= 'A' && (c) <= 'Z')
#define ISLOWER(c) ((c) >= 'a' && (c) <= 'z')
#define ISALNUM(c) (ISDIGIT(c) || ISUPPER(c) || ISLOWER(c))
#define ISDIRECTIVE(c) ISLOWER(c)
#define ISPATH(c) (ISALNUM(c) || (c) == '.' || (c) == '-' || (c) == '_')

//#define CF_ERR(fmt, ...) ERR("line %u: " fmt, cf_lineno, ##__VA_ARGS__)

static char *cf_cur;
static char *cf_end;
static unsigned int cf_lineno;
static struct cf_block *cf_block;
#define s cf_cur

static char *cf_path(void)
{
  char *begin = s--;
begin:
  ++s;
  if (*s == '.') goto begin;
  if (ISPATH(*s)) goto rest;
  return NULL;
rest:
  ++s;
  if (*s == '/') goto begin;
  if (ISPATH(*s)) goto rest;
  size_t len = s - begin;
  char *path = memcpy(xmalloc(len + 1), begin, len);
  path[len] = '\0';
  return path;
}

static int cf_space(void)
{
  if (!ISSPACE(*s)) return 0;
  do ++s;
  while (ISSPACE(*s));
  return 1;
}

static int cf_next(void)
{
  while (ISSPACE(*s)) ++s;
  return *s != '\n' && *s != '#';
}

static struct cf_command *cf_link(void)
{
  // gale::link <path> [if <cond>]

  char *path = NULL;

  if (!(path = cf_path()))
    goto fail;
  if (cf_next())
    goto fail;

  //if (*s != 'i' || *(++s) != 'f')
  //  goto fail;
  //++s;
  //if (template_parse_space())
  //  goto fail;
  //if (template_parse_condition())
  //  goto fail;

done:
  struct cf_link_command *cmd = xnew(struct cf_link_command);
  cmd->type = CF_LINK;
  cmd->path = path;
  return (void*)cmd;

fail:
  free(path);
  return NULL;
}

static struct cf_command *cf_command(void)
{
  if (*s == '~') {
    if (*(++s) != '/')
      return NULL;
    ++s;
    return cf_link();
  }

  if (!ISDIRECTIVE(*s))
    return NULL;
  char *name = s;
  do ++s;
  while (ISDIRECTIVE(*s));
  size_t len = s - name;

  if (len == 4 && !memcmp(name, "link", 4))
    return cf_link();

  return NULL;
}

static int cf_parse_main(void)
{
line:
  ++cf_lineno;
  if (s >= cf_end)
    return 1;

body:
  if (*s == '\n') {
    ++s;
    goto line;
  }

  // find command string
  if (*(s++) != 'g') goto body;
  if (*s     != 'a') goto body;
  if (*(++s) != 'l') goto body;
  if (*(++s) != 'e') goto body;
  if (*(++s) != ':') goto body;
  if (*(++s) != ':') goto body;
  ++s;

  struct cf_command *cmd = cf_command();
  if (!cmd)
    return 0;

  if (cf_block->cmdcnt >= cf_block->cmdcap) {
    cf_block->cmdcap <<= 1;
    xresize(cf_block->cmds, cf_block->cmdcap, struct cf_command*);
  }
  cf_block->cmds[cf_block->cmdcnt++] = cmd;

  while (*s != '\n') ++s;
  ++s;
  goto line;
}

static struct cf *cf_parse(char *buf, size_t len)
{
  struct cf *cf = cf_new();
  cf_cur = buf;
  cf_end = buf + len;
  cf_lineno = 0;
  cf_block = &cf->block;

  if (cf_parse_main())
    return cf;

  ERR("syntax error at line %u", cf_lineno);

  cf_destroy(cf);
  return NULL;
}

#undef s

static struct cf *cf_load(const char *filename)
{
  //MSG("loading %s", filename);
  struct stat st;
  char *buf = NULL;
  struct cf *cf = NULL;

  if (stat(filename, &st)) {
    ERR_SYS("failed to stat '%s'", filename);
    goto finish;
  }
  size_t size = st.st_size;
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    ERR_SYS("failed to open '%s'", filename);
    goto finish;
  }
  buf = xmalloc(size + 1);
  char *s = buf;
  while (s < buf + size) {
    ssize_t readsize = read(fd, s, buf + size - s);
    if (readsize < 0) {
      ERR_SYS("read");
      close(fd);
      goto finish;
    }
    s += readsize;
  }
  close(fd);

  // append a trailing newline if necessary
  if (buf[size - 1] != '\n')
    buf[size++] = '\n';

  cf = cf_parse(buf, size);

finish:
  free(buf);
  return cf;
}

/*-- cf: dump --*/

static void cf_dump_command(struct cf_command *cmd, FILE *f, int indent)
{
  switch (cmd->type) {
    case CF_LINK:
      indentf(f, indent, "link ~/%s", ((struct cf_link_command*)cmd)->path);
      break;
  }
}

static void cf_dump_block(struct cf_block *block, FILE *f, int indent)
{
  if (block->cmdcnt) {
    indentf(f, indent, "---");
    for (unsigned int i = 0; i < block->cmdcnt; i++)
      cf_dump_command(block->cmds[i], f, indent);
    indentf(f, indent, "---");
  } else {
    indentf(f, indent, "(none)");
  }
}

static void cf_dump(struct cf *cf, FILE *f, int indent)
{
  cf_dump_block(&cf->block, f, indent);
}

/*******************************************************************************
 * CONFIG
 ******************************************************************************/

#define CONFIG_MTIME_NONE 0

/*-- conffile --*/

struct conffile
{
  size_t size;
  gale_mtime_t mtime;
  struct cf *cf;
};

static void conffile_init(struct conffile *file)
{
  file->size = 0;
  file->mtime = CONFIG_MTIME_NONE;
  file->cf = NULL;
}

static void conffile_free(struct conffile *file)
{
  if (file->cf)
    cf_destroy(file->cf);
}

static void conffile_dump(struct conffile *file, FILE *f, int indent, const char *name)
{
  indentf(f, indent, "%s", name);
  if (file->cf)
    cf_dump(file->cf, f, indent + 2);
}

/*-- confdir --*/

#define CONFDIR_ROOT 0x1

enum {
  CONFENT_NONE,
  CONFENT_FILE,
  CONFENT_DIR
};

struct confdir
{
  gale_mtime_t mtime;
  int flags;
  struct avl entries;
};

struct confent
{
  int type;
  //int fresh;
  char *name;
  struct avl_node node;
  union {
    struct conffile file;
    struct confdir dir;
  };
};

static void confdir_init(struct confdir *dir)
{
  dir->mtime = CONFIG_MTIME_NONE;
  dir->flags = 0;
  avl_init(&dir->entries, struct confent, node);
}

static void confdir_free(struct confdir *dir)
{
  AVL_ITER(&dir->entries, struct confent, ent, next) {
    free(ent->name);
    switch (ent->type) {
      case CONFENT_FILE:
        conffile_free(&ent->file);
        break;
      case CONFENT_DIR:
        confdir_free(&ent->dir);
        break;
    }
  }
}

static struct confent *confdir_add(struct confdir *dir, const char *name, size_t namelen)
{
  char *namecopy = memcpy(xmalloc(namelen + 1), name, namelen);
  namecopy[namelen] = '\0';
  xdefine(ent, struct confent);
  ent->type = CONFENT_NONE;
  ent->name = namecopy;
  ent->node.key = namecopy;
  struct confent *existing = avl_insert(&dir->entries, ent);
  if (existing == ent)
    return ent;
  free(namecopy);
  free(ent);
  return existing;
}

static void confdir_dump(struct confdir *dir, FILE *f, int indent, const char *name)
{
  indentf(f, indent, "%s/", name);
  AVL_ITER(&dir->entries, struct confent, ent, next) {
    switch (ent->type) {
      case CONFENT_NONE:
        indentf(f, indent, "%s (unknown)", ent->name);
        break;
      case CONFENT_FILE:
        conffile_dump(&ent->file, f, indent, ent->name);
        break;
      case CONFENT_DIR:
        confdir_dump(&ent->dir, f, indent, ent->name);
        break;
    }
  }
}

/*-- config --*/

struct config
{
  struct confdir root;
  struct avl log;
  int bad;
  int need_update_cache;
};

static void config_init(struct config *conf)
{
  confdir_init(&conf->root);
  conf->root.flags = CONFDIR_ROOT;
  avl_init_offset(&conf->log, 0);
  conf->need_update_cache = 0;
  conf->bad = 0;
}

//static void config_free(struct config *conf);

static void config_dump(struct config *conf, FILE *f)
{
  return confdir_dump(&conf->root, f, 0, ".gale");
}

/*******************************************************************************
 * CONFIG LOADER
 ******************************************************************************/

struct load_state
{
  struct config *conf;
  char *path;
  size_t pathlen;
  size_t pathcap;
};

static int load_file(struct load_state *ls, struct conffile *file, struct stat *st)
{
  if (file->size == st->st_size && file->mtime == st->st_mtime && file->cf)
    // nothing changed, the version in cache is correct
    return 0;

  DEBUG("reading file: %s", ls->path);
  if (file->cf)
    cf_destroy(file->cf);
  if (!(file->cf = cf_load(ls->path)))
    ls->conf->bad = 1;
  file->size = st->st_size;
  file->mtime = st->st_mtime;
  ls->conf->need_update_cache = 1;
}

static int scan_dir(struct confdir *dir, const char *path)
{
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

    struct confent *ent = confdir_add(dir, name, strlen(name));
    //ent->fresh = 1;
  }

  // find deleted entries
  //for (struct node *node = dir->entries.head; node; node = node->next) {
  //  struct confent *ent = container_of(node, struct confent, node);
  //  if (!ent.fresh)
  //}

  if (errno) {
    ERR_SYS("readdir");
    closedir(d);
    return -1;
  }

  closedir(d);
  return 0;
}

static int load_dir(struct load_state *ls, struct confdir *dir, struct stat *st)
{
  if (dir->mtime != st->st_mtime) {
    // update directory listing
    DEBUG("reading directory: %s", ls->path);
    if (scan_dir(dir, ls->path))
      return -1;
    dir->mtime = st->st_mtime;
    ls->conf->need_update_cache = 1;
  }

  int ret = -1;
  size_t pathlen = ls->pathlen;
  ls->path[pathlen] = '/';

  AVL_ITER(&dir->entries, struct confent, ent, next) {
    size_t namelen = strlen(ent->name);
    ls->pathlen = pathlen + 1 + namelen;
    if (ls->pathlen >= ls->pathcap)
      xresize(ls->path, (ls->pathcap *= 2), char);
    memcpy(ls->path + pathlen + 1, ent->name, namelen + 1);

    struct stat st;
    if (stat(ls->path, &st)) {
      ERR_SYS("failed to stat '%s'", ls->path);
      goto finish;
    }

    if (S_ISREG(st.st_mode)) {
      // conffile
      if (ent->type == CONFENT_DIR)
        confdir_free(&ent->dir);
      if (ent->type != CONFENT_FILE) {
        ent->type = CONFENT_FILE;
        conffile_init(&ent->file);
      }
      if (load_file(ls, &ent->file, &st))
        goto finish;
    } else if (S_ISDIR(st.st_mode)) {
      // confdir
      if (ent->type == CONFENT_FILE)
        conffile_free(&ent->file);
      if (ent->type != CONFENT_DIR) {
        ent->type = CONFENT_DIR;
        confdir_init(&ent->dir);
      }
      if (load_dir(ls, &ent->dir, &st))
        goto finish;
    }
  }

  ret = 0;

finish:
  ls->path[pathlen] = '\0';
  ls->pathlen = pathlen;
  return ret;
}

static int load_config(struct config *conf)
{
  struct stat st;
  if (stat(".gale", &st)) {
    ERR_SYS("failed to stat '%s'", ".gale");
    return -1;
  }

  struct load_state ls;
  ls.conf = conf;
  ls.path = memcpy(xmalloc(256), ".gale", 5+1);
  ls.pathlen = 5;
  ls.pathcap = 256;

  int ret = load_dir(&ls, &conf->root, &st);
  free(ls.path);
  return ret;
}

/*******************************************************************************
 * CACHE
 ******************************************************************************/

/*-- Cache Reader --*/

struct cache_parse_state
{
  struct config *conf;
  const char *cur;
  const char *end;
  unsigned int lineno;
};

#define s ps->cur
#define PARSE_ERR(...) (fprintf(stderr, "error: cache: line %u: ", ps->lineno), \
                        fprintf(stderr, __VA_ARGS__), \
                        putc('\n', stderr))

static int cache_skip_eol(struct cache_parse_state *ps, int noteol)
{
  if (*s != '\n')
    return noteol;
newline:
  ++s;
  ++ps->lineno;
  if (s >= ps->end)
    return -1;
  if (*s == '\n')
    goto newline;
  if (*s == '#') {
    while (*s != '\n')
      ++s;
    goto newline;
  }
  return 0;
}

static inline int cache_parse_eol(struct cache_parse_state *ps)
{
  return cache_skip_eol(ps, -1);
}

static int cache_parse_space(struct cache_parse_state *ps)
{
  if (!ISSPACE(*s))
    return -1;
  do ++s;
  while (ISSPACE(*s));
  return 0;
}

static int cache_parse_component(struct cache_parse_state *ps, const char **name, size_t *len)
{
  *name = s;
  while (*s == '.')
    ++s;
  if (!ISPATH(*s))
    return -1;
  do ++s;
  while (ISPATH(*s));
  *len = s - *name;
  return 0;
}

static int cache_parse_path(struct cache_parse_state *ps, const char **path, size_t *len)
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

static int cache_parse_size(struct cache_parse_state *ps, size_t *sizep)
{
  size_t size = 0;
  if (!ISDIGIT(*s))
    return -1;
  do size = size * 10 + *(s++) - '0';
  while (ISDIGIT(*s));
  *sizep = size;
  return 0;
}

static int cache_parse_mtime(struct cache_parse_state *ps, long long *mtimep)
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

static int cache_parse_cf_block(struct cache_parse_state *ps, struct cf_block *block)
{
  const char *path;
  char *pathcopy;
  size_t pathlen;

begin:
  if (*s == 'l') { ++s; goto l; }
  if (*s == ';') { ++s; return cache_parse_eol(ps); }
  return -1;

l:
  if (cache_parse_space(ps)) return -1;
  if (cache_parse_path(ps, &path, &pathlen)) return -1;
  if (cache_parse_eol(ps)) return -1;

  pathcopy = memcpy(xmalloc(pathlen + 1), path, pathlen);
  pathcopy[pathlen] = '\0';
  cf_add_link(block, pathcopy);
  goto begin;
}

static int cache_parse_file(struct cache_parse_state *ps, struct conffile *file)
{
  file->cf = cf_new();
  return cache_parse_cf_block(ps, &file->cf->block);
}

static int cache_parse_dir(struct cache_parse_state *ps, struct confdir *dir)
{
  const char *name;
  size_t namelen;
  size_t size;
  long long mtime;
  struct confent *ent;

begin:
  if (*s == 'f') { ++s; goto f; }
  if (*s == 'd') { ++s; goto d; }
  if (*s == '.') { ++s; return cache_parse_eol(ps) && !(dir->flags & CONFDIR_ROOT); }
  return -1;

f:
  if (cache_parse_space(ps)) return -1;
  if (cache_parse_component(ps, &name, &namelen)) return -1;
  if (cache_parse_space(ps)) return -1;
  if (cache_parse_size(ps, &size)) return -1;
  if (cache_parse_space(ps)) return -1;
  if (cache_parse_mtime(ps, &mtime)) return -1;
  if (cache_parse_eol(ps)) return -1;

  ent = confdir_add(dir, name, namelen);
  if (ent->type != CONFENT_NONE)
    return -1;
  ent->type = CONFENT_FILE;
  conffile_init(&ent->file);
  ent->file.size = size;
  ent->file.mtime = mtime;
  //DEBUG("cache: added file %s", ent->name);

  if (cache_parse_file(ps, &ent->file)) return -1;
  goto begin;

d:
  if (cache_parse_space(ps)) return -1;
  if (cache_parse_component(ps, &name, &namelen)) return -1;
  if (cache_parse_space(ps)) return -1;
  if (cache_parse_mtime(ps, &mtime)) return -1;
  if (cache_parse_eol(ps)) return -1;

  ent = confdir_add(dir, name, namelen);
  if (ent->type != CONFENT_NONE)
    return -1;
  ent->type = CONFENT_DIR;
  confdir_init(&ent->dir);
  ent->dir.mtime = mtime;
  //DEBUG("cache: added directory %s", ent->name);

  if (cache_parse_dir(ps, &ent->dir)) return -1;
  goto begin;
}

static int cache_parse_main(struct cache_parse_state *ps)
{
  if (cache_skip_eol(ps, 0)) return -1;
  if (*s != 'r') return -1;
  ++s;
  if (cache_parse_space(ps)) return -1;
  if (cache_parse_mtime(ps, &ps->conf->root.mtime)) return -1;
  if (cache_parse_eol(ps)) return -1;
  if (cache_parse_dir(ps, &ps->conf->root)) return -1;
  if (s < ps->end) return -1;
  return 0;
}

static int cache_parse(struct config *conf, const char *buf, size_t len)
{
  struct cache_parse_state ps;
  ps.conf = conf;
  ps.cur = buf;
  ps.end = buf + len;
  ps.lineno = 1;

  if (cache_parse_main(&ps)) {
    ERR("cache: syntax error at line %u", ps.lineno);
    return -1;
  }
  return 0;
}

static int cache_load(struct config *conf, const char *filename)
{
  int ret = -1;
  char *buf = NULL;
  int fd = open(filename, O_RDONLY);

  if (fd < 0) {
    if (errno == ENOENT)
      ret = 0;
    else
      ERR_SYS("failed to open '%s'", filename);
    goto finish;
  }

  off_t len = lseek(fd, 0, SEEK_END);
  if (len < 0) {
    ERR_SYS("lseek");
    goto finish;
  }
  if (len == 0) {
    WARN("empty cache file");
    ret = 0;
    goto finish;
  }

  if (lseek(fd, 0, SEEK_SET) < 0) {
    ERR_SYS("lseek");
    goto finish;
  }

  buf = xmalloc(len + 1);
  char *p = buf;
  while (p < buf + len) {
    ssize_t readsize = read(fd, p, buf + len - p);
    if (readsize < 0) {
      ERR_SYS("read");
      goto finish;
    }
    if (readsize == 0) {
      ERR_SYS("read returned 0");
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

/*-- Cache Writer --*/

static void cache_write_cf(FILE *f, struct cf_block *block)
{
  for (unsigned int i = 0; i < block->cmdcnt; i++) {
    struct cf_command *cmd = block->cmds[i];
    switch (cmd->type) {
      case CF_LINK:
        fprintf(f, "l %s\n", ((struct cf_link_command*)cmd)->path);
        break;
    }
  }
  fputs(";\n", f);
}

static void cache_write_dir(FILE *f, struct confdir *dir)
{
  AVL_ITER(&dir->entries, struct confent, ent, next) {
    switch (ent->type) {
      case CONFENT_FILE:
        fprintf(f, "f %s %zu %lld\n", ent->name, ent->file.size, ent->file.mtime);
        if (ent->file.cf)
          cache_write_cf(f, &ent->file.cf->block);
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

/*******************************************************************************
 * LOG
 ******************************************************************************/

//static int log_load(struct config *conf, const char *filename);
//static int log_write(struct config *conf, const char *filename);

/*******************************************************************************
 * OPTION PARSER
 ******************************************************************************/

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

/*******************************************************************************
 * MAIN
 ******************************************************************************/

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
  int dry = 0;
  int verbose = 0;

  GALEOPT(argv + 1) {
    OPT('h', "help", help(stdout, 0));
    OPT('n', "dry-run", dry = 1);
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

  // 1. Load the cache.
  if (cache_load(&conf, ".gale.cache")) return 1;

  // 2. Load the config against the cache.
  if (load_config(&conf)) return 1;
  //config_dump(&conf, stdout);

  // 3. Write the updated cache.
  if (conf.need_update_cache) {
    MSG("Updating cache...");
    if (cache_save(&conf, ".gale.cache")) return 1;
  }

  // 4. Delete unused files.
  //if (prune(&conf)) return 1;

  // 5. Write the log.
  //if (log_write(&conf)) return 1;

  // 6. Update the home directory.
  //if (update(&conf)) return 1;

  // 7. Write the cache again.
  //(unused)

  return 0;
}
