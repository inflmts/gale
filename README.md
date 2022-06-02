# Gale

InfiniteLimits Core Console Configuration (previously known as _corecon_, _psi_,
or _coreterm_)

## Overview

This is my personal terminal configuration consolidated into a Git repository, a
rite of passage for any Linux user seeking something more from their computing
environment. This is my imprint, my personality in the form of files and
utilities tailored only to me, on the software universe I willingly and
gratefully accept from the many millions of open source developers worldwide
contributing to the betterment of the programs we use every day. This repository
may change drastically -- my skills will improve, my needs will adapt, and my
whims will fluctuate -- but it will always remain, at its core, a distinct
reflection of me.

The way this repository works is simple: Symlink `~/.local/gale` to this
directory. Do host-specific configuration with `windconf`. Run `gale-apply` to
make things happen.

Windconf is Gale's local configuration system. The `windconf` command queries
and modifies the configuration file `$XDG_CONFIG_HOME/gale/config`, which is a
text file with an extremely simple format. Each line corresponds to exactly one
entry, consisting of a key (containing any character except '=') and optionally
a '=' followed by a value (containing any text). See `windconf --help` for
usage. A list of variables Gale itself recognizes it provided below, though Gale
will quietly ignore other variables set in the configuration file (for use by
Storm, for example).

The `gale-apply` command generates and installs files. As the build system, Gale
currently uses Ninja coupled with simple shell scripts for speed and simplicity.

## Installation

Run the bootstrap script from the source directory:

    ./bootstrap

This will set up the environment for you, symlink `~/.local/gale`, and spawn a
shell. (It'll tell you this, of course.) Here you can use the usual Gale
commands:

    windconf set systemd
    gale-apply

## Changing the Source Root

On the other hand, if you only change the source root, run:

    ./bootstrap --symlink

To correct the symlink.

## Configuration

The following configuration variables are understood by Gale:

    systemd
      Enable systemd support (eg. user environment configuration, units).

    autologin=<action>
      Automatically do something on login. Supported values for <action> are:

      storm:<action>
        Forward to Storm.

