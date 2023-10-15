#include <fcntl.h> /* open */
#include <sys/stat.h> /* mkdir, stat */
#include <unistd.h>
#include "common.h"
#include "astr.h"
#include "config.h"
#include "galinst-base.h"
#include "galinst-manifest.h"
#include "uthash.h"

#define PARSE_BUFFER_SIZE 1024
#define PARSE_EOF -2

#define IS_BLANK(c) ((c) == ' ' || (c) == '\t')
#define IS_LOWER(c) ((c) >= 'a' && (c) <= 'z')
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_WORD(c) ((c) > ' ' && (c) != '\177')

#define IS_COMMAND_NAME_BEGIN_CHAR(c) IS_LOWER(c)
#define IS_COMMAND_NAME_CHAR(c) IS_LOWER(c)

#define IS_GROUP_NAME_BEGIN_CHAR(c) (IS_LOWER(c) || IS_DIGIT(c))
#define IS_GROUP_NAME_CHAR(c) (IS_LOWER(c) || IS_DIGIT(c) || (c) == '-')

struct parser
{
  struct manifest *man;
  int fd;
  unsigned int line;
  unsigned int col;
  char *cur;
  char *end;
  char buf[PARSE_BUFFER_SIZE];
  struct manifest_group *cur_grp;
  char *error;
};

FORMAT_PRINTF(2, 0)
static void set_errorvf(struct parser *p, const char *format, va_list ap)
{
  free(p->error);
  p->error = xvstrfmt(format, ap);
}

FORMAT_PRINTF(2, 3)
static void set_errorf(struct parser *p, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  set_errorvf(p, format, ap);
  va_end(ap);
}

FORMAT_PRINTF(2, 0)
static void set_syntax_errorvf(struct parser *p, const char *format, va_list ap)
{
  char *message = xvstrfmt(format, ap);
  set_errorf(p, "syntax error at line %u col %u: %s", p->line, p->col, message);
  free(message);
}

FORMAT_PRINTF(2, 3)
static void set_syntax_errorf(struct parser *p, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  set_syntax_errorvf(p, format, ap);
  va_end(ap);
}

// Creates and returns a group with the specified name. A group with this name
// must not already exist in the manifest.
static struct manifest_group *add_group(
    struct manifest *man,
    const char *name)
{
  struct manifest_group *grp = xnew(struct manifest_group);
  grp->name = xstrdup(name);
  grp->status = MANIFEST_GROUP_DISABLED;

  grp->links = NULL;
  grp->links_tail = &grp->links;

  grp->dependency_count = 0;
  grp->dependency_names = NULL;
  grp->dependencies = NULL;

  HASH_ADD_KEYPTR(hh, man->groups, grp->name, strlen(grp->name), grp);

  return grp;
}

// Returns the group with the specified name, or NULL if none was found.
static struct manifest_group *get_group(
    struct manifest *man,
    const char *name)
{
  struct manifest_group *grp;
  HASH_FIND(hh, man->groups, name, strlen(name), grp);
  return grp;
}

// Returns the entry with the specified path, or NULL if none was found. `path`
// must be a normalized path.
struct manifest_entry *manifest_get_entry(struct manifest *man, const char *path)
{
  struct manifest_entry *ent;
  HASH_FIND(hh, man->entries, path, strlen(path), ent);
  return ent;
}

// Add an entry to the manifest. An entry with the same path must not already
// exist in the manifest.
void manifest_add_entry(struct manifest *man, struct manifest_entry *ent)
{
  HASH_ADD_KEYPTR(hh, man->entries, ent->path, strlen(ent->path), ent);
}

static struct manifest_entry *need_dir(struct parser *p, const char *path)
{
  struct manifest_entry *ent = manifest_get_entry(p->man, path);

  if (ent) {
    if (ent->type == MANIFEST_ENTRY_DIR)
      return ent;

    set_errorf(p, "'%s' already declared as non-directory", path);
    return NULL;
  }

  char *pathcopy = xstrdup(path);
  char *slash = strrchr(pathcopy, '/');
  struct manifest_entry *parent = NULL;

