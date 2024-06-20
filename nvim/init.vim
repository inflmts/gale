"
" Nvim initialization file
"
" This file is part of Gale.
"

" VIM-PLUG
"=======================================

silent! if plug#begin()
  Plug 'sainnhe/sonokai'
  Plug 'preservim/nerdtree'
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
set is ic
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
" insert mode completion
set completeopt=menu,preview,longest
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

" in normal mode, CTRL-X clears search highlighting (see :nohlsearch)
" otherwise, CTRL-X acts as a general escape key
nnoremap <C-X> <Cmd>noh<CR>
inoremap <C-X> <Esc>
cnoremap <C-X> <C-C>
xnoremap <C-X> <Esc>
snoremap <C-X> <Esc>
onoremap <C-X> <Esc>

" space and backspace scroll the window, like info
nnoremap <Space> <C-D>
xnoremap <Space> <C-D>
nnoremap <BS> <C-U>
xnoremap <BS> <C-U>

" j and k operate in screen lines (gj and gk)
" this is useful for wrapped text
nnoremap j gj
xnoremap j gj
nnoremap k gk
xnoremap k gk

" CTRL-S saves the file
nnoremap <C-S> <Cmd>up<CR>
xnoremap <C-S> <Cmd>up<CR>
inoremap <C-S> <Cmd>up<CR><Esc>

" H, M, and L scroll the window so that the cursor is at the top, middle, or
" bottom of the window, respectively
nnoremap H zt
xnoremap H zt
nnoremap M zz
xnoremap M zz
nnoremap L zb
xnoremap L zb

" save a few keystrokes for (un)indenting
nnoremap < <<
nnoremap > >>
xnoremap < <gv
xnoremap > >gv

" readline key bindings for command-line mode
cnoremap <C-A> <Home>
cnoremap <C-E> <End>
cnoremap <C-D> <Del>
cnoremap <C-B> <Left>
cnoremap <C-F> <Right>
cnoremap <C-P> <Up>
cnoremap <C-N> <Down>

" NERDTree bindings
nnoremap <C-T> <Cmd>NERDTreeToggle<CR>

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

  " color trailing whitespace red, because if you have trailing whitespace then
  " the code style witch will get you
  function s:gale_highlight_trailspace()
    if !exists('w:gale_trailspace_match_id')
      let w:gale_trailspace_match_id = matchadd('TrailSpace', '\s\+$', -1)
    endif
  endfunction

  tabdo windo call s:gale_highlight_trailspace()
  auto WinEnter * call s:gale_highlight_trailspace()
  hi TrailSpace ctermbg=1 guibg=#ff0000

augroup END

" MISCELLANEOUS
"=======================================

hi LineNr ctermbg=233 guibg=#222226
hi Error ctermfg=7 ctermbg=203
hi ErrorText cterm=underline ctermfg=203
hi! link SignColumn LineNr


if $TERM ==# "linux"
  hi Visual cterm=reverse
endif

command! R execute "source " .. fnameescape($MYVIMRC)

command! Galinst !galinst

" vim:ft=vim
