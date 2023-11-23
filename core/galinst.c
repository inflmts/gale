/*
 * galinst - Gale installer
 */
#include <unistd.h>
#include "common.h"
#include "config.h"
#include "optparse.h"
#include "galinst-base.h"
#include "galinst-manifest.h"

static const char *usage =
   "usage: galinst [<option>...]\n"
   "\n"
   "Options:\n"
   "  -n, --dry-run       don't do anything, only show what would happen\n"
   "  -v, --verbose       be verbose\n"
   "  -q, --quiet         suppress informational messages\n"
   "  -h, --help          show this help\n";

bool color_output = false;

bool detect_color_support(void)
{
  const char *term = getenv("TERM");
  if (!term)
    return false;
  if (!strcmp(term, "dumb"))
    return false;
  return true;
}

#define OPT_DRY 'n'
#define OPT_VERBOSE 'v'
#define OPT_QUIET 'q'
#define OPT_HELP 'h'

int main(int argc, char **argv)
{
  gale_init_trace();

  if (argc > 0) {
    argc--;
    argv++;
  }

  static const struct optspec options[] = {
    { OPT_DRY, "dry-run", 0 },
    { OPT_VERBOSE, "verbose", 0 },
    { OPT_QUIET, "quiet", 0 },
    { OPT_HELP, "help", 0 },
    {0}
  };

  int opt;
  struct optstate s;
  optinit(&s, argc, argv);

  bool dry = false;

  while ((opt = optparse(&s, options)) != 0) {
    switch (opt) {
      case OPT_DRY:
        dry = true;
        break;
      case OPT_VERBOSE:
        loglevel = LOGLEVEL_VERBOSE;
        break;
      case OPT_QUIET:
        loglevel = LOGLEVEL_NONE;
        break;
      case OPT_HELP:
        fputs(usage, stderr);
        exit(0);
      default:
        exit(1);
    }
  }

  const char *home_dir = getenv("HOME");

  if (!home_dir) {
    galinst_error("$HOME is not set");
    exit(1);
  }

  GALE_TRACE("entering directory '%s'", home_dir);

  if (chdir(home_dir) == -1) {
    galinst_error_sys("failed to chdir to '%s'", home_dir);
    exit(1);
  }

  static const char *man_filename = ".gale/galinst.conf";
  static const char *config_filename = ".config/gale/config";
  static const char *log_filename = ".data/gale/galinst.log";

  char *errmsg;

  struct manifest man;
  if (manifest_load(&man, man_filename)) {
    error("failed to load manifest: %s", man.error);
    exit(2);
  }

  struct config conf;
  if (config_load(&conf, config_filename, &errmsg)) {
    error("failed to load config: %s", errmsg);
    exit(2);
  }

  manifest_apply_config(&man, &conf);

  config_free(&conf);

  if (manifest_load_log(&man, log_filename)) {
    galinst_error("failed to load log: %s", man.error);
    manifest_free(&man);
    free(man.error);
    exit(2);
  }

  manifest_install(&man, dry);

  if (!dry && man.need_update_log) {
    GALE_TRACE("writing updated log to '%s'", log_filename);
    if (manifest_write_log(&man, log_filename)) {
      manifest_free(&man);
      exit(2);
    }
  }

  manifest_free(&man);

  return 0;
}
