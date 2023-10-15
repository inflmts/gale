/*
 * galconf - Gale configuration utility
 */
#include <fcntl.h> /* open */
#include <sys/stat.h> /* mkdir */
#include <unistd.h>
#include "common.h"
#include "astr.h"
#include "config.h"

static char *get_config_path(void)
{
  const char *path;

  /*
  path = getenv("XDG_CONFIG_HOME");
  if (path && *path)
    return xstrfmt("%s/gale/config", path);
    */

  path = getenv("HOME");
  if (path && *path)
    return xstrfmt("%s/.config/gale/config", path);

  fatal("$HOME is not set");
}

static void load_config(struct config *conf, const char *path)
{
  char *errmsg;
  if (config_load(conf, path, &errmsg)) {
    error("failed to load config: %s", errmsg);
    exit(2);
  }
}

static int create_parent_directories(const char *path)
{
  char *parent = get_dirname(path);
  int err = 0;
  if (mkdir(parent, 0755)) {
    if (errno == ENOENT) {
      err = create_parent_directories(parent);
    } else {
      error_sys("failed to create '%s'", parent);
      err = -1;
    }
  }
  free(parent);
  return err;
}

static void write_config(struct config *conf, const char *path)
{
  int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
  if (fd == -1) {
    if (errno != ENOENT) {
      error_sys("failed to open '%s'", path);
      exit(2);
    }

    if (create_parent_directories(path))
      exit(2);

    fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd == -1) {
      error_sys("failed to open '%s'", path);
      exit(2);
    }
  }

  FILE *stream = fdopen(fd, "w");
  if (!stream) {
    error_sys("fdopen");
    exit(2);
  }

  struct config_entry *ent;
  for (ent = conf->head; ent; ent = config_entry_next(ent)) {
    if (fprintf(stream, "%s=%s\n", ent->key, ent->value) < 0) {
      error_sys("write error");
      exit(2);
    }
  }

  fclose(stream);
}

void usage(FILE *stream)
{
  fprintf(stream, "\
usage: galconf <command> [<argument>...]\n\
commands:\n\
  list\n\
  set <key>=<value>...\n\
  unset <key>...\n\
  test <key>... (not implemented)\n\
  get [--str|--bool] [--default=<value>] <key>\n\
  wipe (not implemented)\n\
options:\n\
  -h, --help            show this help\n\
" );
}

NORETURN void help(void)
{
  usage(stdout);
  exit(0);
}

/*****************************************************************************
 * COMMANDS
 ****************************************************************************/

#define DECLARE_COMMAND(name) static void command_##name (int argc, char **argv)

DECLARE_COMMAND(list)
{
  enum {
    OPT_HELP = 'h'
  };

  static const struct optspec options[] = {
    { OPT_HELP, "help", false },
    {0}
  };

  int opt;
  struct optstate s;
  optinit(&s, options, argc, argv);

  while ((opt = optparse(&s)) != -1) {
    switch (opt) {
      case OPT_HELP:
        help();
    }
  }

  if (s.index < argc) {
    error("list accepts no arguments");
    exit(1);
  }

  char *path = get_config_path();
  GALE_TRACE("config path: %s", path);

  struct config conf;
  load_config(&conf, path);

  struct config_entry *ent;
  for (ent = conf.head; ent; ent = config_entry_next(ent)) {
    printf("%s=%s\n", ent->key, ent->value);
  }

  config_free(&conf);
}

struct config_edit
{
  char *key;
  char *value;
};

DECLARE_COMMAND(set)
{
  enum {
    OPT_HELP = 'h'
  };

  static const struct optspec options[] = {
    { OPT_HELP, "help", false },
    {0}
  };

  int opt;
  struct optstate s;
  optinit(&s, options, argc, argv);

  while ((opt = optparse(&s)) != -1) {
    switch (opt) {
      case OPT_HELP:
        help();
    }
  }

  if (s.index >= argc)
    return;

  int edit_count = 0;
  struct config_edit *edits = xalloc((argc - s.index) * sizeof(struct config_edit));

  for (int i = s.index; i < argc; i++) {
    char *arg = argv[i];
    char *eq = strchr(arg, '=');

    if (!eq) {
      error("invalid argument: %s", arg);
      exit(2);
    }

    size_t key_len = eq - arg;
    char *key = xalloc(key_len + 1);
    memcpy(key, arg, key_len);
    key[key_len] = '\0';

    if (!config_is_valid_key(key)) {
      error("invalid key: %s", key);
      exit(2);
    }

    char *value = xstrdup(eq + 1);

    edits[edit_count].key = key;
    edits[edit_count].value = value;
    edit_count++;
  }

  char *path = get_config_path();
  GALE_TRACE("config path: %s", path);
  GALE_TRACE("applying %d edits", edit_count);

  struct config conf;
  load_config(&conf, path);

  for (int i = 0; i < edit_count; i++) {
    config_set(&conf, edits[i].key, edits[i].value);
    free(edits[i].key);
    free(edits[i].value);
  }
  free(edits);

  write_config(&conf, path);
  config_free(&conf);
  free(path);
}

