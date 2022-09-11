
     dP""b8    db    88     888888
    dP   `"   dPYb   88     88__
    Yb  "88  dP__Yb  88  .o 88""
     YboodP dP""""Yb 88ood8 888888

# InfiniteLimits Core Configuration

Github: <https://github.com/inflmts>

## Overview

These are my personal config files.

Gale is primarily targeted toward GNU/Linux systems. (It uses many GNU-specific
command options, for example.) It might not work on other operating systems.
I'll leave that work for when I have to use those systems myself.

Gale exclusively deals with files. Every file generated, installed, or symlinked
to the home directory is derived from source files in this repository.
Generating, installing, and symlinking files in Gale is collectively known as
_building_. Gale uses [Ninja](https://ninja-build.org), along with a simple
shell script Ninja generator (`configure`), to perform builds.

## Building

See

    ./configure --help

for available options. Build with

    ./configure <options>...
    ninja

## Notes

Ascii text generated with <https://ascii.co.uk>.

See `COMPARISON.md` for a small comparison of what I think are the pros and cons
of various methods of dotfile management.

This is probably way overcomplicated for what people usually need. I am known
for being really good at coming up with bad ideas. That's why I need other
people to help me, otherwise my code ends up looking like this.

