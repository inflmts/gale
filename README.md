# Gale

Daniel Li <inflmts@gmail.com>

## Overview

**Gale** is a collection of scripts, configuration files, and utilities that
represents the personal configuration I want shared across any system I use.
Gale is primarily targeted toward GNU/Linux systems. The single-source directory
structure of this repository allows it to be easily managed by Git.

The core of Gale is a set of C utilities collectively called Galcore. Source
code is located in the `core` directory. These utilities include `galconf`,
which provides quick access to Gale's simple configuration system, and
`galinst`, which is used to install Gale.

## Getting Started

Requirements:

* GCC/Clang
* Ninja

To build Galcore, run the configure script, then build using `ninja`:

```
cd core
./configure --debug
ninja
```

This will create debug builds of `galconf` and `galinst` and place them in the
`core` directory. To create an optimized release build, omit `--debug` from the
`configure` command line. The available `configure` options can be found by
looking at the `configure` script or by running `./configure --help`.

This repository must be made available at `~/.gale`, either physically or through
a symbolic link.

The set of symlinks to create can be controlled using `galconf`. These symlinks
can then be created using `galinst`. Whenever the manifest or configuration are
changed, `galinst` should be run to pick up new symlinks and delete old ones.

## Details

### XDG Base Directories

The [XDG Base Directory
Specification](https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html)
variables `$XDG_CONFIG_HOME` and `$XDG_DATA_HOME` are **hardcoded** in Gale and
should not be changed. For example, the utilities in Galcore **do not** use
these variables at all. Their values are:

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
  Storm. In fact, the name "Gale" was chosen to mean "the wind of the storm."

