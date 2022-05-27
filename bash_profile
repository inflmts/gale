# use vim as editor
export EDITOR=vim

# explicitly specify xdg dirs
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_DATA_HOME="$HOME/.data"
export XDG_STATE_HOME="$HOME/.state"
export XDG_CACHE_HOME="$HOME/.cache"

# don't justify man pages
export MANOPT='--nj'

export PATH

# prepend an entry to $PATH
prepend_path() {
  case :"$PATH": in
    *:"$1":*) ;;
    *) PATH="$1:$PATH" ;;
  esac
}

# add standard executable directory
prepend_path ~/.local/bin
# add omega executable directory (compatability)
prepend_path ~/.local/omega/bin

unset -f prepend_path

# quit unless interactive
case "$-" in *i*) ;; *) return ;; esac

## INTERACTIVE

# dialogmenu
#
#   A login menu using dialog(1).
#
dialogmenu() {
  if ! command -v dialog >/dev/null 2>&1; then
    echo >&2 "dialog not installed"
    return 1
  fi

  local resp

  while resp="$(dialog --menu Menu 0 0 0 \
    c 'return to command line' \
    q 'logout' \
    s 'sleep' \
    o 'shutdown' \
    r 'reboot' \
    x 'launch X' \
    3>&1 1>&2 2>&3 3>&-
  )"; do  # ^
          # swap stdout and stderr

    case "$resp" in
      q)
        clear
        exit
        ;;
      s)
        systemctl suspend
        ;;
      o)
        clear
        poweroff
        break
        ;;
      r)
        clear
        reboot
        break
        ;;
      x)
        clear
        launchx
        ;;
      *)
        clear
        break
        ;;
    esac
  done
}

if command -v dialog >/dev/null 2>&1 && [ "$(tty)" = /dev/tty1 ]; then
  # open the menu if dialog is installed and on tty1
  dialogmenu
fi

# CTRL-O opens dialogmenu
bind -m emacs -x '"\C-o":dialogmenu'
bind -m vi-command -x '"\C-o":dialogmenu'
bind -m vi-insert -x '"\C-o":dialogmenu'

# load ~/.bashrc
. ~/.bashrc

# vim:ft=bash
