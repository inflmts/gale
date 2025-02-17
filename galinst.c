#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MSG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define WARN(fmt, ...) MSG("warning: " fmt, ##__VA_ARGS__)
#define ERR(fmt, ...) MSG("error: " fmt, ##__VA_ARGS__)
#define ERR_SYS(fmt, ...) ERR(fmt ": %s", ##__VA_ARGS__, strerror(errno))
#define DIE(n, fmt, ...) do { ERR(fmt, ##__VA_ARGS__); exit(n); } while (0)

#define containerof(p, type, member) ((type*)((char*)p - offsetof(type, member)))

/*******************************************************************************
 * MEMORY
 ******************************************************************************/

void *xmalloc(size_t size)
{
  void *p = malloc(size);
  if (!p) {
    ERR("out of memory");
    exit(-1);
  }
  return p;
}

void *xrealloc(void *p, size_t size)
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

void dict_init(struct dict *d)
{
  d->head = NULL;
}

void dict_free(struct dict *d)
{
  struct node *cur = d->head;
  struct node *next;
  while (cur) {
    next = cur->next;
    free(cur->key);
    free(cur);
    cur = next;
  }
}

struct node *dict_find(struct dict *d, const char *key)
{
  struct node *node = d->head;
  while (node) {
    if (!strcmp(key, node->key))
      return node;
    node = node->next;
  }
  return NULL;
}

struct node *dict_insert(struct dict *d, struct node *node)
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

//struct node *dict_remove(struct dict *d, const char *key);

/*******************************************************************************
 * STATE
 ******************************************************************************/

/*******************************************************************************
 * CONFIG
 ******************************************************************************/

int config_load(struct state *state, const char *filename);

/*******************************************************************************
 * TEMPLATE
 ******************************************************************************/

#define TEMPLATE_EXPR_FLAG 0x0
#define TEMPLATE_EXPR_AND 0x1
#define TEMPLATE_EXPR_OR 0x2
#define TEMPLATE_EXPR_NOT 0x100

struct template_expr
{
  int type;
  union {
    char *flag;
    struct {
      struct template_expr *operands;
      size_t opcnt;
    };
  };
};

enum {
  TEMPLATE_BLOCK_LITERAL,
  TEMPLATE_BLOCK_IF,
  TEMPLATE_BLOCK_ELIF,
  TEMPLATE_BLOCK_ELSE
};

/*-- template_body --*/

struct template_block
{
  int type;
};

struct template_body
{
  struct template_block **blocks;
  size_t bcnt;
  size_t bcap;
};

static void template_init_body(struct template_body *body)
{
  tmpl->blocks = xnew(struct template_block);
  tmpl->blkcnt = 0;
  tmpl->blkcap = 1;
}

static void template_free_body(struct template_body *body)
{
  while (tmpl->nblks)
    template_free_block(&tmpl->blocks[--tmpl->nblks]);
  free(tmpl->blocks);
}

static void template_add_block(struct template_body *body, struct template_block *block)
{
  if (body->bcnt >= body->bcap) {
    body->bcap = (tpl->bcap + 1) * 2;
    xresizei(body->blocks, tpl->bcap, struct template_block);
  }
  body->blocks[body->bcnt++] = block;
}

/*-- template_block --*/

struct template_literal_block
{
  int type;
  size_t begin;
  size_t end;
};

struct template_if_block
{
  int type;
  struct template_expr expr;
  struct template_body body;
};

static void template_destroy_literal_block(struct template_literal_block *block)
{
  free(block);
}

static void template_destroy_if_block(struct template_if_block *block)
{
  template_free_expr(&block->expr);
  template_free_body(&block->body);
  free(block);
}

static void template_destroy_block(struct template_block *block)
{
  switch (block->type) {
    case TEMPLATE_BLOCK_LITERAL:
      template_destroy_literal_block(block);
      break;
    case TEMPLATE_BLOCK_IF:
    case TEMPLATE_BLOCK_ELIF:
    case TEMPLATE_BLOCK_ELSE:
      template_destroy_if_block(block);
      break;
  }
}

