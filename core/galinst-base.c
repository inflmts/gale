#include "galinst-base.h"

int loglevel = LOGLEVEL_INFO;

void _galinst_info(const char *format, ...)
{
  va_list ap;
  fputs("galinst: ", stderr);
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputs("\n", stderr);
}

int is_normalized_path(const char *path)
{
  // States:
  //   0 = start of component
  //   1 = after dot (.)
  //   2 = after dot-dot (..)
  //   3 = anywhere else
  //
  int state = 0;
  while (*path != '\0') {
    if (*path == '/') {
      if (state != 3) {
        return 0;
      }
      state = 0;
    } else if (*path == '.') {
      if (state < 3) {
        ++state;
      }
    } else {
      state = 3;
    }
    ++path;
  }
  return state == 3;
}
