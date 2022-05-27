# setup environment
export EDITOR=vim
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_DATA_HOME="$HOME/.data"
export XDG_STATE_HOME="$HOME/.state"
export XDG_CACHE_HOME="$HOME/.cache"
export MANOPT='--nj'

export PATH

# prepend_path <path>
#
#   Add the passed path to $PATH if it isn't in there already.
#
prepend_path() {
  case :"$PATH": in
    *:"$1":*) ;;
    *) PATH="$1:$PATH" ;;
  esac
}

# add standard executable directory
prepend_path ~/.local/bin
# add omega executable directory
prepend_path ~/.omega/bin
# add corecon executable directory
prepend_path ~/.local/corecon/bin

unset -f prepend_path

# At this point quit if not interactive. The bootstrap script sources this to
# set up the environment so we don't want to disturb it.
case "$-" in *i*) ;; *) return ;; esac

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
