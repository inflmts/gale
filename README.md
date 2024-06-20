# Gale

Daniel Li (InfiniteLimits) <inflmts@gmail.com>

> "Your will is the most accurate way to predict the future." - Elon Musk

## Overview

**Gale** is my personal configuration system. The purpose of Gale is to collect
my configuration files and scripts (my "dotfiles") in a single repository to be
shared between computers using Git.

Gale is primarily targeted towards Linux systems. An exception is the Neovim
configuration file, which also works on Windows.

Gale uses a symlink approach to put files into the home directory. The `galinst`
script (the "Gale installer") loads the manifest defined in `install.conf` and
makes the filesystem match the manifest by creating new symlinks, fixing
incorrect symlinks, and deleting old symlinks that are no longer in the manifest
(and their empty parent directories). Currently, `install.conf` does not support
any conditional logic, although this will likely be implemented in the future to
dynamically alter the manifest based on the Gale profile and other
characteristics of the running machine.

## Getting Started

1. Clone this repository to `~/gale`.

2. Set `~/.config/gale/profile` to the profile you want to use.

3. Install Gale using `galinst`.

4. Log out and log back in to pick up environment variables.

`galinst` will automatically install itself into `~/.local/bin`, which should be
available in `$PATH`. Once installed, Gale can be updated simply by running
`galinst`. `galinst` supports verbose and dry-run options; see `galinst --help`
for more information.

## Details

### Repository Location

This repository must be placed at `~/gale`. Previously, it was located at
`~/.gale` (with a dot), similar to how other programs store user files. However,
since the purpose of Gale is to manage configuration files, placing Gale itself
in a configuration-like location seemed inappropriate. Directories beginning
with a dot are usually managed by the program; Gale on the other hand is
entirely user-maintained, so it made sense to put it in a prominently visible
location.

### Host-Specific Configuration

Although I try to use the same configuration across as many systems as possible,
there will be cases where this is not possible. For these files, `galinst` can
be instructed to install different symlinks based on the system.

Every host is assigned a unique profile, a string identifier stored at
`~/.config/gale/profile`. This is useful when autodetection would be difficult
or unnecessarily complicated. Other than this, Gale has zero external
configuration, eliminating the need to maintain a standard configuration format.

### XDG Base Directories

The [XDG Base Directory
Specification](https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html)
variables `$XDG_CONFIG_HOME` and `$XDG_DATA_HOME` are **hardcoded** in Gale and
cannot be changed. For example, `galinst` does not use these variables at all.
Their values are:

```
$XDG_CONFIG_HOME    ~/.config
$XDG_DATA_HOME      ~/.data
```

Other variables, however, are read from the environment. Their default values
are:

```
$XDG_STATE_HOME     ~/.state
$XDG_CACHE_HOME     ~/.cache
```

Note that the value of `$XDG_DATA_HOME` and the default value of
`$XDG_STATE_HOME` differ from the standard.

## Trivia

* I went through many, many different personal configuration systems, often
  experimenting with vastly different management schemes, before settling on
  Gale. These included, in not very strict order, Psi, Zeta, DMM, Corecon, and
  Storm. In fact, the name "Gale" was chosen to mean "the wind before the
  storm."

