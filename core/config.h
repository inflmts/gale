/*
 * Gale configuration library
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"
#include "uthash-handle.h"

// Returns true if `s` is a valid key. A valid config key:
//
//   1. Must begin with a lowercase letter or digit.
//   2. Must only contain lowercase letters, digits, dashes (-), or periods (.).
//
bool config_is_valid_key(const char *s);

struct config_entry
{
  char *key;
  char *value;
  UT_hash_handle hh;
};

struct config
{
  struct config_entry *head;
};

// Returns the config entry after `ent`.
static inline struct config_entry *config_entry_next(struct config_entry *ent)
{
  return ent->hh.next;
}

// Load a config object from the file `filename`. If `filename` is NULL, uses
// the default file path. If the file does not exist, this is not considered an
// error and the config object is initialized with an empty configuration. When
// finished, the config object should be released using config_free().
//
// Returns 0 on success. On error, -1 is returned and `*errmsg` is set to a
// string representing the error message. The error message string should be
// released using free().
//
int config_load(struct config *conf, const char *filename, char **errmsg);

// Get the value of the specified key, or NULL if the key was not found.
char *config_get(struct config *conf, const char *key);

// Change the value of a key.
void config_set(struct config *conf, const char *key, const char *value);

// Delete a key. Returns true if the key was found and removed.
bool config_unset(struct config *conf, const char *key);

// Free memory used by a config object.
void config_free(struct config *conf);

#endif