  if (slash) {
    // Terminate `pathcopy` at the last slash. This effectively makes `pathcopy`
    // the parent directory of `path`.
    *slash = '\0';
    parent = need_dir(p, pathcopy);
    if (!parent) {
      free(pathcopy);
      return NULL;
    }
    *slash = '/';
  }

  ent = xnew(struct manifest_entry);
  ent->path = pathcopy;
  ent->type = MANIFEST_ENTRY_DIR;
  ent->oldtype = MANIFEST_ENTRY_NONE;
  ent->parent = parent;
  ent->data.dir.created = false;
  manifest_add_entry(p->man, ent);
  return ent;
}

static int create_link_entry(
    struct parser *p,
    const char *src,
    const char *dest)
{
  struct manifest_entry *ent = manifest_get_entry(p->man, dest);

  if (ent) {
    set_errorf(p, "'%s' already declared", dest);
    return -1;
  }

  char *destcopy = xstrdup(dest);
  char *slash = strrchr(destcopy, '/');
  struct manifest_entry *parent = NULL;

  if (slash) {
    // Terminate `destcopy` at the last slash. This effectively makes `destcopy`
    // the parent directory of `dest`.
    *slash = '\0';
    parent = need_dir(p, destcopy);
    if (!parent) {
      free(destcopy);
      return -1;
    }
    *slash = '/';
  }

  ent = xnew(struct manifest_entry);
  ent->path = destcopy;
  ent->type = MANIFEST_ENTRY_LINK;
  ent->oldtype = MANIFEST_ENTRY_NONE;
  ent->parent = parent;
  ent->data.link.src = xstrdup(src);
  ent->data.link.next = NULL;

  *p->cur_grp->links_tail = ent;
  p->cur_grp->links_tail = &ent->data.link.next;
  manifest_add_entry(p->man, ent);
  return 0;
}

int manifest_is_valid_group_name(const char *s)
{
  while (*s != '\0') {
    if (!IS_GROUP_NAME_CHAR(*s))
      return 0;
    ++s;
  }
  return 1;
}

static int cur_char(struct parser *p)
{
  if (p->cur)
    return *p->cur;
  else
    return PARSE_EOF;
}

static int next_char(struct parser *p)
{
  if ((++p->cur) >= p->end) {
    ssize_t size = read(p->fd, p->buf, PARSE_BUFFER_SIZE);
    if (size == -1) {
      set_errorf(p, "read error");
      return -1;
    }
    if (size == 0) {
      p->line++;
      p->col = 0;
      p->cur = NULL;
      return PARSE_EOF;
    }
    p->cur = p->buf;
    p->end = p->buf + size;
  }

  int c = *p->cur;
  if (c == '\n') {
    p->line++;
    p->col = 0;
  } else {
    p->col++;
  }
  return c;
}

static int skip_comment(struct parser *p)
{
  int c;
  for (;;) {
    if ((c = next_char(p)) == -1)
      return -1;

    if (c == PARSE_EOF)
      return 0;

    if (c == '\n') {
      if ((c = next_char(p)) == -1)
        return -1;
      return 0;
    }
  }
}

static int expect_blank(struct parser *p)
{
  int c = cur_char(p);

  if (!IS_BLANK(c)) {
    set_errorf(p, "expected whitespace");
    return -1;
  }

  for (;;) {
    if ((c = next_char(p)) == -1)
      return -1;
    if (!IS_BLANK(c))
      break;
  }

  return 0;
}

static int expect_end_of_command(struct parser *p, const char *name)
{
  int c = cur_char(p);
  for (;;) {
    if (c == PARSE_EOF)
      return -1;

    if (c == '\n') {
      if ((c = next_char(p)) == -1)
        return -1;
      return 0;
    }

    if (!IS_BLANK(c)) {
      set_syntax_errorf(p, "too many arguments for '%s' command", name);
      return -1;
    }

    if ((c = next_char(p)) == -1)
      return -1;
  }
}

static char *parse_word(struct parser *p)
{
  int c = cur_char(p);

  struct astr word_as;
  astr_init(&word_as);
  astr_append_char(&word_as, c);

  for (;;) {
    if ((c = next_char(p)) == -1) {
      astr_free(&word_as);
      return NULL;
    }
    if (!IS_WORD(c))
      break;
    astr_append_char(&word_as, c);
  }

  return astr_release(&word_as);
}

