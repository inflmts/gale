#!/usr/bin/env bash
# simple block device monitor
#
# Requirements:
#   - bash
#   - udevadm
#   - findmnt
#   - lsblk
#

set -u

fifo=$(mktemp -u)
mkfifo "$fifo"
udevadm monitor -u -s block > "$fifo" & udevadm_pid=$!
findmnt --poll=mount,umount -nro action > "$fifo" & findmnt_pid=$!
exec < "$fifo"
rm "$fifo"
trap 'kill "$udevadm_pid" "$findmnt_pid"; exit' EXIT HUP INT QUIT PIPE TERM

while :; do
  # gobble up the rest of the output
  while read -r -N 1024 -t 0.25 _tmp; do :; done

  lsblk -nro name,rm,hotplug,fstype,mountpoint | {
    output=
    while read -r name rm hotplug fstype mountpoint; do
      if [ -n "$fstype" ] && { [ "$rm" = 1 ] || [ "$hotplug" = 1 ]; }; then
        if [ -n "$mountpoint" ]; then
          output="$output <span color=\"#00ffff\">$name</span>"
        else
          output="$output $name"
        fi
      fi
    done
    printf '%s\n' "${output# }"
  }

  # wait for udevadm or findmnt to spit out something
  read -r -N 1 _tmp
done
