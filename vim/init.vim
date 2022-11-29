" Vim initialization file.
"
" Author: Daniel Li
"
" The following functions, if defined, affect the behavior of this file:
"
"   GalePlug()    register vim-plug plugins
"

" OPTIONS
" ==============================================================================

" backspace over everything
set backspace=indent,eol,start
" indentation
set et sw=2 sts=2 sta ai si
" searching
set is ic smartcase
" vsplit appears on right
set splitright
" join always leaves exactly one space
set nojs
" scrolling
set nowrap so=5 ss=10 siso=10
" display
set number showcmd title fillchars=vert:│
" automatically insert comment leader on <Enter> (r) and o/O (o)
" remove comment leaders on join (j)
set formatoptions+=roj
" automatically re-read a file if it is changed outside of vim
set autoread
" command line completion shows possibilities in a menu
set wildmenu
" added in Vim 9:
" use a popup menu for command line completions
silent! set wildoptions=pum

" m     disable menu bar
" M     don't source menu.vim
" t     disable tearoff menu items
" T     disable toolbar
" l/L   disable left scrollbar (in split windows)
" r/R   disable right scrollbar (in split windows)
set go-=m go-=M go-=t go-=T go-=l go-=L go-=r go-=R
" enable the mouse in all modes
set mouse=a

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

" GENERAL
" ==============================================================================

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

if &term == "foot" || &term == "alacritty"
  set ttyfast
endif

au FileType sh setlocal tw=80 fo-=t
au FileType markdown setlocal tw=80 et sw=2 sts=2
au FileType make setlocal noet
au BufRead README* setlocal tw=80

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
