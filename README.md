# Gale

Daniel Li &mdash; [inflmts.com](https://inflmts.com)

## Introduction

**Gale** is my personal configuration system. The purpose of Gale is to collect
my configuration files and scripts (my "dotfiles") in a single repository to be
shared between computers using Git. Gale is primarily used on Linux.
However, some configuration files will also work on Windows (without Gale
integration, of course), for example the Neovim configuration file.

## Getting Started

The only required software is Git.

```
git clone -n https://github.com/inflmts/gale.git ~/.gale
cd ~/.gale
git config core.worktree ../..
git config status.showUntrackedFiles no
git checkout
```

## Usage

The repository contains a `~/.local/bin/gale` script that invokes Git with the
correct gitdir and working tree, so that it works from anywhere in the home
directory. It passes all its arguments to Git, so you can do `gale status`,
`gale commit`, `gale push`, etc.

To add a file or the entire contents of a directory:

```
gale add <file>
```

To update all tracked files:

```
gale add -u
```

Don't use `gale add -A`, as that will add everything in the home directory.

To list tracked files under the current working directory:

```
gale ls-files
```

## Directories

* `~/.gale/.git` - Gale's own Git repository
* `~/.local/bin` - scripts
* `~/.config` - `$XDG_CONFIG_HOME`
* `~/.data` - `$XDG_DATA_HOME`
* `~/.state` - `$XDG_STATE_HOME`
* `~/.cache` - `$XDG_CACHE_HOME`

Note that the values of `$XDG_DATA_HOME` and `$XDG_STATE_HOME` differ from the
[XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html).
I did this mostly for fun.

In addition, if `$XDG_RUNTIME_DIR` is not set at login, Gale will try to use
`/run/daniel` if it is a directory and is owned by the current user.

## History

I went through many, many different personal configuration systems, often
experimenting with vastly different management schemes, before settling on Gale.
The names I gave them were equally colorful: Psi, Zeta, Omega, DMM (Dynamic
Module Manager), and more. Before Gale, there was Corecon ("core console"), and
Storm (everything else). These two were eventually merged into one repository
and renamed "Gale" &mdash; "the wind before the storm."

Gale originally used [Ninja](https://ninja-build.org) to "build" the home
directory from the "source" repository. This approach turned out to be more
complicated than necessary, since most of the files were simply copied or
symlinked from source to destination. Also, Ninja doesn't handle symlinks as
build outputs correctly.

So eventually Ninja was dropped, and symlinks were used exclusively. The
repository would be placed at `~/.gale`, and a program (`galinst` written in
shell, or later `gallade` written in C) would manage these symlinks based on a
declarative configuration (`install.conf` for `galinst`, inline config blocks
for `gallade`). There was the possibility of a template system, but that was
never implemented.

I eventually realized I was doing a lot of work for no reason,
because I was already using Git, which is great at tracking files.
The current scheme (informally "Gale 2") resembles [yadm](https://yadm.io),
with Git managing the home directory itself.
There was still some support for dynamic configuration files via a
`~/.gale/current` symlink that would point to a different tracked directory
on different hosts, however I never used it and it was eventually dropped.