DECLARE_COMMAND(unset)
{
  enum {
    OPT_HELP = 'h'
  };

  static const struct optspec options[] = {
    { OPT_HELP, "help", false },
    {0}
  };

  int opt;
  struct optstate s;
  optinit(&s, options, argc, argv);

  while ((opt = optparse(&s)) != -1) {
    switch (opt) {
      case OPT_HELP:
        help();
    }
  }

  // If no arguments were provided, exit now
  if (s.index == argc)
    return;

  // Check arguments
  for (int i = s.index; i < argc; i++) {
    if (!config_is_valid_key(argv[i])) {
      error("invalid key: %s", argv[i]);
      exit(1);
    }
  }

  char *path = get_config_path();
  GALE_TRACE("config path: %s", path);

  struct config conf;
  load_config(&conf, path);

  bool changed = false;
  for (int i = s.index; i < argc; i++)
    if (config_unset(&conf, argv[i]))
      changed = true;

  if (changed)
    write_config(&conf, path);

  free(path);
}

DECLARE_COMMAND(test)
{
  enum {
    OPT_HELP = 'h'
  };

  static const struct optspec options[] = {
    { OPT_HELP, "help", false },
    {0}
  };

  int opt;
  struct optstate s;
  optinit(&s, options, argc, argv);

  while ((opt = optparse(&s)) != -1) {
    switch (opt) {
      case OPT_HELP:
        help();
    }
  }

  fatal("not implemented");
}

DECLARE_COMMAND(get)
{
  enum {
    OPT_HELP = 'h',
    OPT_STR = 1000,
    OPT_BOOL,
    OPT_DEFAULT
  };

  enum {
    VT_STR,
    VT_BOOL
  };

  static const struct optspec options[] = {
    { OPT_HELP, "help", false },
    { OPT_STR, "str", false },
    { OPT_BOOL, "bool", false },
    { OPT_DEFAULT, "default", true },
    {0}
  };

  int opt;
  struct optstate s;
  optinit(&s, options, argc, argv);

  int type = VT_STR;
  const char *default_value = NULL;

  while ((opt = optparse(&s)) != -1) {
    switch (opt) {
      case OPT_HELP:
        help();
      case OPT_STR:
        type = VT_STR;
        break;
      case OPT_BOOL:
        type = VT_BOOL;
        break;
      case OPT_DEFAULT:
        default_value = s.optarg;
        break;
    }
  }

  if ((argc - s.index) != 1) {
    error("expected one argument");
    exit(1);
  }

  const char *key = argv[s.index];

  if (!config_is_valid_key(key)) {
    error("invalid key: %s", key);
    exit(1);
  }

  char *path = get_config_path();
  GALE_TRACE("config path: %s", path);

  struct config conf;
  load_config(&conf, path);

  const char *value = config_get(&conf, key);

  if (value) {
    switch (type) {
      case VT_BOOL:
        if (strcmp(value, "yes") && strcmp(value, "no"))
          value = NULL;
        break;
    }
  }

  if (!value)
    value = default_value;

  if (value)
    printf("%s\n", value);

  config_free(&conf);

  if (!value)
    exit(1);
}

DECLARE_COMMAND(wipe)
{
  enum {
    OPT_HELP = 'h'
  };

  static const struct optspec options[] = {
    { OPT_HELP, "help", false },
    {0}
  };

  int opt;
  struct optstate s;
  optinit(&s, options, argc, argv);

  while ((opt = optparse(&s)) != -1) {
    switch (opt) {
      case OPT_HELP:
        help();
    }
  }

  fatal("not implemented");
}

DECLARE_COMMAND(help)
{
  help();
}

struct command
{
  char *name;
  void (*func)(int argc, char **argv);
};

int main(int argc, char **argv)
{
  gale_init_trace();

  if (argc < 2) {
    usage(stderr);
    exit(1);
  }

  const char *command_name = argv[1];
  argc -= 2;
  argv += 2;

  static const struct command commands[] = {
    { "list", command_list },
    { "get", command_get },
    { "set", command_set },
    { "unset", command_unset },
    { "test", command_test },
    { "wipe", command_wipe },
    { "help", command_help },
    { "--help", command_help },
    { NULL, NULL }
  };

  for (const struct command *c = commands; c->name; c++) {
    if (!strcmp(c->name, command_name)) {
      c->func(argc, argv);
      exit(0);
    }
  }

  error("invalid command: %s", command_name);
  exit(1);
}
