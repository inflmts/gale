linkbin galinst

# general
linkbin util/gale-sleep
linkbin util/gale-shutdown
linkbin util/gale-reboot
linkbin util/gale-pid-start

# sh
link sh/profile.sh .profile

# bash
link bash/bashrc .bashrc
link bash/profile .bash_profile
link bash/inputrc .inputrc

# dialog
link dialog/dialogrc .dialogrc

# less
link less/lesskey .lesskey

# lf
link lf/lfrc .config/lf/lfrc
link lf/colors .config/lf/colors
linkbin lf/gale-win-lf
link lf/gale-win-lf.desktop .data/applications/gale-win-lf.desktop

# neovim
link nvim/init.vim .config/nvim/init.vim

# screen
link screen/screenrc .screenrc

# vim
link vim/vimrc .vimrc
linkbin vim/get-vim-plug

# desktop
linkbin util/term
linkbin util/gale-explorer

# foot
link foot/foot.ini .config/foot/foot.ini

# alacritty
link alacritty/alacritty.toml .config/alacritty/alacritty.toml

# wayland
linkbin wayland/wautosleep

# sway
link sway/config .config/sway/config
linkbin sway/gale-sway
linkbin sway/gale-sway-outputs

if [ "$profile" = archiplex ]; then
  link archiplex/outputs .data/gale/outputs
fi

# hyprland
link hyprland/hyprland.conf .config/hypr/hyprland.conf
linkbin hyprland/gale-hyprland
linkbin hyprland/gale-hyprland-waybar

# cagebreak
link cagebreak/cagebreak.conf .config/cagebreak/config
