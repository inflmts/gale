# Gale

InfiniteLimits Core Console Configuration (previously known as _corecon_, _psi_,
or _coreterm_)

by Daniel Achelon

Email: <inflmts@gmail.com>

Github: <https://github.com/inflmts>

## Preface

Behold, my personal terminal configuration. This is my imprint, the source of my
personality, on the software universe I willingly and gratefully accept from the
many millions of open source developers worldwide contributing to the betterment
of the programs we use every day. There is no doubt that, in ten years, this
repository will look very different than the way it does today. My grasp of the
tools spread before me, the goals I aim for through software, even my individual
preferences; it will all change, for better or for worse. But this repository
will always reflect it faithfully; remaining, at its core, a distinct reflection
of my will to do things differently.

My speech is finished. Let the fun begin.

## Overview

Gale exclusively deals with files. Every file generated or installed to the home
directory is derived (and thus can be regenerated) from source files in this
repository. Generating and installing files in Gale is collectively known as
_building_. Gale uses [Ninja](https://ninja-build.org) to perform builds.

## Building

Building Gale is simple. First make sure you have Ninja. Then edit `config.sh`
if applicable, and build:

    ./configure
    ninja

## Configuration

The `configure` script sources the local configuration file `config.sh` and
generates the ninja build file, `build.ninja`, which is read by Ninja. The
following variables set in `config.sh` affect the behavior of `configure`:

    $BUILDDIR

      The build directory (default: build)

    $PREFIX

      The base directory for static files (default: $HOME/.local)

    $BINDIR

      The directory for user executables (default: $PREFIX/bin)

    $LIBDIR

      The directory for other files (default: $PREFIX/lib/gale)

    $GALE_ENABLE_SYSTEMD

      If non-null, enables systemd support (default: off)

### Shell Initialization

`~/.profile` sources local configuration from `$XDG_CONFIG_HOME/gale/profile`,
if available.

`~/.bash_profile` sources `~/.profile` and local configuration from
`$XDG_CONFIG_HOME/gale/bash_profile`, if available.

`~/.bashrc` sources local configuration from `$XDG_CONFIG_HOME/gale/bashrc`, if
available. The following variables have an effect when set in this file:

    $GALE_ENABLE_NVM

      If non-null, autodetects and initializes nvm if available. Note that this
      sources ~/.nvm/nvm.sh with --no-use, so as to not delay startup. This
      means you must manually use 'nvm use default' to actually use node.
      Default is enabled.

## Comparison With a Symlink-Based Approach

### Pros

- Host-specific configuration is easy. All it takes is writing a generator and
  adding a new configuration option.

- Making changes or checking out different versions of this repository will not
  immediately destabilize the system. Gale is also less likely to fall into a
  dangerous state where different files within the system (in a symlink-based
  approach, this would be symlinked vs. generated files) reflect different
  versions of the repository.

- Programs that modify their configuration files will not accidentally affect
  this source tree.

### Cons

- This is probably overkill for a bunch of dotfiles.

- Duplicate files waste space.

- Edit-test cycles are slower because modified files must be re-installed to
  take effect. However, this doesn't apply to generated files, and can be
  streamlined with Vim autocommands or mappings.

