# corecon

InfiniteLimits Core Console Config

## Overview

The way this repository works is simple: Put it in `~/.local/corecon`. Do
host-specific configuration with `corecon config`. Run `corecon apply` to make
things happen.

My plan is for `~/.local/corecon` to be symlinked to somewhere else where
editing happens. Most code in corecon should only refer to files through this
symlink.

The `corecon config` command is the official interface to corecon's local
configuration. This utility queries and modifies the configuration file
`$XDG_CONFIG_HOME/corecon/config`. The format of this file is extremely simple:
one entry is allowed per line, consisting of a key (containing any character
except '=') and optionally a '=' followed by a value (containing any text). See
`corecon config --help` for usage.

The `corecon apply` command generates and installs files. It currently uses
Ninja coupled with shell scripts as the build system. It should be run every
time something changes in the respository that requires generation/installation.

## Installation

Symlink `~/.local/corecon`:

```
lib/sym $(realpath .) ~/.local/corecon
```

Source the environment setup script from the shell:

```
. ./bootstrap.sh
```

You probably want to:

```
corecon config systemd
```

Finally, run `corecon apply` to install corecon.

## Configuration

`systemd` - enable systemd support

  vim:tw=80
