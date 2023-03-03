
     dP""b8    db    88     888888
    dP   `"   dPYb   88     88__
    Yb  "88  dP__Yb  88  .o 88""
     YboodP dP""""Yb 88ood8 888888

# InfiniteLimits Linux Configuration

by Daniel Li

## Overview

**Gale** is a collection of scripts and configuration files that allows me to
manage all my personal configuration, on any host, from one centralized source
using Git. It is intended to be the ultimate front-end; Gale is designed to
integrate with software, not the other way around.

**Gale is primarily targeted toward GNU/Linux systems.** Development is heavily
dependent on the operating system I'm (struggling to) use right now.

Gale places files outside the `~/.gale` directory through a process I like to
call _updating_. Updating involves taking static files in `~/.gale` and either
creating symlinks to them or using them to generate other files. Gale uses
[Ninja][2] perform updates.

Directories per the [XDG Base Directory Specification][1] are hardcoded in Gale
and cannot be changed. Their values are:

    $XDG_CONFIG_HOME    ~/.config
    $XDG_DATA_HOME      ~/.data
    $XDG_STATE_HOME     ~/.log
    $XDG_CACHE_HOME     ~/.cache

## Getting Started

This repository should be made available at `~/.gale`.

If this repository is not already at `~/.gale`, a symlink to the current working
directory can be created automatically with:

    ./bootstrap

Now update Gale:

    bin/galupd

Once set up, Gale can be updated with `galupd`.

## Notes

Ascii text generated with <https://ascii.co.uk>.

This is probably way overcomplicated for what I actually need. I am really good
at coming up with bad ideas. That's why I need other people to help me,
otherwise stuff I make ends up looking like this.

[1]: https://www.freedesktop.org/basedir-spec/basedir-spec-latest.html
[2]: https://ninja-build.org

