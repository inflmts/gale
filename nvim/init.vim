"
" Nvim initialization file
"
" This file is part of Gale.
"

" VIM-PLUG
"=======================================

silent! if plug#begin()
  Plug 'sainnhe/sonokai'
  "Plug 'yuezk/vim-js'
  Plug 'pangloss/vim-javascript'

  let g:sonokai_disable_italic_comment = 1
  let g:sonokai_transparent_background = 1
  let g:sonokai_spell_foreground = 'colored'
  let g:sonokai_disable_terminal_colors = 1
  let g:sonokai_better_performance = 1

  call plug#end()

  silent! colorscheme sonokai
endif

" OPTIONS
"=======================================

" indentation
set et sw=2 sts=2 sta ai si
" searching
set is ic smartcase
" editing
set fo=tcqroj nojs
" scrolling
set nowrap so=5 ss=10 siso=10
" display
set number title fillchars=vert:│
" automatically re-read a file if it's changed externally
set autoread
" vsplit appears on right
set splitright
" command line completion
set wildmenu wildmode=longest:full,full
" don't automatically hide windows
set nohidden

" enable the mouse in all modes
set mouse=a

" KEYBINDINGS
"=======================================
"
" TIP: See |map-table| to see all map modes in a convenient table.
"
" NOTE: As of now, the mappings below clobber Select mode. Use at your own risk.

" in normal mode, CTRL-X clears search highlighting (see :nohlsearch)
" otherwise, CTRL-X acts as a general escape key
nnoremap <C-X> <Cmd>noh<CR>
inoremap <C-X> <Esc>
cnoremap <C-X> <C-C>
xnoremap <C-X> <Esc>
onoremap <C-X> <Esc>

" space and backspace scroll the window
noremap  <Space> <C-D>
noremap  <BS> <C-U>

" make j and k operate in screen lines (gj and gk)
" this is useful for wrapped text
nnoremap j gj
xnoremap j gj
nnoremap k gk
xnoremap k gk

" CTRL-J and CTRL-K move the cursor fast.
" CTRL-N and CTRL-P move the cursor even faster.
noremap  <C-J> 5gj
noremap  <C-K> 5gk
noremap  <C-N> 10gj
noremap  <C-P> 10gk

" CTRL-S saves the file
noremap  <C-S> <Cmd>up<CR>

noremap  H z<CR>
noremap  M z.
noremap  L z-

" save a few keystrokes for (un)indenting
nnoremap < <<
nnoremap > >>
xnoremap < <gv
xnoremap > >gv

cnoremap <C-A>  <Home>
cnoremap <C-E>  <End>
cnoremap <C-D>  <Del>
cnoremap <C-B>  <Left>
cnoremap <C-F>  <Right>
cnoremap <C-P>  <Up>
cnoremap <C-N>  <Down>

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

inoremap <C-V>  <C-R><C-O>+
inoremap <C-A>  <C-V>

" GUI
"=======================================

if has('gui_running')
  if exists('g:neovide')
    " Neovide supports ligatures
    set guifont=Cascadia\ Code:h10
  else
    set guifont=Cascadia\ Mono:h10
  endif
endif

" NEOVIDE
"=======================================

if exists('g:neovide')
  let g:neovide_cursor_animation_length = 0.05
  let g:neovide_cursor_trail_size = 0.5
  let g:neovide_transparency = 0.8
endif

" AUTOCOMMANDS
"=======================================

augroup init
  " clear previously defined autocommands
  auto!

  auto FileType sh setlocal fo-=t
  auto FileType markdown setlocal et sw=2 sts=2 tw=80
  auto FileType make setlocal noet
  auto FileType asciidoc setlocal nosi comments=fb:-,fb:*,fb://
  auto FileType cs,java setlocal sw=4 sts=4

  auto WinNew * call s:gale_trailspace_match()
augroup END

" MISCELLANEOUS
"=======================================

hi LineNr ctermbg=233 guibg=#222226
hi Error ctermfg=7 ctermbg=203
hi ErrorText cterm=underline ctermfg=203
hi! link SignColumn LineNr

" color trailing whitespace red, because if you have trailing whitespace then
" the code style witch will get you
function s:gale_trailspace_match()
  if exists('w:gale_trailspace_match_id')
    call matchdelete(w:gale_trailspace_match_id)
  else
    let w:gale_trailspace_match_id = -1
  endif
  let w:gale_trailspace_match_id = matchadd('TrailSpace', '\s\+$', 100, w:gale_trailspace_match_id)
endfunction

tabdo windo call s:gale_trailspace_match()
hi TrailSpace ctermbg=1 guibg=#ff0000

if $TERM ==# "linux"
  hi Visual cterm=reverse
endif

command! R echomsg "Reloading..." | execute "source " .. fnameescape(stdpath('config') .. "/init.vim")

" vim:ft=vim
