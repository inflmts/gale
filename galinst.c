#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MSG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define WARN(fmt, ...) MSG("warning: " fmt, ##__VA_ARGS__)
#define ERR(fmt, ...) MSG("error: " fmt, ##__VA_ARGS__)
#define ERR_SYS(fmt, ...) ERR(fmt ": %s", ##__VA_ARGS__, strerror(errno))
#define DIE(n, fmt, ...) (ERR(fmt, ##__VA_ARGS__), exit(n))

#define container_of(p, type, member) ((type*)((char*)p - offsetof(type, member)))

static inline void findent(FILE *stream, unsigned int indent)
{
  while (indent) {
    putc(' ', stream);
    --indent;
  }
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

#define xnew(type) (type*)xmalloc(sizeof(type))
#define xnewv(n, type) (type*)xmalloc(n * sizeof(type))
#define xresize(p, n, type) (type*)xrealloc(p, n * sizeof(type))
#define xresizei(p, n, type) (p = xresize(p, n, type))

/*******************************************************************************
 * DICT
 ******************************************************************************/

// simple sorted dictionary
// TODO: use red-black tree

struct node
{
  const char *key;
  struct node *next;
};

struct dict
{
  struct node *head;
};

static void dict_init(struct dict *d)
{
  d->head = NULL;
}

#define DICT_ITER(d, cur, nxt) \
  for (struct node *cur = (d)->head, *nxt; cur && (nxt = cur->next, cur); cur = nxt)

static struct node *dict_get(struct dict *d, const char *key)
{
  struct node *node = d->head;
  while (node) {
    if (!strcmp(key, node->key))
      return node;
    node = node->next;
  }
  return NULL;
}

static struct node *dict_insert(struct dict *d, struct node *node)
{
  const char *key = node->key;
  struct node **slot = &d->head;
  while (*slot) {
    int diff = strcmp(key, (*slot)->key);
    if (diff == 0)
      return NULL;
    if (diff < 0)
      break;
    slot = &(*slot)->next;
  }
  node->next = *slot;
  *slot = node;
  return node;
}

//static struct node *dict_remove(struct dict *d, const char *key);

/*******************************************************************************
 * CONFIG
 ******************************************************************************/

#define CONFIG_PATHBUF_SIZE 256
#define CONFIG_MTIME_NONE 0

struct confent;
static void confent_dump(struct confent *ent, FILE *stream, int indent);

/*-- conffile --*/

struct conffile
{
  off_t size;
  long long mtime;
  char *flags;
  size_t flagcnt;
  struct template *tmpl;
};

static void conffile_init(struct conffile *file)
{
  file->size = 0;
  file->mtime = CONFIG_MTIME_NONE;
  file->flags = NULL;
  file->flagcnt = 0;
  file->tmpl = NULL;
}

/*-- confdir --*/

#define CONFDIR_ROOT 0x1

struct confdir
{
  long long mtime;
  int flags;
  struct dict entries;
};

enum {
  CONFENT_NONE,
  CONFENT_FILE,
  CONFENT_DIR
};

struct confent
{
  int type;
  int fresh;
  char *name;
  struct node node;
  union {
    struct conffile file;
    struct confdir dir;
  };
};

static void confdir_init(struct confdir *dir)
{
  dir->mtime = CONFIG_MTIME_NONE;
  dir->flags = 0;
  dict_init(&dir->entries);
}

static void confdir_dump(struct confdir *dir, FILE *stream, int indent, const char *name)
{
  findent(stream, indent);
  fprintf(stream, "%s/\n", name);
  DICT_ITER(&dir->entries, node, next) {
    struct confent *ent = container_of(node, struct confent, node);
    confent_dump(ent, stream, indent + 2);
  }
}

/*-- confent --*/

static void confent_init(struct confent *ent, int type, char *name)
{
  ent->type = type;
  ent->name = name;
  ent->node.key = name;
}

static void confent_free(struct confent *ent)
{
  free(ent->name);
}

static void confent_dump(struct confent *ent, FILE *stream, int indent)
{
  switch (ent->type) {
    case CONFENT_NONE:
      findent(stream, indent);
      fprintf(stream, "%s (unknown)\n", ent->name);
      break;
    case CONFENT_FILE:
      findent(stream, indent);
      fprintf(stream, "%s (file)\n", ent->name);
      //conffile_dump(&ent->file, stream, indent, ent->name);
      break;
    case CONFENT_DIR:
      confdir_dump(&ent->dir, stream, indent, ent->name);
      break;
  }
}

/*-- config --*/

struct config
{
  struct confdir root;
};

static void config_init(struct config *conf)
{
  confdir_init(&conf->root);
  conf->root.flags = CONFDIR_ROOT;
}

//static void config_free(struct config *conf);

static int config_update_dir(struct config *conf, struct confdir *dir,
                             char *path, size_t pathlen, struct stat *st);

static int config_update_ent(struct config *conf, struct confent *ent, char *path, size_t pathlen)
{
  struct stat st;
  if (stat(path, &st)) {
    ERR_SYS("failed to stat '%s'", path);
    return -1;
  }

  if (S_ISREG(st.st_mode)) {
    switch (ent->type) {
      case CONFENT_NONE:
        break;
      case CONFENT_FILE:
        break;
      case CONFENT_DIR:
        break;
    }
  } else if (S_ISDIR(st.st_mode)) {
    switch (ent->type) {
      case CONFENT_NONE:
        ent->type = CONFENT_DIR;
        confdir_init(&ent->dir);
        return config_update_dir(conf, &ent->dir, path, pathlen, &st);
      case CONFENT_FILE:
        break;
      case CONFENT_DIR:
        return config_update_dir(conf, &ent->dir, path, pathlen, &st);
    }
  }
}

static int config_scan_dir(struct config *conf, struct confdir *dir, const char *path)
{
  DIR *dirp = opendir(path);
  if (!dirp) {
    ERR_SYS("failed to open '%s'", path);
    return -1;
  }

  struct dirent *dent;
  while (errno = 0, dent = readdir(dirp)) {
    const char *name = dent->d_name;

    // ignore files that begin with a dot
    if (*name == '.')
      continue;

    // skip entries that are not files or directories
#ifdef _DIRENT_HAVE_D_TYPE
    if (dent->d_type != DT_UNKNOWN
        && dent->d_type != DT_DIR
        && (dir->flags & CONFDIR_ROOT || dent->d_type != DT_REG))
      continue;
#endif

//  size_t namelen = strlen(name);
//  if (pathlen + 1 + namelen >= CONFIG_PATHBUF_SIZE) {
//    WARN("path too long: %s/%s", path, name);
//    continue;
//  }

    struct confent *ent;
    struct node *node = dict_get(&dir->entries, name);
    if (node) {
      ent = container_of(node, struct confent, node);
    } else {
      ent = xnew(struct confent);
      confent_init(ent, CONFENT_NONE, strdup(name));
      node = &ent->node;
      dict_insert(&dir->entries, node);
    }
    ent->fresh = 1;
  }

  // find deleted entries
//for (struct node *node = dir->entries.head; node; node = node->next) {
//  struct confent *ent = container_of(node, struct confent, node);
//  if (!ent.fresh)
//}

  if (errno) {
    ERR_SYS("readdir");
    closedir(dirp);
    return -1;
  }

  closedir(dirp);
  return 0;
}

static int config_update_dir(struct config *conf, struct confdir *dir,
                             char *path, size_t pathlen, struct stat *st)
{
  if (dir->mtime != st->st_mtime) {
    // update directory listing
    if (config_scan_dir(conf, dir, path))
      return -1;
    dir->mtime = st->st_mtime;
  }

  for (struct node *node = dir->entries.head; node; node = node->next) {
    struct confent *ent = container_of(node, struct confent, node);
    size_t namelen = strlen(ent->name);
    if (pathlen + 1 + namelen >= CONFIG_PATHBUF_SIZE) {
      ERR("path too long: %s/%s", path, ent->name);
      return -1;
    }
    path[pathlen] = '/';
    strcpy(path + pathlen + 1, ent->name);
    int ret = config_update_ent(conf, ent, path, pathlen + 1 + namelen);
    path[pathlen] = '\0';
    if (ret)
      return -1;
  }

  return 0;
}

static int config_update(struct config *conf)
{
  char path[CONFIG_PATHBUF_SIZE];
  strcpy(path, ".gale");
  size_t pathlen = 5;

  struct stat st;
  if (stat(path, &st)) {
    ERR_SYS("failed to stat '%s'", path);
    return -1;
  }

  if (config_update_dir(conf, &conf->root, path, pathlen, &st))
    return -1;

  return 0;
}

static void config_dump(struct config *conf, FILE *stream)
{
  return confdir_dump(&conf->root, stream, 0, ".gale");
}

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
//static void template_dump_block(union template_block *block, FILE *stream, int indent)
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
//
//#define ISDIRECTIVE(c) ((c) >= 'a' && (c) <= 'z')
//
//static int template_parse_install(struct parse_state *ps)
//{
//  // gale::~/.local/bin/bash
//  // gale::if (archiplex and foo)
//  // gale::elif c
//  // gale::else
//  // gale::endif
//}
//
//static struct template *template_parse(const char *s, size_t len)
//{
//#define PARSE_ERR(fmt, ...) ERR("line %u: " fmt, lineno, ##__VA_ARGS__)
//
//  const char *end = s + len;
//  const char *line;
//  unsigned int lineno = 0;
//  char directive[16];
//  size_t len;
//
//  struct template *tmpl = template_new();
//  struct template_block *current_block = template_add_block(tpl);
//  current_block->begin = s;
//  current_block->type = TEMPLATE_BLOCK_LITERAL;
//
//start_of_line:
//  line = s;
//  ++lineno;
//
//body:
//  if (*s == '\n') {
//    ++s;
//    goto start_of_line;
//  }
//
//  // find directive marker
//  if (*s     != 'g') goto body;
//  if (*(++s) != 'a') goto body;
//  if (*(++s) != 'l') goto body;
//  if (*(++s) != 'e') goto body;
//  if (*(++s) != ':') goto body;
//  if (*(++s) != ':') goto body;
//  ++s;
//
//  // this is a directive
//  if (!ISDIRECTIVE(*s)) goto fail;
//  t = s;
//  do ++s;
//  while (ISDIRECTIVE(*s));
//  len = s - t;
//  if (len >= sizeof(directive)) goto fail;
//  memcpy(directive, t, len);
//  directive[len] = '\0';
//
//  ps->s = s;
//  if (!strcmp(directive, "install"))
//    goto directive_install;
//  if (!strcmp(directive, "if"))
//    goto directive_if;
//
//  PARSE_ERR("invalid directive: %s", directive);
//  goto fail;
//
//directive_install:
//directive_if:
//  enum {
//    IFSTATE_INIT,
//    IFSTATE_NOT
//  } ifstate = IFSTATE_INIT;
//
//  cond = template_new_cond();
//
//directive_if:
//  if (*s == ' ') {
//    ++s;
//    goto directive_if;
//  }
//
//  if (IS_IFTOKEN(*s)) {
//    char *token = s++;
//    while (IS_IFTOKEN(*s)) ++s;
//    if (s - token == 3 && !memcmp(token, "not", 3)) {
//      ifstate = IFSTATE_NORM;
//      goto directive_if;
//    }
//    if (s - token == 3 && !memcmp(token, "and", 3)) {
//      struct template_bexpr *be = xnew(struct template_bexpr);
//      be->type = TEMPLATE_BEXPR_AND;
//      goto directive_if;
//    }
//    goto directive_if;
//  }
//
//  // consume the rest of the line
//  while (*s++ != '\n');
//  current_block->end = s;
//  current_block = template_add_block(tpl);
//  current_block->begin = s;
//  current_block->type = TEMPLATE_BLOCK_COND;
//  current_block->cond = cond;
//  cond = NULL;
//  goto start_of_line;
//
//fail:
//  template_destroy(tmpl);
//  if (cond)
//    template_destroy_cond(cond);
//  return -1;
//}
//
//static struct template *template_load(const char *filename)
//{
//  struct stat st;
//  char *buf = NULL;
//  struct template *tmpl = NULL;
//
//  if (stat(filename, &st)) {
//    ERR_SYS("failed to stat '%s'", filename);
//    goto finish;
//  }
//  size_t size = st.st_size;
//  int fd = open(filename, O_RDONLY);
//  if (fd < 0) {
//    ERR_SYS("failed to open '%s'", filename);
//    goto finish;
//  }
//  buf = xmalloc(size + 1);
//  char *s = buf;
//  while (s < buf + size) {
//    ssize_t readsize = read(fd, s, buf + size - s);
//    if (readsize < 0) {
//      ERR_SYS("read");
//      close(fd);
//      goto finish;
//    }
//    s += readsize;
//  }
//  close(fd);
//  fd = -1;
//
//  // append a trailing newline if necessary
//  if (buf[size - 1] != '\n')
//    buf[size++] = '\n';
//
//  tmpl = template_parse(buf, size);
//
//finish:
//  free(buf);
//  return tmpl;
//}
//
//void template_dump(struct template *tmpl, FILE *stream)
//{
//  fprintf(stream, "install path: %s\n", tmpl->install_path);
//  template_dump_body(&tmpl->body, stream, 0);
//}

/*******************************************************************************
 * LOG
 ******************************************************************************/

//static int log_load(struct config *conf, const char *filename);
//static int log_write(struct config *conf, const char *filename);

/*******************************************************************************
 * XOPT
 ******************************************************************************/

#define XOPT(argv) \
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

static void help(FILE *stream, int ret)
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
  fputs(usage, stream);
  exit(ret);
}

int main(int argc, char **argv)
{
  int dry = 0;
  int verbose = 0;

  XOPT(argv + 1) {
    OPT('h', "help", help(stdout, 0));
    OPT('n', "dry-run", dry = 1);
    OPT('v', "verbose", verbose = 1);
  }

  const char *home = getenv("HOME");
  if (!home) {
    DIE(1, "$HOME is not set");
  }
  if (chdir(home)) {
    ERR_SYS("could not chdir to '%s'", home);
    exit(1);
  }

  struct config conf;
  config_init(&conf);
  if (config_update(&conf))
    exit(1);
  config_dump(&conf, stdout);

  return 0;
}