static char *parse_quoted(struct parser *p)
{
  int c;
  int escape = 0;
  struct astr as;
  astr_init(&as);

  for (;;) {
    if ((c = next_char(p)) == -1) {
      astr_free(&as);
      return NULL;
    }

    if (c == PARSE_EOF) {
      astr_free(&as);
      set_syntax_errorf(p, "expected '\"' to terminate quoted string");
      return NULL;
    }

    if (escape) {
      astr_append_char(&as, c);
      escape = 0;
    } else if (c == '\\') {
      escape = 1;
    } else if (c == '"') {
      break;
    } else {
      astr_append_char(&as, c);
    }
  }

  if ((c = next_char(p)) == -1) {
    astr_free(&as);
    return NULL;
  }

  return astr_release(&as);
}

static char *try_parse_normalized_path(struct parser *p)
{
  int c = cur_char(p);
  char *path;

  if (c == '"') {
    path = parse_quoted(p);
  } else if (IS_WORD(c)) {
    path = parse_word(p);
  } else {
    set_syntax_errorf(p, "expected normalized path");
    return NULL;
  }

  if (!path)
    return NULL;

  if (!is_normalized_path(path)) {
    set_errorf(p, "'%s' is not a normalized path", path);
    free(path);
    return NULL;
  }

  return path;
}

static char *try_parse_group_name(struct parser *p)
{
  if (!IS_WORD(cur_char(p))) {
    set_syntax_errorf(p, "expected group name");
    return NULL;
  }

  char *name = parse_word(p);

  if (!name)
    return NULL;

  if (!manifest_is_valid_group_name(name)) {
    set_errorf(p, "'%s' is not a valid group name", name);
    free(name);
    return NULL;
  }

  return name;
}

// Parse a group header. The parser should point to the '[' that begins a group.
// Returns 0 on success and -1 on error.
static int parse_group_header(struct parser *p)
{
  int c;

  if ((c = next_char(p)) == -1) {
    return -1;
  }

  if (!IS_GROUP_NAME_BEGIN_CHAR(c)) {
    set_syntax_errorf(p, "expected group name");
    return -1;
  }

  struct astr name_as;
  astr_init(&name_as);
  astr_append_char(&name_as, c);

  for (;;) {
    if ((c = next_char(p)) == -1) {
      astr_free(&name_as);
      return -1;
    }
    if (c == ']') {
      break;
    }
    if (!IS_GROUP_NAME_CHAR(c)) {
      set_syntax_errorf(p, "expected valid group name or ']'");
      astr_free(&name_as);
      return -1;
    }
    astr_append_char(&name_as, c);
  }

  if ((c = next_char(p)) == -1) {
    astr_free(&name_as);
    return -1;
  }

  char *name = astr_release(&name_as);
  if (get_group(p->man, name)) {
    set_errorf(p, "multiple declarations of group '%s'", name);
    free(name);
    return -1;
  }

  p->cur_grp = add_group(p->man, name);

  return 0;
}

static int parse_link_command(struct parser *p)
{
  if (expect_blank(p))
    return -1;

  char *src = try_parse_normalized_path(p);
  if (!src) {
    return -1;
  }

  if (expect_blank(p)) {
    free(src);
    return -1;
  }

  char *dest = try_parse_normalized_path(p);
  if (!dest) {
    free(src);
    return -1;
  }

  if (expect_end_of_command(p, "link")) {
    free(src);
    free(dest);
    return -1;
  }

  int err = create_link_entry(p, src, dest);
  free(src);
  free(dest);
  return err;
}

static int parse_linkbin_command(struct parser *p)
{
  if (expect_blank(p))
    return -1;

  char *src = try_parse_normalized_path(p);
  if (!src) {
    return -1;
  }

  if (expect_end_of_command(p, "linkbin")) {
    free(src);
    return -1;
  }

  char *base = strrchr(src, '/');
  base = base ? base + 1 : src;

  char *dest = xstrfmt(".local/bin/%s", base);

  int err = create_link_entry(p, src, dest);
  free(src);
  free(dest);
  return err;
}

