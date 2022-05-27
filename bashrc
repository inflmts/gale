# quit unless interactive
# apparently needed for rcp or scp or something
case "$-" in *i*) ;; *) return ;; esac

__ps1_exit_status() {
  local n="$?"
  [ "$n" -gt 0 ] && printf ' ?%s' "$n"
}

# build a custom prompt
printf -v PS1 %s \
  '\[\e[0;1;31m\]' \
  '\u@\h ' \
  '\[\e[34m\]' \
  '\w' \
  '\[\e[33m\]' \
  '$(__ps1_exit_status)' \
  '\[\e[0m\]' \
  ' \$ '

#
# ALIASES & FUNCTIONS
#

alias ls='ls -hv --color=auto --group-directories-first'
alias lsa='ls -A'
alias ll='ls -l'
alias lla='ls -lA'
alias lt='tree -vC --dirsfirst'
alias lta='lt -a'

alias ex='exec'
alias grep='grep --color=auto'
alias pacman='pacman --color=auto'
alias date='date +"%a %Y-%m-%d %I:%M %p"'
alias lsblk='lsblk -o name,mountpoint,fstype,label,partlabel,fsused,fssize,fsuse%'
alias userctl='systemctl --user'

# change to new directory
cdnew() {
  if [ "$#" -eq 1 ]; then
    mkdir -p -- "$1" && cd -- "$1"
  else
    echo >&2 "usage: cdnew <dir>"
  fi
}

# change to directory with fzf
cdfzf() {
  if ! command -v fzf &>/dev/null; then
    echo >&2 "fzf not installed"
    return 1
  fi

  local shopt="$(shopt -p)"
  shopt -s nullglob
  shopt -u dotglob
  while dir="$(
    for dir in .. * .[!.]* ..?*; do
      [ -d "$dir" ] && printf "%s\n" "$dir"
    done | fzf --prompt "$PWD/" --no-sort --no-clear
  )"; do
    cd "$dir"
  done
  tput rmcup
  eval "$shopt"
}

udisks-mount() {
  udisksctl mount -b "$1"
}

udisks-eject() {
  udisksctl unmount -b "$1" && udisksctl power-off -b "$1"
}

# use tput to clear scrollback buffer on (u)rxvt
bind -m emacs -x '"\C-l": tput rs1 rs2 rs3 rf'
bind -m vi-command -x '"\C-l": tput rs1 rs2 rs3 rf'
bind -m vi-insert -x '"\C-l": tput rs1 rs2 rs3 rf'

# vim:ft=bash
