# Gale

Daniel Li &mdash; [inflmts.com](https://inflmts.com)

## Introduction

**Gale** is my personal configuration system. The purpose of Gale is to collect
my configuration files and scripts (my "dotfiles") in a single repository to be
shared between computers using Git. Gale is designed to be used on Linux,
although some configuration files will also work on Windows (without Gale
integration, of course), for example the Neovim configuration file.

## Getting Started

The only required software is Git.

```
mkdir ~/.gale
cd ~/.gale
git init -b main
git config core.worktree ../..
git config status.showUntrackedFiles no
git remote add origin https://github.com/inflmts/gale.git
git pull
```

For host-specific configuration:

```
ln -s <host> ~/.gale/current
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

To create a dynamic configuration file (call it `example.conf`), write the
configuration file at `~/.gale/<host>/example.conf`, then create a symbolic link
at `example.conf` pointing to `~/.gale/current/example.conf`. `~/.gale/current`
will be a symbolic link pointing to the `~/.gale/<host>` directory for the
current host.

## XDG Base Directories

Gale uses the following values for the
[XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html):

```
$XDG_CONFIG_HOME    ~/.config
$XDG_DATA_HOME      ~/.data
$XDG_STATE_HOME     ~/.state
$XDG_CACHE_HOME     ~/.cache
```

Note that the values of `$XDG_DATA_HOME` and `$XDG_STATE_HOME` differ from the
standard.

In addition, if `$XDG_RUNTIME_DIR` is not set at login, a fallback directory
will be created at `/tmp/daniel`.

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

The current scheme (informally "Gale 2") resembles [yadm](https://yadm.io),
with the alternates mechanism replaced by the less-powerful `~/.gale/current`
symlink.