static int parse_require_command(struct parser *p)
{
  if (expect_blank(p))
    return -1;

  char *dep = try_parse_group_name(p);
  if (!dep)
    return -1;

  if (expect_end_of_command(p, "require")) {
    free(dep);
    return -1;
  }

  struct manifest_group *grp = p->cur_grp;
  size_t dc = ++grp->dependency_count;
  grp->dependency_names = xrealloc(grp->dependency_names, dc * sizeof(char *));
  grp->dependency_names[dc - 1] = dep;
  return 0;
}

static int parse_default_command(struct parser *p)
{
  if (expect_blank(p))
    return -1;

  int c = cur_char(p);
  if (!IS_WORD(c)) {
    set_syntax_errorf(p, "expected 'enabled' or 'disabled'");
    return -1;
  }

  char *status_str = parse_word(p);
  if (!status_str)
    return -1;

  if (!strcmp(status_str, "enabled"))
    p->cur_grp->status = MANIFEST_GROUP_ENABLED;
  else if (!strcmp(status_str, "disabled"))
    p->cur_grp->status = MANIFEST_GROUP_DISABLED;
  else {
    set_syntax_errorf(p, "expected 'enabled' or 'disabled'");
    return -1;
  }

  if (expect_end_of_command(p, "default")) {
    free(status_str);
    return -1;
  }

  return 0;
}

// Parse a command. The parser should point to the first character of the
// command name, which must be a character for which
// IS_COMMAND_NAME_BEGIN_CHAR() returns true.
//
// Returns 0 on success and -1 on error.
static int parse_command(struct parser *p)
{
  int c = cur_char(p);

  struct astr name_as;
  astr_init(&name_as);
  astr_append_char(&name_as, c);

  for (;;) {
    if ((c = next_char(p)) == -1) {
      astr_free(&name_as);
      return -1;
    }

    if (!IS_COMMAND_NAME_CHAR(c))
      break;

    astr_append_char(&name_as, c);
  }

  char *name = astr_release(&name_as);
  int err;
  if (!strcmp(name, "link"))
    err = parse_link_command(p);
  else if (!strcmp(name, "linkbin"))
    err = parse_linkbin_command(p);
  else if (!strcmp(name, "require"))
    err = parse_require_command(p);
  else if (!strcmp(name, "default"))
    err = parse_default_command(p);
  else {
    set_errorf(p, "invalid command '%s'", name);
    err = -1;
  }
  free(name);
  return err;
}

// Parse the manifest from the specified file descriptor.
// Returns 0 on success and -1 on error.
static int parse_manifest(struct manifest *man, int fd)
{
  struct parser p;
  p.man = man;
  p.fd = fd;
  p.line = 1U;
  p.col = 1U;
  p.cur = p.buf;
  p.end = p.buf + 1;
  p.cur_grp = NULL;
  p.error = NULL;

  man->groups = NULL;
  man->entries = NULL;
  man->error = NULL;

  int c;
  if ((c = next_char(&p)) == -1)
    goto fail;

  for (;;) {
    c = cur_char(&p);

    if (c == PARSE_EOF)
      break;

    if (c == '#') {
      if (skip_comment(&p))
        goto fail;
    } else if (c == '[') {
      if (parse_group_header(&p))
        goto fail;
    } else if (IS_BLANK(c) || (c) == '\n') {
      if (next_char(&p) == -1)
        goto fail;
    } else if (IS_COMMAND_NAME_BEGIN_CHAR(c)) {
      if (parse_command(&p))
        goto fail;
    } else {
      set_syntax_errorf(&p, "expected '#', '[', or command name");
      goto fail;
    }
  }

  struct manifest_group *grp;
  for (grp = man->groups; grp; grp = grp->hh.next) {
    grp->dependencies = xalloc(grp->dependency_count * sizeof(struct manifest_group *));
    for (size_t i = 0; i < grp->dependency_count; i++) {
      const char *dep_name = grp->dependency_names[i];
      struct manifest_group *dep = get_group(man, dep_name);
      if (!dep) {
        set_errorf(&p, "failed to resolve dependency '%s' of '%s'", dep_name, grp->name);
        goto fail;
      }
      grp->dependencies[i] = dep;
    }
  }

  return 0;

fail:
  manifest_free(man);
  man->error = p.error;
  return -1;
}

