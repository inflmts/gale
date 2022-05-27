# Core Console Configuration (corecon)

## Overview

The way this repository works is simple: Put it in `~/.local/corecon`. Do
host-specific configuration with `corecon config`. Run `corecon apply` to make
things happen.

My plan is for `~/.local/corecon` to be symlinked to somewhere else where
editing happens. The symlink can be changed with the bootstrap script. Most code
in corecon should only refer to files through this symlink.

The `config` command changes local configuration stored in
`$XDG_DATA_HOME/corecon/config`. This file is intended to be sourced by shells
supporting POSIX-like variable assignments and quoting, and is indeed sourced by
the `config` command itself to support persistency. Run `corecon config --help`
for a list of available options.

The `apply` command does the minimum work necessary to generate everything and
create symlinks to the right place. It currently uses Ninja combined with simple
shell scripts to build files. `apply` automatically generates the `apply.ninja`
file and invokes Ninja. You may need to log out and log back in to pick up
environment variables. Additionally, `systemctl --user daemon-reload` or other
kinds of restarting/reloading may be necessary.

## Installation

When installing for the first time, or when changing the source root, run the
bootstrap script:

```sh
$ ./bootstrap [<args>...]
```

All this does is link `~/.local/corecon`, then run `corecon config` with the
provided arguments. `corecon apply` or `--apply` must be used to apply changes.

## Updating

As stated above, all you need to do is run:

```sh
$ corecon apply
```

  vim:tw=80:fo+=a