#define IS_IFTOKEN(c) ((c) == '-' || ((c) >= 'a' && (c) <= 'z'))

void template_free_expr(struct template_expr *expr)
{
  switch (expr->type & 0xff) {
    case TEMPLATE_BEXPR_AND:
    case TEMPLATE_BEXPR_OR:
      while (expr->opcnt)
        template_free_expr(&expr->operands[--expr->opcnt]);
      break;
  }
}

/*-- template --*/

struct template
{
  char *install_path;
  struct template_body body;
};

static struct template *template_new()
{
  struct template *tmpl = xnew(struct template);
  tmpl->install_path = NULL;
  template_init_body(&tmpl->body);
  return tmpl;
}

static void template_destroy(struct template *tmpl)
{
  free(tmpl->install_path);
  template_free_body(&tmpl->body);
  free(tmpl);
}

/*-- template_parse --*/

static char *template_parse_expr(char *s, struct template_expr *expr)
{
  char *token = NULL;

begin:
  if (*s == ' ') {
    ++s;
    goto begin;
  }

  if (!IS_IFTOKEN(*s))
    goto fail;

  char *t = s++;
  while (IS_IFTOKEN(*s)) ++s;
  free(token);
  token = memcpy(xalloc(s - t + 1), t, s - t);
  token[s - t] = '\0';

  if (!strcmp(token, "not")) {
    if (!s = template_parse_expr(s, expr))
      goto fail;
    expr->type ^= TEMPLATE_EXPR_NOT;
    goto success;
  }

  if (!strcmp(token, "and")) {
    expr->type = TEMPLATE_EXPR_AND;
    expr->operands = xnew(struct template_expr);
    expr->opcnt = 1;
  }

success:
  free(token);
  return s;

fail:
  free(token);
  return NULL;
}

#define ISDIRECTIVE(c) ((c) >= 'a' && (c) <= 'z')

static int template_parse_install(struct parse_state *ps)
{
  // gale::~/.local/bin/bash
  // gale::if (archiplex and foo)
  // gale::elif c
  // gale::else
  // gale::endif
}

static struct template *template_parse(const char *s, size_t len)
{
#define PARSE_ERR(fmt, ...) ERR("line %u: " fmt, lineno, ##__VA_ARGS__)

  const char *end = s + len;
  const char *line;
  unsigned int lineno = 0;
  char directive[16];
  size_t len;

  struct template *tmpl = template_new();
  struct template_block *current_block = template_add_block(tpl);
  current_block->begin = s;
  current_block->type = TEMPLATE_BLOCK_LITERAL;

start_of_line:
  line = s;
  ++lineno;

body:
  if (*s == '\n') {
    ++s;
    goto start_of_line;
  }

  // find directive marker
  if (*s     != 'g') goto body;
  if (*(++s) != 'a') goto body;
  if (*(++s) != 'l') goto body;
  if (*(++s) != 'e') goto body;
  if (*(++s) != ':') goto body;
  if (*(++s) != ':') goto body;
  ++s;

  // this is a directive
  if (!ISDIRECTIVE(*s)) goto fail;
  t = s;
  do ++s;
  while (ISDIRECTIVE(*s));
  len = s - t;
  if (len >= sizeof(directive)) goto fail;
  memcpy(directive, t, len);
  directive[len] = '\0';

  ps->s = s;
  if (!strcmp(directive, "install"))
    goto directive_install;
  if (!strcmp(directive, "if"))
    goto directive_if;

  PARSE_ERR("invalid directive: %s", directive);
  goto fail;

directive_install:
directive_if:
  enum {
    IFSTATE_INIT,
    IFSTATE_NOT
  } ifstate = IFSTATE_INIT;

  cond = template_new_cond();

directive_if:
  if (*s == ' ') {
    ++s;
    goto directive_if;
  }

  if (IS_IFTOKEN(*s)) {
    char *token = s++;
    while (IS_IFTOKEN(*s)) ++s;
    if (s - token == 3 && !memcmp(token, "not", 3)) {
      ifstate = IFSTATE_NORM;
      goto directive_if;
    }
    if (s - token == 3 && !memcmp(token, "and", 3)) {
      struct template_bexpr *be = xnew(struct template_bexpr);
      be->type = TEMPLATE_BEXPR_AND;
      goto directive_if;
    }
    goto directive_if;
  }