// Load the manifest from the specified file.
// Returns 0 on success and nonzero on error.
int manifest_load(struct manifest *man, const char *filename)
{
  GALE_TRACE("loading manifest from '%s'", filename);

  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    man->error = xstrfmt("failed to open '%s'", filename);
    return -1;
  }

  int err = parse_manifest(man, fd);
  close(fd);
  return err;
}

void manifest_print(struct manifest *man)
{
  struct manifest_group *grp;

  for (grp = man->groups; grp; grp = grp->hh.next) {
    const char *statstr;
    switch (grp->status) {
      case MANIFEST_GROUP_DISABLED: statstr = "disabled"; break;
      case MANIFEST_GROUP_ENABLED: statstr = "enabled"; break;
      case MANIFEST_GROUP_ACTIVE: statstr = "active"; break;
      default: statstr = "unknown";
    }

    printf("group %s (%s)\n", grp->name, statstr);

    if (grp->dependency_count > 0) {
      printf("  requires %s", grp->dependencies[0]->name);
      for (size_t i = 1; i < grp->dependency_count; i++)
        printf(", %s", grp->dependencies[i]->name);
      printf("\n");
    }

    struct manifest_entry *ent;
    for (ent = grp->links; ent; ent = ent->data.link.next) {
      printf("  link %s -> %s\n", ent->path, ent->data.link.src);
    }
  }
}

/*
// Delete the file at `dest` and any newly empty parent directories.
UNUSED static int delete_link(const char *dest)
{
  if (unlink(dest) == -1) {
    galinst_error_sys("failed to delete '%s'", dest);
    return -1;
  }

  char *dir = normalized_dirname(dest);
  while (dir) {
    if (rmdir(dir) == -1) {
      if (errno == ENOTEMPTY || errno == EEXIST) {
        free(dir);
        return 0;
      }
      galinst_error_sys("failed to rmdir '%s'", dir);
      free(dir);
      return -1;
    }
    char *parent = normalized_dirname(dir);
    free(dir);
    dir = parent;
  }

  return 0;
}
*/

void activate_group(struct manifest *man, struct manifest_group *grp)
{
  if (grp->status == MANIFEST_GROUP_ACTIVE)
    return;

  grp->status = MANIFEST_GROUP_ACTIVE;

  // GALE_TRACE("manifest_apply_config: activated %s", grp->name);

  for (size_t i = 0; i < grp->dependency_count; i++) {
    activate_group(man, grp->dependencies[i]);
  }
}

void manifest_apply_config(struct manifest *man, struct config *conf)
{
  struct manifest_group *grp;
  for (grp = man->groups; grp; grp = grp->hh.next) {
    if (grp->status == MANIFEST_GROUP_ACTIVE)
      continue;

    char *key = xstrfmt("install.%s", grp->name);
    char *value = config_get(conf, key);
    if (value) {
      if (!strcmp(value, "yes")) {
        grp->status = MANIFEST_GROUP_ENABLED;
      } else if (!strcmp(value, "no")) {
        grp->status = MANIFEST_GROUP_DISABLED;
      } else {
        galinst_warn("expected 'yes' or 'no' for key '%s'", key);
      }
    }
    if (grp->status == MANIFEST_GROUP_ENABLED) {
      activate_group(man, grp);
    }
    free(key);
  }
}

static int create_parent_directories(const char *path)
{
  char *parent = get_dirname_null(path);
  if (!parent)
    return 0;

  if (mkdir(parent, 0777) == 0) {
    galinst_info("created directory: %s", parent);
    goto success;
  }

  if (errno != ENOENT) {
    galinst_error("failed to mkdir '%s'", parent);
    goto fail;
  }

  if (create_parent_directories(parent)) {
    goto fail;
  }

  if (mkdir(parent, 0777) == 0) {
    galinst_info("created directory: %s", parent);
    goto success;
  }

  galinst_error("failed to mkdir '%s'", parent);

fail:
  free(parent);
  return -1;

success:
  free(parent);
  return 0;
}

