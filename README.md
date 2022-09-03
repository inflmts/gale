
     dP""b8    db    88     888888
    dP   `"   dPYb   88     88__
    Yb  "88  dP__Yb  88  .o 88""
     YboodP dP""""Yb 88ood8 888888

InfiniteLimits Core Configuration
(previously known as _corecon_, _storm_, _psi_, or _coreterm_)

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

## Notes

Ascii text generated with <https://ascii.co.uk>.
