#!/bin/sh

set -ue
ws=$(swaymsg -t get_workspaces \
  | jq -re '[range(4;11)] - map(select(.focus | length > 0) | .num) | first')
case $1 in
  focus) swaymsg "workspace number $ws" ;;
  move) swaymsg "move container to workspace number $ws; workspace number $ws" ;;
esac
