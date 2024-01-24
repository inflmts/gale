# Gale

Daniel Li <inflmts@gmail.com>

## Overview

**Gale** is my personal configuration system. It consists of configuration files
and scripts that I want shared between computers. The single-source directory
structure of this repository allows it to be easily managed by Git. Gale is
primarily geared towards Linux systems.

## Getting Started

This repository must be made available at `~/.gale`, either physically or through
a symbolic link. A symbolic link is recommended.

Gale is installed using a bash script called `galinst` in the `util` directory.
After creating the `~/.gale` symlink, Gale can be installed using this script:

```
util/galinst
```

`galinst` will automatically install itself into `~/.local/bin`, which should be
available in `$PATH`. This way, Gale can be updated simply by running `galinst`.

## Details

### XDG Base Directories

The [XDG Base Directory
Specification](https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html)
variables `$XDG_CONFIG_HOME` and `$XDG_DATA_HOME` are **hardcoded** in Gale and
should not be changed. For example, `galinst` does not use these variables at
all. Their values are:

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
  Storm. In fact, the name "Gale" was chosen to mean "the wind before the storm."

