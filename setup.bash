# internal
add_symlink .local/bin/galconf .gale/internal/galconf
add_symlink .local/bin/galsetup .gale/internal/galsetup
add_symlink .local/bin/galupd .gale/internal/galupd

# bash
add_symlink .bashrc .gale/bash/bashrc
add_symlink .bash_profile .gale/bash/profile
add_symlink .inputrc .gale/bash/inputrc

# vim
add_symlink .vimrc .gale/vim/vimrc
add_symlink .local/bin/get-vim-plug .gale/vim/get-vim-plug

add -g -o .vim/autoload/plug.vim \
    -c '.gale/bin/get-vim-plug' \
    -d 'Downloading vim-plug...'

# lf
add_symlink .config/lf/lfrc .gale/lf/lfrc
add_symlink .local/bin/gale-win-lf .gale/lf/gale-win-lf

# nnn
add_symlink {.local/bin,.gale/nnn}/n3
add_symlink {.local/bin,.gale/nnn}/gale-win-n3
add_symlink .data/applications/gale-win-n3.desktop .gale/nnn/gale-win-n3.desktop

for plugin in terminal
do add_symlink ".config/nnn/plugins/$plugin" ".gale/nnn/plugins/$plugin"
done

# dialog
add_symlink .dialogrc .gale/dialog/dialogrc

# screen
add_symlink .screenrc .gale/core/screenrc

# less
add_symlink .lesskey .gale/core/lesskey

# misc
add_symlink .local/bin/term .gale/bin/term
add_symlink .local/bin/stupid .gale/bin/stupid

if :; then # wayland
  add_symlink .config/foot/foot.ini .gale/foot/foot.ini
  add_symlink .config/alacritty/alacritty.yml .gale/alacritty/alacritty.yml
  add_symlink .local/bin/wautosleep .gale/wayland/wautosleep
fi

if :; then # sway
  add_symlink .config/sway/config .gale/sway/sway.conf
  add_symlink .local/bin/gale-sway .gale/sway/gale-sway
  add_symlink .local/bin/swaystatus .gale/sway/swaystatus
  add_symlink .local/bin/sway-output-mode .gale/sway/sway-output-mode
fi

if :; then # hyprland
  add_symlink .config/hypr/hyprland.conf .gale/hypr/hyprland.conf
  add_symlink .local/bin/gale-hyprland .gale/hypr/gale-hyprland
fi
