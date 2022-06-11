# Gale

InfiniteLimits Core Console Configuration (previously known as _corecon_, _psi_,
or _coreterm_)

## Preface

This is my personal terminal-only configuration consolidated into a Git
repository. This is my imprint, my personality in the form of files and
utilities tailored only to me, on the software universe I willingly and
gratefully accept from the many millions of open source developers worldwide
contributing to the betterment of the programs we use every day. This repository
will most likely change dramatically -- my skills will improve, my needs will
adapt, and my whims will fluctuate -- but it will always remain, at its core, a
distinct reflection of me and my will to do things differently.

My speech is finished. Let's get down to business.

## Overview

Gale exclusively deals with files. A _build_ in Gale consists of generating and
installing files to the home directory. Gale uses Ninja to perform builds. This
has several benefits compared to an approach involving symlinks:

- Host-specific configuration is easy. All it takes is writing a generator and
  adding a new configuration option.

- Making changes or checking out different versions of this repository will not
  immediately affect the stability of the system. The system is also less likely
  to fall into a dangerous state where different files within the system (ie.
  symlinked vs. generated files) reflect different revisions of this repository.

- Programs that modify their configuration files will not accidentally affect
  this source tree.

## Building & Configuration

The `configure` script sets build-time configuration and generates the Ninja
build file. It is usually not necessary to invoke `configure` directly, only
when building for the first time or changing configuration. Ninja is able to
detect when the `configure` script changes and update its build file
accordingly. Usage:

    ./configure [<option>...]

The following configuration options are currently supported:

    --builddir=<dir>
      The build directory (default: build)

    --prefix=<dir>
      The base directory for static files (default: ~/.local)

    --bindir=<dir>
      The directory for user executables (default: <prefix>/bin)

    --systemd
      Enable systemd support.

