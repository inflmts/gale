add_setup_dep .config/gale/config

enable_systemd=$(galconf --get --default=no systemd) || exit
enable_sway=$(galconf --get --default=no sway) || exit
enable_hyprland=$(galconf --get --default=no hyprland) || exit
if [[ $enable_sway = yes ]] || [[ $enable_hyprland = yes ]]; then
  # if any of these are enabled, wayland is also automatically enabled
  enable_wayland=yes
else
  enable_wayland=$(galconf --get --default=no wayland) || exit
fi

# internal
add_symlink {.local/bin,.gale/internal}/galconf
add_symlink {.local/bin,.gale/internal}/galsetup
add_symlink {.local/bin,.gale/internal}/galupd

# bash
add_symlink .bashrc .gale/bash/bashrc
add_symlink .bash_profile .gale/bash/profile
add_symlink .inputrc .gale/bash/inputrc

# vim
add_symlink .vimrc .gale/vim/vimrc
add_symlink .local/bin/get-vim-plug .gale/vim/get-vim-plug

# lf
add_symlink {.config/lf,.gale/lf}/lfrc
add_symlink {.local/bin,.gale/lf}/gale-win-lf

# nnn
add_symlink {.local/bin,.gale/nnn}/n3
add_symlink {.local/bin,.gale/nnn}/gale-win-n3
add_symlink {.data/applications,.gale/nnn}/gale-win-n3.desktop

for plugin in terminal
do add_symlink ".config/nnn/plugins/$plugin" ".gale/nnn/plugins/$plugin"
done

# dialog
add_symlink .dialogrc .gale/dialog/dialogrc

# screen
add_symlink .screenrc .gale/screen/screenrc

# less
add_symlink .lesskey .gale/less/lesskey

# misc
add_symlink .local/bin/term .gale/bin/term
add_symlink .local/bin/stupid .gale/bin/stupid

if [[ $enable_wayland = yes ]]; then
  add_symlink .config/foot/foot.ini .gale/foot/foot.ini
  add_symlink .config/alacritty/alacritty.yml .gale/alacritty/alacritty.yml
  add_symlink .local/bin/wautosleep .gale/wayland/wautosleep
fi

if [[ $enable_sway = yes ]]; then
  add_symlink .config/sway/config .gale/sway/sway.conf
  add_symlink .local/bin/gale-sway .gale/sway/gale-sway
  add_symlink .local/bin/swaystatus .gale/sway/swaystatus
  add_symlink .local/bin/sway-output-mode .gale/sway/sway-output-mode
fi

if [[ $enable_hyprland = yes ]]; then
  add_symlink .config/hypr/hyprland.conf .gale/hypr/hyprland.conf
  add_symlink .local/bin/gale-hyprland .gale/hypr/gale-hyprland
fi
