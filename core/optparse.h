/*
 * optparse.h - option parsing utilities
 */
#ifndef OPTPARSE_H
#define OPTPARSE_H

#include <stdbool.h>

struct optspec
{
  /* Option value. Must be a positive integer. If the value is between 1-255,
   * this will be used as the short option character. */
  int value;
  /* Long option name. NULL indicates no long option. */
  char *name;
  /* If true, the option takes an argument. */
  bool arg;
};

struct optstate
{
  int index;
  int count;
  char **args;
  char *optarg;
  char *shortopt;
};

/* Initialize the option parser. `count` specifies the number of arguments, and
 * `args` is a pointer to an array starting with the first argument.
 */
void optinit(
    struct optstate *s,
    int count,
    char **args);

/* Returns the value of the next option. `options` points to an array of
 * recognized options, terminated by an optspec with value 0.
 *
 * If an invalid option or missing argument is encountered, an error message is
 * printed and optparse() returns -1. If no more options remain, returns 0. If
 * the option requires an argument, `s->optarg` will be set to the option
 * argument.
 */
int optparse(struct optstate *s, const struct optspec *options);

#endif