  // consume the rest of the line
  while (*s++ != '\n');
  current_block->end = s;
  current_block = template_add_block(tpl);
  current_block->begin = s;
  current_block->type = TEMPLATE_BLOCK_COND;
  current_block->cond = cond;
  cond = NULL;
  goto start_of_line;

fail:
  template_destroy(tmpl);
  if (cond)
    template_destroy_cond(cond);
  return -1;
}

static struct template *template_load(const char *filename)
{
  struct stat st;
  char *buf = NULL;
  struct template *tmpl = NULL;

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
  fd = -1;

  // append a trailing newline if necessary
  if (buf[size - 1] != '\n')
    buf[size++] = '\n';

  tmpl = template_parse(buf, size);

finish:
  free(buf);
  return tmpl;
}

/*******************************************************************************
 * LOG
 ******************************************************************************/

static int log_load(struct state *state, const char *filename);
static int log_write(struct state *state, const char *filename);

/*******************************************************************************
 * FG
 ******************************************************************************/

struct fgspec {
  int value;
  const char *name;
  int hasarg;
};

struct fg
{
  const struct fgspec *spec;
  char **args;
  char *arg;
  char *clump;
};

static void fginit(struct fg *fg, const struct fgspec *spec, char **args)
{
  fg->spec = spec;
  fg->args = args;
  fg->clump = NULL;
}

static int fgnext(struct fg *fg)
{
  int opt;
  char *arg = fg->clump;
  if (arg)
    goto clump;
  arg = *fg->args;
  if (!arg)
    return 0;
  if (arg[0] != '-' || arg[1] == '\0')
    DIE(2, "invalid argument: '%s'", arg);
  ++fg->args;
  ++arg;
  if (*arg != '-')
    goto clump;
  ++arg;

  char *stuck = strchr(arg, '=');
  if (stuck)
    *stuck = '\0';
  for (const struct fgspec *spec = fg->spec; spec->value; ++spec) {
    if (!strcmp(spec->name, arg)) {
      if (spec->hasarg) {
        if (stuck) {
          fg->arg = stuck + 1;
        } else {
          if (!*fg->args)
            DIE(2, "option requires an argument: --%s", arg);
          fg->arg = *(fg->args++);
        }
      } else if (stuck) {
        DIE(2, "option accepts no argument: --%s", arg);
      }
      return spec->value;
    }
  }
  DIE(2, "invalid option: --%s", arg);

clump:
  opt = *arg;
  fg->clump = *(++arg) ? arg : NULL;
  for (const struct fgspec *spec = fg->spec; spec->value; ++spec) {
    if (spec->value == opt) {
      if (spec->hasarg) {
        if (fg->clump) {
          fg->arg = fg->clump;
          fg->clump = NULL;
        } else {
          if (!*fg->args)
            DIE(2, "option requires an argument: -%c", opt);
          fg->arg = *(fg->args++);
        }
      }
      return opt;
    }
  }
  DIE(2, "invalid option: -%c", opt);
}

/*******************************************************************************
 * MAIN
 ******************************************************************************/

static const char usage[] = "\
usage: galinst [options]\n\
\n\
A build system for dotfiles.\n\
\n\
Options:\n\
  -h, --help        show this help and exit\n\
  -v, --verbose     be verbose\n\
";

int main(int argc, char **argv)
{
  static const struct fgspec options[] = {
    {'h', "help", 0},
    {'v', "verbose", 0},
    {, "debug-template", 1}
    {0}
  };

  int verbose = 0;
  const char *debug_template = NULL;

  int opt;
  struct fg fg;
  fginit(&fg, options, argv + 1);
  while (opt = fgnext(&fg)) {
    switch (opt) {
      case 'h':
        fputs(usage, stderr);
        return 0;
      case 'v':
        verbose = 1;
        break;
      case 'T':
        debug_template = fg.arg;
        break;
    }
  }
}
