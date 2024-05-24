#!/bin/sh

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
