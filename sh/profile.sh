#
# ~/.profile
#
# This file is part of Gale.
#

# ENVIRONMENT VARIABLES
#=======================================

if command -v nvim >/dev/null; then
  export EDITOR=nvim
elif command -v vim >/dev/null; then
  export EDITOR=vim
fi

# XDG base directories
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_DATA_HOME="$HOME/.data"
export XDG_STATE_HOME="$HOME/.state"
export XDG_CACHE_HOME="$HOME/.cache"

# create fallback $XDG_RUNTIME_DIR if necessary
if [ -z "$XDG_RUNTIME_DIR" ]; then
  export XDG_RUNTIME_DIR="/tmp/user/$(id -u)"
  mkdir -p "$XDG_RUNTIME_DIR"
fi

export PATH

# prepend an entry to $PATH if it isn't already there
gale_add_path() {
  case :"$PATH": in
    *:"$1":*) ;;
    *) PATH="$1:$PATH" ;;
  esac
}

# add standard executable directory
gale_add_path ~/.local/bin

# load local config file
if [ -s ~/.config/gale/profile ]; then
  . ~/.config/gale/profile
fi

# if running in systemd, update the user manager environment
systemctl --user import-environment >/dev/null 2>&1 \
  PATH \
  EDITOR \
  XDG_CONFIG_HOME \
  XDG_DATA_HOME \
  XDG_STATE_HOME \
  XDG_CACHE_HOME
