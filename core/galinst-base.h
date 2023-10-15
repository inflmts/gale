#ifndef GALINST_BASE_H
#define GALINST_BASE_H

#include "common.h"

#define galinst_error(...) error(__VA_ARGS__)
#define galinst_error_sys(...) error_sys(__VA_ARGS__)
#define galinst_warn(...) warn(__VA_ARGS__)
#define galinst_warn_sys(...) warn_sys(__VA_ARGS__)

#define LOGLEVEL_NONE 0
#define LOGLEVEL_INFO 1
#define LOGLEVEL_VERBOSE 2

extern int loglevel;

void _galinst_info(const char *format, ...) FORMAT_PRINTF(1, 2);
#define galinst_info(...) \
  do { if (loglevel >= LOGLEVEL_INFO) _galinst_info(__VA_ARGS__); } while (0)
#define galinst_verbose(...) \
  do { if (loglevel >= LOGLEVEL_VERBOSE) _galinst_info(__VA_ARGS__); } while (0)

// True if colors are enabled
extern bool color_output;

// Returns true if the argument is a normalized path. A normalized path:
//
//   1. Cannot begin or end with a slash.
//   2. Cannot contain any repeated slashes.
//   3. Cannot contain any '.' or '..' components.
//
int is_normalized_path(const char *s);

#endif