// Returns a string which should be placed in a symbolic link at `dest` so that
// it points to `src` in the .gale directory. `src` and `dest` must be
// normalized paths.
//
// The returned pointer should be released using free().
static char *get_link_target(const char *src, const char *dest)
{
  int backs = 0;
  for (const char *dc = dest; *dc != '\0'; dc++)
    if (*dc == '/')
      ++backs;

  size_t src_len = strlen(src);
  char *target = xalloc(3 * backs + 7 + src_len + 1);
  char *tp = target;
  while (backs) {
    *(tp++) = '.';
    *(tp++) = '.';
    *(tp++) = '/';
    --backs;
  }
  memcpy(tp, ".gale/", 6);
  tp += 6;
  memcpy(tp, src, src_len + 1);

  return target;
}

static int prune_link(const char *path, bool dry)
{
  if (dry) {
    galinst_info("[dry] prune: remove: %s", path);
    return 0;
  }

  struct stat st;

  if (lstat(path, &st)) {
    // If the link doesn't exist, we don't count this as an error and return
    // zero to indicate that we should still try to remove parent directories.
    if (errno == ENOENT)
      return 0;

    galinst_warn_sys("prune: failed to stat '%s'", path);
    return -1;
  }

  if (!S_ISLNK(st.st_mode)) {
    galinst_info("prune: not a symlink, skipping: %s", path);
    return -1;
  }

  // If the file is a symbolic link, delete it.

  galinst_info("prune: removing: %s", path);

  if (unlink(path) == 0 || errno == ENOENT)
    return 0;

  galinst_warn_sys("prune: failed to unlink '%s'", path);
  return -1;
}

static int prune_dir(const char *path, bool dry)
{
  if (dry) {
    galinst_info("[dry] prune: remove directory: %s", path);
    return 0;
  }

  galinst_info("prune: removing directory: %s", path);

  if (rmdir(path) == 0 || errno == ENOENT)
    return 0;

  if (errno == ENOTEMPTY || errno == EEXIST)
    return -1;

  galinst_warn_sys("prune: failed to rmdir '%s'", path);
  return -1;
}

static int install_dir(struct manifest_entry *ent, bool dry)
{
  GALE_ASSERT(ent->type == MANIFEST_ENTRY_DIR);

  if (ent->data.dir.created)
    return 0;

  if (dry) {
    galinst_info("[dry] creating directory: %s", ent->path);
  } else {
    galinst_info("creating directory: %s", ent->path);

    if (mkdir(ent->path, 0755) == -1) {
      if (errno == ENOENT) {
        if (!ent->parent) {
          galinst_error("mkdir '%s' failed with ENOENT", ent->path);
          return -1;
        }
        if (install_dir(ent->parent, dry))
          return -1;
      }

      galinst_error("failed to mkdir '%s'", ent->path);
      return -1;
    }
  }

  ent->data.dir.created = true;
  return 0;
}

enum {
  LINK_SKIPPED,
  LINK_INSTALLED,
  LINK_FAILED
};

static int install_link_helper(
    const char *target,
    const char *dest,
    struct manifest_entry *parent,
    bool dry)
{
  size_t target_len = strlen(target);
  char *buf = xalloc(target_len + 1);

  // Read the symlink at dest.
  // Allow an extra byte to detect overflow.
  ssize_t len = readlink(dest, buf, target_len + 1);

  if (len >= 0) {

    // File exists. Check to see if it has the right target.

    if (len == target_len && !memcmp(buf, target, target_len)) {
      // The target matches. Skip it.
      free(buf);
      galinst_verbose("skipping: %s", dest);
      return LINK_SKIPPED;
    }

    free(buf);

    // The target doesn't match. Replace it.

    if (dry) {
      galinst_info("[dry] replacing symlink: %s -> %s", dest, target);
      return LINK_INSTALLED;
    }

    galinst_info("replacing symlink: %s -> %s", dest, target);

    if (unlink(dest) == -1) {
      galinst_error_sys("failed to unlink '%s'", dest);
      return LINK_FAILED;
    }

    if (symlink(target, dest) == -1) {
      galinst_error_sys("failed to create symlink '%s'", dest);
      return LINK_FAILED;
    }

    return LINK_INSTALLED;
  }

  free(buf);

  if (errno == EINVAL) {
    // Not a symbolic link
    galinst_error_sys("refused to overwrite '%s'", dest);
    return LINK_FAILED;
  }

  if (errno != ENOENT) {
    galinst_error_sys("failed to readlink '%s'", dest);
    return LINK_FAILED;
  }

  // Symlink doesn't exist. Try creating it.

  if (dry) {
    galinst_info("[dry] creating symlink: %s -> %s", dest, target);
    return LINK_INSTALLED;
  }

  galinst_info("creating symlink: %s -> %s", dest, target);

  if (symlink(target, dest) == 0) {
    return LINK_INSTALLED;
  }

  if (errno != ENOENT) {
    galinst_error_sys("failed to symlink '%s'", dest);
    return LINK_FAILED;
  }

  // Parent directory doesn't exist. Try creating it.
  if (install_dir(parent, dry))
    return LINK_FAILED;

  // Try creating the symlink again.
  if (symlink(target, dest) == 0)
    return LINK_INSTALLED;

  // Give up.
  galinst_error_sys("failed to symlink '%s'", dest);
  return LINK_FAILED;
}

