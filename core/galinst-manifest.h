#ifndef GALINST_MANIFEST_H
#define GALINST_MANIFEST_H

#include "common.h"
#include "config.h"
#include "uthash-handle.h"

enum {
  MANIFEST_ENTRY_NONE,
  MANIFEST_ENTRY_LINK,
  MANIFEST_ENTRY_DIR
};

struct manifest_entry
{
  char *path;
  int type;
  int oldtype;
  struct manifest_entry *parent;

  union {
    struct {
      char *src;
      struct manifest_entry *next;
    } link;
    struct {
      bool created;
    } dir;
  } data;

  union {
    struct {
      struct manifest_entry *next;
    } link;
    struct {
      int refcount;
    } dir;
  } old;

  UT_hash_handle hh;
};

enum {
  MANIFEST_GROUP_DISABLED,
  MANIFEST_GROUP_ENABLED,
  MANIFEST_GROUP_ACTIVE
};

struct manifest_group
{
  char *name;
  int status;

  struct manifest_entry *links;
  struct manifest_entry **links_tail;

  size_t dependency_count;
  char **dependency_names;
  struct manifest_group **dependencies;

  UT_hash_handle hh;
};

struct manifest
{
  struct manifest_group *groups;
  struct manifest_entry *entries;
  struct manifest_entry *oldlinks;
  struct manifest_entry **oldlinks_tail;
  bool need_update_log;
  char *error;
};

// Returns true if the argument is a valid group name. A valid group name:
//
//   1. Must begin with a lowercase letter or digit.
//   2. Must contain only lowercase letters, digits, or dashes.
//
int manifest_is_valid_group_name(const char *s);

// Load the manifest from the specified file.
//
// Returns 0 on success. On error, -1 is returned and man->error is set to the
// error message.
//
int manifest_load(struct manifest *man, const char *filename);

void manifest_init_log(struct manifest *man);

// Load the log from the specified file.
//
// Returns 0 on success. On error, -1 is returned and man->error is set to the
// error message.
//
// (Implemented in galinst-log.c)
//
int manifest_load_log(struct manifest *man, const char *filename);

int manifest_write_log(struct manifest *man, const char *filename);

// Get an entry in the manifest. `path` must be a normalized path.
//
// Returns the entry with the specified path or NULL if none exists.
//
struct manifest_entry *manifest_get_entry(struct manifest *man, const char *path);

void manifest_add_entry(struct manifest *man, struct manifest_entry *ent);

void manifest_print(struct manifest *man);

void manifest_apply_config(struct manifest *man, struct config *conf);

void manifest_install(struct manifest *man, bool dry);

// Release memory used by a manifest.
//
void manifest_free(struct manifest *man);

#endif
