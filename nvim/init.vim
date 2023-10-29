"
" Nvim initialization file
"
" This file is part of Gale.
"

" Source local configuration from `~/.config/gale/nvim-init.vim`. Note that this
" occurs _before_ options, mappings, etc. are configured.
"
" These functions can be defined to implement custom behavior:
"
"   GalePlug()    run between plug#begin() and plug#end()
"   GaleLate()    run after everything else
"
let s:local_init_file = $HOME .. '/.config/gale/nvim-init.vim'
if filereadable(s:local_init_file)
  exe "source " .. fnameescape(s:local_init_file)
endif

silent! if plug#begin()
  Plug 'sainnhe/sonokai'

  let g:sonokai_disable_italic_comment = 1
  let g:sonokai_transparent_background = 1
  let g:sonokai_spell_foreground = 'colored'
  let g:sonokai_disable_terminal_colors = 1
  let g:sonokai_better_performance = 1

  call plug#end()

  silent! colorscheme sonokai
endif

" OPTIONS
" ==============================================================================

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

set guifont=Cascadia\ Code:h10
" enable the mouse in all modes
set mouse=a

" KEYBINDINGS
" ==============================================================================
"
" Tip: Use ':h map-table' to see map modes in a convenient table.

" Use CTRL-X act like Escape
noremap <C-X> <Esc>
noremap! <C-X> <Esc>

" Make space and backspace scroll the window
noremap <Space> <C-D>
noremap <BS> <C-U>

" CTRL-J and CTRL-K move the cursor fast.
" CTRL-N and CTRL-P move the cursor even faster.
noremap <C-J> 5j
noremap <C-K> 5k
noremap <C-N> 10j
noremap <C-P> 10k

noremap H z<CR>
noremap M z.
noremap L z-

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

" NEOVIDE
" ==============================================================================

if exists('g:neovide')
  let g:neovide_cursor_animation_length = 0.05
  let g:neovide_transparency = 0.8
endif

" AUTOCOMMANDS
" ==============================================================================

augroup init
  " Clear previously defined autocommands
  auto!

  auto FileType * setlocal tw=80
  auto FileType sh setlocal fo-=t
  auto FileType markdown setlocal et sw=2 sts=2 tw=80
  auto FileType make setlocal noet
  auto FileType asciidoc setlocal nosi comments=fb:-,fb:*,fb://

  " Color trailing whitespace red, because trailing whitespace is bad for your
  " heart
  set list
  set listchars=trail:\  " space
augroup END

hi Whitespace ctermbg=1 guibg=#ff0000

" GENERAL
" ==============================================================================

hi LineNr ctermbg=233 guibg=#222226
hi Error ctermfg=7 ctermbg=203
hi ErrorText cterm=underline ctermfg=203
hi! link SignColumn LineNr

if &term == "linux"
  hi Visual cterm=reverse
endif

command! R execute "source " .. fnameescape(stdpath('config') .. "/init.vim")

" vim:ft=vim