static int install_link(struct manifest_entry *ent, bool dry)
{
  GALE_ASSERT(ent->type == MANIFEST_ENTRY_LINK);

  char *target = get_link_target(ent->data.link.src, ent->path);
  int result = install_link_helper(target, ent->path, ent->parent, dry);
  free(target);
  return result;
}

void manifest_install(struct manifest *man, bool dry)
{
  // Prune old links stored in log

  struct manifest_entry *ent;
  struct manifest_entry *dir;

  for (ent = man->oldlinks; ent; ent = ent->old.link.next) {
    GALE_ASSERT(ent->oldtype == MANIFEST_ENTRY_LINK);

    if (ent->type == MANIFEST_ENTRY_LINK)
      continue;

    if (prune_link(ent->path, dry))
      continue;

    for (dir = ent->parent; dir; dir = dir->parent) {
      GALE_ASSERT(dir->oldtype == MANIFEST_ENTRY_DIR);

      if (dir->type == MANIFEST_ENTRY_DIR)
        break;

      if ((--dir->old.dir.refcount) > 0)
        break;

      if (prune_dir(dir->path, dry))
        break;
    }
  }

  // Install

  struct manifest_group *grp;
  bool nothing_to_do = true;

  for (grp = man->groups; grp; grp = grp->hh.next) {

    // Skip inactive groups
    if (grp->status != MANIFEST_GROUP_ACTIVE)
      continue;

    galinst_verbose("checking group: %s", grp->name);

    for (ent = grp->links; ent; ent = ent->data.link.next) {
      if (ent->oldtype != MANIFEST_ENTRY_LINK)
        man->need_update_log = true;

      if (install_link(ent, dry) != LINK_SKIPPED)
        nothing_to_do = false;
    }
  }

  if (nothing_to_do)
    galinst_info("nothing to do");
}

int manifest_write_log(struct manifest *man, const char *filename)
{
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    if (errno != ENOENT) {
      galinst_error_sys("failed to open '%s'", filename);
      return -1;
    }

    if (create_parent_directories(filename))
      return -1;

    fp = fopen(filename, "w");
    if (!fp) {
      galinst_error_sys("failed to open '%s'", filename);
      return -1;
    }
  }

  struct manifest_group *grp;
  struct manifest_entry *ent;

  for (grp = man->groups; grp; grp = grp->hh.next) {

    // Ignore inactive groups
    if (grp->status != MANIFEST_GROUP_ACTIVE)
      continue;

    for (ent = grp->links; ent; ent = ent->data.link.next) {
      if (fprintf(fp, "%s\n", ent->path) < 0) {
        galinst_error_sys("write error");
        fclose(fp);
        return -1;
      }
    }
  }

  fclose(fp);

  return 0;
}

void manifest_free(struct manifest *man)
{
  struct manifest_group *grp, *grp_next;
  struct manifest_entry *ent, *ent_next;

  for (grp = man->groups; grp; grp = grp_next) {
    grp_next = grp->hh.next;

    free(grp->name);
    free(grp->dependencies);
    free(grp);
  }

  for (ent = man->entries; ent; ent = ent_next) {
    ent_next = ent->hh.next;

    free(ent->path);

    if (ent->type == MANIFEST_ENTRY_LINK) {
      free(ent->data.link.src);
    }

    free(ent);
  }
}
