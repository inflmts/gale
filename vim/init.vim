"
" Vim initialization file.
"
" Author: Daniel Li
"
" The following functions, if defined, affect the behavior of this file:
"
"   GalePlug()    register vim-plug plugins
"

" PLUGINS
" ==============================================================================

" detect if vim-plug is installed
silent! if plug#begin()
  " install plugins
  Plug 'sainnhe/sonokai'
  Plug 'preservim/nerdtree'
  "Plug 'fcpg/vim-altscreen'

  let g:sonokai_disable_italic_comment = 1
  let g:sonokai_transparent_background = 1
  let g:sonokai_spell_foreground = 'colored'
  let g:sonokai_disable_terminal_colors = 1
  let g:sonokai_better_performance = 1

  let g:NERDTreeShowHidden = 1
  let g:NERDTreeDirArrowExpandable = "+"
  let g:NERDTreeDirArrowCollapsible = "-"
  let g:NERDTreeCascadeSingleChildDir = 0

  let g:lsp_diagnostics_enabled = 1
  let g:lsp_diagnostics_echo_cursor = 1

  hi LspErrorText ctermfg=203 ctermbg=233

  " run plugin hooks
  if exists("*GalePlug")
    call GalePlug()
  endif

  call plug#end()

  " apply custom colorscheme
  silent! colorscheme sonokai
endif

" OPTIONS
" ==============================================================================

" backspace over everything
set backspace=indent,eol,start
" indentation
set et sw=2 sts=2 sta ai si
" searching
set is ic smartcase
" editing
set fo+=roj tw=80 nojs
" scrolling
set nowrap so=5 ss=10 siso=10
" display
set number showcmd title fillchars=vert:│
" automatically re-read a file if it is changed outside of vim
set autoread
" vsplit appears on right
set splitright
" command line completion shows possibilities in a menu
set wildmenu
" added in Vim 9:
" use a popup menu for command line completions
if exists('&wildoptions')
  set wildoptions=pum
endif

" m     disable menu bar
" M     don't source menu.vim
" t     disable tearoff menu items
" T     disable toolbar
" l/L   disable left scrollbar (in split windows)
" r/R   disable right scrollbar (in split windows)
set go-=m go-=M go-=t go-=T go-=l go-=L go-=r go-=R
" enable the mouse in all modes
set mouse=a

" Use $XDG_CACHE_HOME/vim for swap, backup, and undo files.
let s:cache_dir = ($XDG_CACHE_HOME ?? ($HOME .. "/.cache")) .. "/vim"
let &dir = s:cache_dir .. "/swap//"
let &backupdir = s:cache_dir .. "/backup//"
let &undodir = s:cache_dir .. "/undo//"

" Create these directories if they don't exist.
call mkdir(&dir, 'p')
call mkdir(&backupdir, 'p')
call mkdir(&undodir, 'p')

" KEYBINDINGS
" ==============================================================================

" space and backspace scroll the window
noremap <Space> <C-D>
noremap <BS> <C-U>

noremap <Leader>wl "=system('wl-paste -n')<CR>

nnoremap < <<
nnoremap > >>
xnoremap < <gv
xnoremap > >gv

cnoremap <C-A>  <Home>
cnoremap <C-E>  <End>
cnoremap <C-D>  <Del>
cnoremap <Esc>h <Left>
cnoremap <M-h>  <Left>
cnoremap <Esc>l <Right>
cnoremap <M-l>  <Right>
cnoremap <Esc>j <PageDown>
cnoremap <M-j>  <PageDown>
cnoremap <Esc>k <PageUp>
cnoremap <M-k>  <PageUp>
cnoremap <Esc>b <S-Left>
cnoremap <M-b>  <S-Left>
cnoremap <Esc>f <S-Right>
cnoremap <M-f>  <S-Right>

nnoremap <C-T> :NERDTreeToggle<CR>
nnoremap <Leader>h :LspHover<CR>

" AUTOCOMMANDS
" ==============================================================================

au FileType sh setlocal fo-=t
au FileType markdown setlocal et sw=2 sts=2
au FileType make setlocal noet

" GENERAL
" ==============================================================================

filetype plugin indent on
syntax enable

hi LineNr ctermbg=233 guibg=#222226
hi Error ctermfg=7 ctermbg=203
hi ErrorText cterm=underline ctermfg=203
hi! link SignColumn LineNr

" color trailing whitespace red, since trailing space is death
hi TrailSpace ctermbg=1 guibg=#ff0000
au VimEnter * match TrailSpace /\s\+$/
au WinNew * match TrailSpace /\s\+$/

if &term == "linux"
  hi Visual cterm=reverse
endif

if &term =~ '^\(foot\|alacritty\)\(-\|$\)'
  set ttyfast
endif

if has('linux')
  " :Fork {command}
  "
  "   Fork a command using setsid(1).
  "
  " For some reason, this sometimes messes up the screen. Use redraw to fix.
  command! -nargs=1 -complete=shellcmd Fork
        \ silent execute "!setsid -f >/dev/null 2>&1"
        \ &shell &shellcmdflag shellescape(<q-args>, 1) | redraw!
endif

" :R
"
"   Reload vim (that is, re-source ~/.vimrc).
"
command! R source ~/.vimrc

" ninja
function! s:ninja()
  let ninja_command = "!ninja"
  if exists("g:ninja_dir") && g:ninja_dir != "."
    let ninja_command = ninja_command .. " -C " .. shellescape(g:ninja_dir, 1)
  endif
  if exists("g:ninja_file")
    let ninja_command = ninja_command .. " -f " .. shellescape(g:ninja_file, 1)
  endif
  execute ninja_command
endfunction

command! -nargs=1 -complete=dir NinjaDir let g:ninja_dir = <q-args>
command! -nargs=1 -complete=file NinjaFile let g:ninja_file = <q-args>
command! -nargs=? Ninja call <SID>ninja()

nnoremap <Leader>n :call <SID>ninja()<CR>

" vim:ft=vim
