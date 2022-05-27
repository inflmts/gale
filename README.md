# Core Console Configuration (corecon)

## Overview

The way this repository works is simple: Put it in `~/.local/corecon`. Do
host-specific configuration with `corecon config`. Run `corecon apply` to make
things happen.

My plan is for `~/.local/corecon` to be symlinked to somewhere else where
editing happens. Most code in corecon should only refer to files through this
symlink.

The `corecon config` command queries and modifies local configuration stored in
`$XDG_CONFIG_HOME/corecon/config`. The format of this file is one entry per
line, with no support for comments. The format of each entry is `<key>=<value>`,
where `<key>` contains any character except '=' and `<text>` is any text. See
`corecon config --help` for usage.

The `corecon apply` command generates and installs files. It currently uses
Ninja coupled with shell scripts as its build system. It should be run every
time something changes in the respository that requires generation/installation.

## Installation

TODO: create bootstrapping method

  vim:tw=80
