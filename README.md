# Core Console Configuration

## Overview

The title speaks for itself. These are my console dotfiles.

The way this repository works is simple. Put it in `~/.local/corecon`. Run
`corecon configure` to symlink stuff to files inside it.

My plan is for `~/.local/corecon` to be symlinked to a non dot-path where
editing happens. This way the symlink can be changed to test out new features
(eg. with `git-worktree(1)`).

## Installation

Run the bootstrap script:

```sh
$ ./bootstrap <profile>
```

where `<profile>` is the current profile, eg. `archiplex`. This script sets up
the profile and links `~/.local/corecon`, then runs `corecon configure`.

You may need to log out and log back in to pick up environment variables.
Additionally, `systemctl --user daemon-reload` or other sorts of reloading may
be necessary.

## Updating

Run the configuration script:

```sh
$ corecon configure
```
