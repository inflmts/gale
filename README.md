# Gale

InfiniteLimits Core Configuration
(previously known as _corecon_, _storm_, _psi_, or _coreterm_)

Github: <https://github.com/inflmts>

## Overview

This is my configuration; that is, what makes my computer, _mine_.

Gale is primarily targeted toward GNU/Linux systems. (It uses many GNU-specific
command options, for example.) It might not work on other operating systems.
I'll leave that work for when I have to use those systems myself.

Gale exclusively deals with files. Every file generated, installed, or symlinked
to the home directory is derived from source files in this repository.
Generating, installing, and symlinking files in Gale is collectively known as
_building_. Gale uses [Ninja](https://ninja-build.org), along with a simple
shell script Ninja generator, to perform builds.

## Building

See

    ./configure --help

for available options. Then build with

    ./configure <options>...
    ninja

## Configuration

`~/.profile` sources local configuration from `$XDG_CONFIG_HOME/gale/profile`,
if available.

`~/.bash_profile` sources `~/.profile` and local configuration from
`$XDG_CONFIG_HOME/gale/bash_profile`, if available.

`~/.bashrc` sources local configuration from `$XDG_CONFIG_HOME/gale/bashrc`, if
available. The following variables have an effect when set in this file:

    $GALE_ENABLE_NVM

      If set, autodetects and initializes nvm if available. Note that this
      causes ~/.nvm/nvm.sh to be sourced with --no-use, to avoid delaying
      startup. This means you must manually call 'nvm use default' to actually
      use node. Default is enabled.

## Benefits of Installing

- Host-specific configuration is simpler. All it takes is converting an install
  rule to use a generator and adding some configuration.

- Making changes or checking out different versions of this repository will not
  immediately destabilize the system, since these changes will need to be
  installed first.

- Programs that modify their configuration files will not accidentally affect
  this source tree.

## Benefits of Symlinking

- More appropriate for a bunch of dotfiles.

- Prevents duplicating files, which saves space.

- Edit-test cycles are faster, since no copying needs to take place.

