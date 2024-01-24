# general
linkbin util/galinst
linkbin util/gale-sleep
linkbin util/gale-shutdown
linkbin util/gale-pid-start

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

# nnn
linkbin nnn/n3
linkbin nnn/gale-win-n3
link nnn/gale-win-n3.desktop .data/applications/gale-win-n3.desktop
link nnn/plugins/terminal .config/nnn/plugins/terminal

# screen
link screen/screenrc .screenrc

# vim
link vim/vimrc .vimrc
linkbin vim/get-vim-plug

# desktop
linkbin util/term

# wayland
link foot/foot.ini .config/foot/foot.ini
link alacritty/alacritty.yml .config/alacritty/alacritty.yml
linkbin wayland/wautosleep

# sway
link sway/sway.conf .config/sway/config
linkbin sway/gale-sway
linkbin sway/swaystatus
linkbin sway/sway-output-mode

# hyprland
link hyprland/hyprland.conf .config/hypr/hyprland.conf
linkbin hyprland/gale-hyprland
linkbin hyprland/gale-hyprland-waybar

# cagebreak
link cagebreak/cagebreak.conf .config/cagebreak/config
