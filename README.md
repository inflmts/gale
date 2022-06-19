# Gale

InfiniteLimits Core Console Configuration (previously known as _corecon_, _psi_,
or _coreterm_)

## Preface

This is my personal terminal configuration consolidated into a Git repository.
This is my imprint, my personality in the form of files and utilities tailored
only to me, on the software universe I willingly and gratefully accept from the
many millions of open source developers worldwide contributing to the betterment
of the programs we use every day. This repository will most likely change
dramatically -- my skills will improve, my needs will adapt, and my whims will
fluctuate -- but it will always remain, at its core, a distinct reflection of me
and my will to do things differently.

My speech is finished. Let's get down to business.

## Overview

Gale exclusively deals with files. A _build_ in Gale consists of generating and
installing files to the home directory based on source files in this repository.
Gale uses Ninja to perform builds. This has several benefits compared to a
symlink-based approach:

- Host-specific configuration is easy. All it takes is writing a generator and
  adding a new configuration option.

- Making changes or checking out different versions of this repository will not
  immediately destabilize the system. Gale is also less likely to fall into a
  dangerous state where different files within the system (in a symlink-based
  approach, this would be symlinked vs. generated files) reflect different
  versions of the repository.

- Programs that modify their configuration files will not accidentally affect
  this source tree.

Copying everything also has several drawbacks:

- Duplicate files waste space.

- Edit-test cycles are slower because modified files must be re-installed to
  take effect. However, this doesn't apply to generated files, and can be
  streamlined with Vim autocommands or mappings.

## Building

Building Gale is simple. First make sure you have Ninja. Then:

    vim config.sh
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

