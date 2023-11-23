#include "common.h"
#include "optparse.h"

void optinit(
    struct optstate *s,
    int count,
    char **args)
{
  s->index = 0;
  s->count = count;
  s->args = args;
  s->shortopt = NULL;
}

static int optparse_short(struct optstate *s, const struct optspec *options)
{
  const struct optspec *o;
  for (o = options; o->value; o++) {
    if ((int)*s->shortopt == o->value) {
      s->shortopt++;
      if (o->arg) {
        if (*s->shortopt != '\0') {
          s->optarg = s->shortopt;
        } else if (s->index < s->count) {
          s->optarg = s->args[s->index++];
        } else {
          error("option requires an argument: -%c", o->value);
          exit(1);
        }
        s->shortopt = NULL;
      } else {
        if (*s->shortopt == '\0') {
          s->shortopt = NULL;
        }
      }
      return o->value;
    }
  }

  error("invalid option: -%c", (int)*s->shortopt);
  return -1;
}

int optparse(struct optstate *s, const struct optspec *options)
{
  if (s->shortopt) {
    return optparse_short(s, options);
  }

  /* End of arguments */
  if (s->index >= s->count) {
    return 0;
  }

  char *cur = s->args[s->index];

  /* Positional argument (includes -) */
  if (cur[0] != '-' || cur[1] == '\0') {
    return 0;
  }

  /* Short option */
  if (cur[1] != '-') {
    s->shortopt = cur + 1;
    s->index++;
    return optparse_short(s, options);
  }

  /* End of options (--) */
  if (cur[2] == '\0') {
    s->index++;
    return 0;
  }

  /* Long option */

  /* Loop until we find a matching long option */
  const struct optspec *o;
  for (o = options; o->value; o++) {
    if (!o->name) /* no long option */
      continue;

    char *oc = o->name;
    char *ac = cur + 2;

    for (;;) {
      if (*oc == '\0') {
        if (o->arg) {
          if (*ac == '\0') {
            error("option requires an argument: --%s", o->name);
            return -1;
          } else if (*ac == '=') {
            s->optarg = ac + 1;
          } else {
            break;
          }
        } else if (*ac == '=') {
          error("option accepts no argument: --%s", o->name);
          return -1;
        } else if (*ac != '\0') {
          break;
        }
        s->index++;
        return o->value;
      }
      if (*ac != *oc) {
        break;
      }
      oc++;
      ac++;
    }
  }

  error("invalid option: %s", cur);
  return -1;
}
