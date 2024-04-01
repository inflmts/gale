# Gale

Daniel Li (InfiniteLimits) <inflmts@gmail.com>

> "Your will is the most accurate way to predict the future." - Elon Musk

## Overview

**Gale** is my personal configuration system. The intention is for all my
configuration files and scripts (my "dotfiles") to be stored in this repository,
so that they can be managed and shared between computers using Git.

The `galinst` script (the "Gale installer") is used to symlink files into the
home directory. It loads the manifest defined in `install.sh` and does the
minimal work necessary to ensure that the filesystem matches the manifest,
including creating new symlinks, fixing incorrect symlinks, and deleting old
symlinks that are no longer in the manifest. Using a scripted install has the
benefit of being able to dynamically select which files to include, while using
symlinks eliminates the edit-install cycle that would result from a regular
installation.

Host-specific configuration is achieved by adding conditional logic to `galinst`
(and not by other methods, like having multiple Git branches). In addition, Gale
is designed to have as little external configuration as possible; instead,
host-specific configuration is added to Gale itself. The intention is to
maximize reuse and redundancy.

Gale is primarily targeted towards Linux systems. An exception is the Neovim
configuration file, which also works on Windows.

## Getting Started

First create a symlink at `~/.gale` pointing to this repository. Then install
using `galinst`:

```
./galinst
```

`galinst` will automatically install itself into `~/.local/bin`, which should be
available in `$PATH`. Once installed, Gale can be updated simply by running
`galinst`. See `galinst --help` for more options.

## Details

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

