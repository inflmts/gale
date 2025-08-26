#
# Bash initialization file
#

# So apparently Bash when accessed over ssh will sometimes source ~/.bashrc
# even when the session is not interactive. In that case we have to be careful
# not to output anything or it might confuse the client program. The safest
# thing to do, as found in many bashrc templates, is to just...

# quit unless interactive
# apparently needed for rcp or scp or something
[[ $- != *i* ]] && return

# +H    disable history substitution with '!'
# -u    fail on undefined variable references
set +H -u

msg() { printf >&2 '%s\n' "$*"; }
warn() { printf >&2 '\033[1;33mwarning:\033[0m %s\n' "$*"; }
err() { printf >&2 '\033[1;31merror:\033[0m %s\n' "$*"; }

# setup colors for ls (and tree)
if gale_dircolors=$(dircolors 2>/dev/null); then
  eval "$gale_dircolors"
fi
unset gale_dircolors

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

  printf '\033[1mLaunch desktop:\033[0m [q] abort [s] sway [h] hyprland '
  read -rsN1 key
  printf '\r\033[K'

  case $key in
    s)
      gale-sway
      ;;
    h)
      gale-hyprland
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

# Bind F5 to the Gale menu
bind -x '"\e[15~": gale_menu'
bind -x '"\e[[E": gale_menu'

#-- FUNCTIONS & ALIASES --------------------------------------------------------

# gale_spawn <command> [<args>...]
#
#   Fork a command using setsid(1).
#
gale_spawn() {
  local path
  if ! path=$(type -P "$1") &>/dev/null; then
    err "command not found: $1"
    return 1
  fi
  shift
  setsid -- "$path" "$@" <&- &>/dev/null & disown
}

alias grep='grep --color=auto'
alias md='mkdir -pv'

alias ls='LC_COLLATE=C ls -hv --group-directories-first --color=auto'
alias la='ls -A'
alias lsa='ls -A'
alias ll='ls -l'
alias lla='ls -lA'

alias tree='LC_COLLATE=C tree --dirsfirst'
alias treea='tree -a'
alias treed='tree -d'
alias treeda='tree -da'

clock() {
  printf '%(%a %m/%d/%Y)T \033[1m%(%I:%M %p)T\033[0m\n'
}

bind -x '"\C-t": clock'

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

f() {
  if [[ $# -eq 0 ]]; then
    err "expected command"
    return 1
  fi
  gale_spawn "$@"
}

fx() {
  if [[ $# -eq 0 ]]; then
    err "expected command"
    return 1
  fi
  gale_spawn "$@" && exit
}

# include file system information
lsfs() {
  command lsblk -o NAME,FSTYPE,LABEL,FSUSED,FSUSE%,FSSIZE,MOUNTPOINTS "$@"
}

# include GPT partition information
lsgpt() {
  command lsblk -o NAME,TYPE,RM,SIZE,PARTUUID,PARTLABEL "$@"
}

# cdnew <dir>
#
#   Create (recursively) and change to directory <dir>.
#
cdnew() {
  if [[ $# -eq 1 ]]; then
    mkdir -vp -- "$1" && cd -- "$1"
  else
    msg 'usage: cdnew <dir>'
    return 1
  fi
}

gale_resolve_block_device() {
  case $1 in
    l=*)
      echo "/dev/disk/by-label/${1#*=}"
      ;;
    pl=*)
      echo "/dev/disk/by-partlabel/${1#*=}"
      ;;
    /*)
      echo "$1"
      ;;
    *)
      err "invalid block device: $1"
      return 1
      ;;
  esac
}

# udmount <source>
#
#   Mount <source> with udisksctl.
#
udmount() {
  local source
  if [[ $# -ne 1 ]]; then
    msg 'usage: udmount <source>'
    return 1
  fi
  source=$(gale_resolve_block_device "$1") || return
  udisksctl mount -b "$source"
}

# udumount <source>
#
#   Unmount <source> with udisksctl.
#
udumount() {
  local source
  if [[ $# -ne 1 ]]; then
    msg 'usage: udeject <source>'
    return 1
  fi
  source=$(gale_resolve_block_device "$1") || return
  udisksctl unmount -b "$source"
}

# lspath
#
#   List the contents of the $PATH environment variable.
#
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

# lsenv
#
#   Sorts and lists exported environment variables.
#
lsenv() {
  env | LC_ALL=C sort -t '=' -k 1
}

cdcode() {
  local name
  name=$(ls ~/code | fzf) && cd ~/code/"$name"
}

adate() {
  date +%Y%m%d-%H%M%S "$@"
}

rot13() {
  tr 'A-Za-z' 'N-ZA-Mn-za-m'
}

r() {
  echo "Reloading initialization script..."
  source ~/.bashrc
}

#-- PROMPT ---------------------------------------------------------------------

before_prompt() {
  # update terminal title
  printf '\033]2;%s\007' "${USER}@${HOSTNAME%%.*} ${PWD/#$HOME/\~}"
}

PROMPT_COMMAND=before_prompt

basic_prompt='\[\e[0;1;41m\] \u@\h \[\e[47;30m\] \w \[\e[0m\] ~$?\n\$ '
basic_ssh_prompt='\[\e[0;1;45m\] \u@\h \[\e[47;30m\] \w \[\e[0m\] ~$?\n\$ '
fancy_prompt='\[\e[0;1;37;48;5;196m\] \u@\h \[\e[48;5;124m\] \w \[\e[0;38;5;203m\] ~$?\[\e[0m\]\n\$ '
fancy_ssh_prompt='\[\e[0;1;37;48;5;129m\] \u@\h \[\e[48;5;55m\] \w \[\e[0;38;5;171m\] ~$?\[\e[0m\]\n\$ '

case $TERM in
linux|dumb)
  if [[ -n ${SSH_CONNECTION-} ]]; then
    PS1=$basic_ssh_prompt
  else
    PS1=$basic_prompt
  fi ;;
*)
  if [[ -n ${SSH_CONNECTION-} ]]; then
    PS1=$fancy_ssh_prompt
  else
    PS1=$fancy_prompt
  fi ;;
esac

# vim:ft=bash
