#
# Bash initialization file
#

# So apparently Bash when accessed over ssh will sometimes source ~/.bashrc
# even when the session is not interactive. In that case,
# much of the stuff below is not useful, and we have to be careful
# not to read or output anything or it might confuse the client program.

# quit unless interactive
# apparently needed for rcp or scp or something
[[ $- != *i* ]] && return

# disable history substitution with '!'
set +H

msg() { printf >&2 '%s\n' "$*"; }
warn() { printf >&2 '\033[1;33mwarning:\033[0m %s\n' "$*"; }
err() { printf >&2 '\033[1;31merror:\033[0m %s\n' "$*"; }

# Disable flow control, which can be confusing for new users,
# and is generally not useful with a scrollback buffer.
# This also allows us to bind CTRL-S and CTRL-Q.
stty -ixon

#-- MENU -----------------------------------------------------------------------

gale_menu() {
  local tty is_vt options key

  if tty=$(tty) && [[ $tty = /dev/tty* ]]; then
    is_vt=1
  else
    is_vt=0
  fi

  options='[q] abort [s] sleep [o] shutdown [r] reboot '
  if (( is_vt )); then
    options+='[d] desktop '
  fi
  printf '\033[1mMenu:\033[0m %s' "$options"
  read -rsN1 key
  printf '\r\033[K'

  case $key in
    s)
      gale-sleep
      ;;
    o)
      if gale_menu_confirm $'\033[1;31mAre you sure you want to shutdown the computer?\033[0m '; then
        gale-shutdown
      fi
      ;;
    r)
      if gale_menu_confirm $'\033[1;31mAre you sure you want to reboot the computer?\033[0m '; then
        gale-reboot
      fi
      ;;
    d)
      if (( is_vt )); then
        gale_menu_desktop
      fi
      ;;
  esac
}

gale_menu_desktop() {
  local key

  printf '\033[1mLaunch desktop:\033[0m [q] abort [s] sway [h] hyprland [l] labwc '
  read -rsN1 key
  printf '\r\033[K'

  case $key in
    s)
      gale-sway
      ;;
    h)
      gale-hyprland
      ;;
    l)
      gale-labwc
      ;;
  esac
}

gale_menu_confirm() {
  local key
  printf %s "$1"
  read -rsN1 key
  printf '\r\033[K'
  case $key in
    [yY]) return 0 ;;
    *) return 1 ;;
  esac
}

#-- ALIASES --------------------------------------------------------------------

alias grep='grep --color=auto'
alias md='mkdir -p'

alias ls='LC_COLLATE=C ls -hv --group-directories-first --color=auto'
alias la='ls -A'
alias lsa='ls -A'
alias ll='ls -l'
alias lla='ls -lA'

alias tree='LC_COLLATE=C tree --dirsfirst'
alias treea='tree -a'
alias treed='tree -d'
alias treeda='tree -da'

alias py='python3'
alias ip4='ip -4 -br addr show scope global'

alias lsfs='lsblk -o NAME,FSTYPE,LABEL,FSUSED,FSUSE%,FSSIZE,MOUNTPOINTS'
alias lsgpt='lsblk -o NAME,TYPE,RM,SIZE,PARTUUID,PARTLABEL'

#-- FUNCTIONS ------------------------------------------------------------------

# __spawn <command> [<args>...]
#
#   Fork a command using setsid(1). This detaches the process from the terminal
#   and prevents it from receiving SIGHUP when the session leader exits.
#   The process itself is forked from a subshell to hide it from job control.
#
__spawn() {
  if [[ $# -eq 0 ]]; then
    err "expected command"
    return 2
  fi
  (setsid -- "$@" <&- &> /dev/null &)
}

adate() {
  date +%Y%m%d-%H%M%S "$@"
}

# display the function's arguments
argv() {
  echo "$# argument(s):"
  if [[ $# -gt 0 ]]; then
    printf '%q\n' "$@"
  fi
}

# display the number of arguments
argc() {
  echo "$# argument(s)"
}

colors() {
  local i
  for i in {0..255}; do
    printf '\033[38;5;%sm%3s ' $i $i
    ((i == 255 || (i + 1) % 20 == 0)) && printf '\033[0m\n'
  done
}

edit() {
  if [[ -n ${WAYLAND_DISPLAY-} ]] && command -v neovide &> /dev/null; then
    neovide --fork "$@"
  else
    nvim "$@"
  fi
}

editx() {
  edit "$@" && exit
}

f() {
  __spawn "$@"
}

fx() {
  __spawn "$@" && exit
}

lsenv() {
  env | LC_ALL=C sort
}

lspath() {
  local -
  local ifs ent
  ifs=$IFS
  IFS=:
  set -f
  for ent in $PATH; do
    echo "${ent/#"$HOME"/\~}"
  done
  IFS=$ifs
}

# mcd <dir>
#
#   Change to the specified directory, creating it if needed.
#
mcd() {
  if [[ $# -ne 1 ]]; then
    echo >&2 "usage: $FUNCNAME <dir>"
    return 2
  fi
  mkdir -p -- "$1" && cd -- "$1"
}

r() {
  echo "Reloading initialization script..."
  source ~/.bashrc
}

rot13() {
  tr 'A-Za-z' 'N-ZA-Mn-za-m'
}

#-- KEYBINDINGS ----------------------------------------------------------------

__lf() {
  local old dir
  old=$PWD
  dir=$(command lf -print-last-dir "$@") && cd "$dir"
  [[ $PWD = $old ]] || __status
}

__status() {
  echo -n "${__status@P}"
}

__time() {
  printf '\e[7m %(%a %m/%d/%Y)T -- %(%I:%M %p)T \e[0m\n'
}

bind -x '"\e[15~": gale_menu'
bind -x '"\e[[E": gale_menu'
bind -x '"\C-o": __lf'
bind -x '"\C-r": __status'
bind -x '"\C-t": __time'
bind -x '"\C-x?": echo "Last process exited $?"'

if [[ -n ${WAYLAND_DISPLAY-} ]]; then
  bind -x '"\C-xn": __spawn foot'
  bind -x '"\C-xe": __spawn neovide'
fi

#-- PROMPT ---------------------------------------------------------------------

__before_prompt() {
  if [[ $? -eq 0 ]]; then
    prompt_token='$'
  else
    prompt_token='!'
  fi
}

PROMPT_COMMAND=__before_prompt

# these colors work nicely on linux virtual consoles
if [[ -n ${SSH_CONNECTION-} ]]; then
  __status='\[\e]2;\u@\h \w\a\e[0;1;38;5;15;48;5;165m\] \u@\h \[\e[48;5;91m\] \w \[\e[0m\]\n'
  PS1=$__status'\[\e[1;38;5;171;48;5;54m\]$prompt_token\[\e[0m\] '
else
  __status='\[\e]2;\u@\h \w\a\e[0;1;38;5;15;48;5;196m\] \u@\h \[\e[48;5;124m\] \w \[\e[0m\]\n'
  PS1=$__status'\[\e[1;38;5;203;48;5;88m\]$prompt_token\[\e[0m\] '
fi
