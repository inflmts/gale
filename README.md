
     dP""b8    db    88     888888
    dP   `"   dPYb   88     88__
    Yb  "88  dP__Yb  88  .o 88""
     YboodP dP""""Yb 88ood8 888888

# InfiniteLimits Linux Configuration

by Daniel Li

## Overview

**Gale** is a collection of scripts and configuration files that allows me to
manage all my personal configuration, on any host, from one centralized source
using Git. It is intended to be the ultimate front-end; Gale is designed to
integrate with software, not the other way around.

**Gale is primarily targeted toward GNU/Linux systems.** Development is
obviously highly dependent on the operating system I'm using right now.

Gale places files outside the `~/.gale` directory through a process in Gale
called _updating_. This involves generating a [Ninja][2] manifest with
`galsetup` that symlinks, installs, or generates files in the home directory and
running `galupd` which is only a wrapper around Ninja. The update can be
customized using `galconf`. These utilities are located in the `internal`
directory.

These [XDG Base Directory Specification][1] variables are hardcoded in Gale and
cannot be changed:

```
$XDG_CONFIG_HOME    ~/.config
$XDG_DATA_HOME      ~/.data
```

The default values of other variables are:

```
$XDG_STATE_HOME     ~/.log
$XDG_CACHE_HOME     ~/.cache
```

## Getting Started

This repository should be made available at `~/.gale`.

If this repository is not already at `~/.gale`, a symlink to the current working
directory can be created automatically with:

```
./bootstrap
```

Now update Gale:

```
internal/galupd
```

Once set up, Gale can be updated with `galupd`.

## Notes

Ascii text generated with <https://ascii.co.uk>.

[1]: https://www.freedesktop.org/basedir-spec/basedir-spec-latest.html
[2]: https://ninja-build.org

