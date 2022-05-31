# corecon

InfiniteLimits Core Console Configuration

## Overview

This is my personal terminal-only configuration consolidated into a Git
repository, a rite of passage for any Linux user seeking something more from
their computing environment. This is my imprint, my personality in the form of
configuration files tailored only to me, on the software universe I willingly
and gratefully accept from the many millions of open source developers worldwide
contributing to the betterment of computers and software. It will change -- my
skills will improve, my needs will adapt, and my personality will fluctuate --
but it will always remain, at its core, a distinct reflection of myself.

The way this repository works is simple: Make `~/.local/corecon` a symlink to
this directory. Do host-specific configuration with `corecon config`. Run
`corecon apply` to make things happen.

The `corecon config` command is the official interface to corecon's local
configuration. This utility queries and modifies the configuration file
`$XDG_CONFIG_HOME/corecon/config`. The format of this file is extremely simple:
one entry is allowed per line, consisting of a key (containing any character
except '=') and optionally a '=' followed by a value (containing any text). See
`corecon config --help` for usage. A list of variables corecon itself recognizes
it provided below, though corecon will accept and ignore other variables set in
the configuration file (for use by Omega, for example).

The `corecon apply` command generates and installs files. Corecon currently uses
Ninja coupled with simple shell scripts as its build system. This command should
be run every time something changes in the respository that needs to be
installed or is the dependency of a generator.

## Installation

Run the bootstrap script from the source directory:

    ./bootstrap

This will set up the environment for you, symlink `~/.local/corecon`, and spawn
a shell. (It'll tell you this because it should be that obvious.) Here you can
use the usual corecon commands:

    corecon config systemd
    corecon apply

## Changing the Source Root

On the other hand, if you only change the source root, run:

    ./bootstrap --symlink

To correct the symlink.

## Configuration

The following configuration variables are understood by corecon:

    systemd
      Enable systemd support (eg. user environment configuration, units).

    autologin=<action>
      Automatically do something on login. Supported values for <action> are:

      bspwm
        Start bspwm. This only occurs once per boot.
