# Gale

Daniel Li &mdash; [inflmts.com](https://inflmts.com)

## Introduction

**Gale** is my personal configuration system. The purpose of Gale is to collect
my configuration files and scripts (my "dotfiles") in a single repository to be
shared between computers using Git. Gale system is designed to be used on Linux,
although some configuration files will also work on Windows (without Gale
integration, of course), for example the Neovim configuration file.

## Getting Started

The only requirements are Git and a C compiler (currently GCC).

```
cd ~
git clone https://github.com/inflmts/gale.git
ln -s gale .gale
gale/util/gallade
```

## Gallade

Gale uses a symlink approach to put files into the home directory. The core of
the Gale system is **Gallade**, the installer, which is written in C (see
`gallade.c`). Gallade scans each file in the source repository and looks for a
_config block_, which contains directives for specifying where to install the
file. A config block begins and ends with a line containing three consecutive
dashes (`---`). The dashes must be at the end of the line and must be preceded
by whitespace. The text before the dashes is stripped from each line in the
config block and must be the same for every line in the block. Empty lines are
currently not supported. Comments are not supported either because the config
block is usually already embedded in a comment.

Gallade then makes the filesystem match the configuration by creating new
symlinks, fixing incorrect symlinks, and deleting old symlinks that are no
longer defined in the configuration (and their empty parent directories).
Currently, Gallade does not support conditional logic, but this will likely be
implemented in the future to allow different configurations on different
machines. Other features that are not implemented yet include templates and
hooks.

The Gallade wrapper script (`util/gallade`) automatically recompiles Gallade if
it detects that the source files have changed. The real Gallade is at
`~/.data/gale/gallade`, next to the log file `~/.data/gale/gallade.log`. Gallade
will install the wrapper script in `~/.local/bin` so it is possible to run with
just `gallade`.

Yes, [that Gallade](https://www.pokemon.com/us/pokedex/gallade).

## XDG Base Directories

The [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html)
variables `$XDG_CONFIG_HOME` and `$XDG_DATA_HOME` are **hardcoded** in Gale and
cannot be changed. For instance, Gallade does not use these variables at all.
Their values are:

```
$XDG_CONFIG_HOME    ~/.config
$XDG_DATA_HOME      ~/.data
```

Other variables are read from the environment. Their default values are:

```
$XDG_STATE_HOME     ~/.state
$XDG_CACHE_HOME     ~/.cache
```

Note that the values of `$XDG_DATA_HOME` and `$XDG_STATE_HOME` differ from the
standard. I think that `~/.local` should be like `/usr/local`, not `/var`.

In addition, a if `$XDG_RUNTIME_DIR` isn't set at login, a fallback directory
will be created at `/tmp/daniel`.

## Why "Gale?"

I went through many, many different personal configuration systems, often
experimenting with vastly different management schemes, before settling on Gale.
The names I gave them were equally colorful: Psi, Zeta, Omega, DMM (Dynamic
Module Manager), and more. Before Gale, there was Corecon ("core console"), and
Storm (everything else). These two were eventually merged into one repository
and renamed "Gale" &mdash; "the wind before the storm."
