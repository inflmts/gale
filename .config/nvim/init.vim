"
" Neovim configuration file
"
" This works on both Linux and Windows.
"

"-- PLUGINS ------------------------------------------------------------------

" load vim-plug if available
if !v:vim_did_enter
  runtime autoload/plug.vim
  if exists('*plug#begin')
    call plug#begin()
    Plug 'sainnhe/sonokai'
    Plug 'preservim/nerdtree'
    Plug 'neovim/nvim-lspconfig'
    Plug 'pangloss/vim-javascript'
    Plug 'alvan/vim-closetag'
    Plug 'HerringtonDarkholme/yats.vim'
    Plug 'brianhuster/live-preview.nvim'
    call plug#end()
  endif
endif

function s:install_vim_plug()
  " https://github.com/junegunn/vim-plug
  let path = stdpath('data') .. '/site/autoload/plug.vim'
  execute '!curl -Lf -o' shellescape(path, 1) '--create-dirs'
        \ '"https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim"'
endfunction

command! PlugSetup call s:install_vim_plug()

"-- OPTIONS ------------------------------------------------------------------

" indentation
set et sw=2 sts=2 sta ai si
" automatically re-read a file if it's changed externally
set autoread
" insert mode completion
set completeopt=menu,preview,longest
" always use unix line endings
set fileformats=unix
" formatting
set fo=tcqroj
" gui font
set guifont=Cascadia\ Mono:h10
" hide buffers when switching to another buffer
set hidden
" incremental search
set incsearch
" never insert two spaces after punctuation
set nojs
" break lines at 'breakat' if wrapping is enabled
set linebreak
" enable the mouse in all modes
set mouse=a
" show line numbers
set number
" this gets annoying with the j/k bindings below
set noshowcmd
" show some context around the cursor
set so=3 siso=6
" vsplit appears on right
set splitright
" set terminal/gui title
set title
" command line completion
set wildmenu wildmode=longest:full,full
" disable text wrapping
set nowrap

"-- KEYBINDINGS --------------------------------------------------------------

" See :h map-table for all map modes in a convenient table.

" In Normal mode, CTRL-X clears search highlighting (:nohlsearch) and clears
" the screen (CTRL-L), which has the side effect of removing any cruft left
" in the command-line area. Otherwise, CTRL-X acts as a general escape key,
" even in the terminal as I don't know many programs that use it.
nnoremap <C-X> <Cmd>noh<Bar>mode<CR>
inoremap <C-X> <Esc>
cnoremap <C-X> <C-C>
xnoremap <C-X> <Esc>
snoremap <C-X> <Esc>
onoremap <C-X> <Esc>
tnoremap <C-X> <C-\><C-N>

" space and backspace scroll the window, like info
nnoremap <Space> <C-F>
xnoremap <Space> <C-F>
nnoremap <BS> <C-B>
xnoremap <BS> <C-B>

" j and k operate in screen lines (gj and gk),
" while CTRL-J and CTRL-K do this but faster
" this is useful for wrapped text
nnoremap j gj
xnoremap j gj
nnoremap k gk
xnoremap k gk
nnoremap <C-J> 10gj
xnoremap <C-J> 10gj
nnoremap <C-K> 10gk
xnoremap <C-K> 10gk

" CTRL-S saves the file
nnoremap <C-S> <Cmd>up<CR>
xnoremap <C-S> <Cmd>up<CR>
inoremap <C-S> <Cmd>up<CR><Esc>
cnoremap <C-S> <Cmd>up<CR><C-C>

" H, M, and L scroll the window so that the cursor is at the top, middle, or
" bottom of the window, respectively
nnoremap H zt
xnoremap H zt
nnoremap M zz
xnoremap M zz
nnoremap L zb
xnoremap L zb

nnoremap z<BS> H
xnoremap z<BS> H
nnoremap z<CR> M
xnoremap z<CR> M
nnoremap z<Space> L
xnoremap z<Space> L

" save a few keystrokes for (un)indenting
nnoremap < <<
nnoremap > >>
xnoremap < <gv
xnoremap > >gv

" compensate for CTRL-X binding
nnoremap + <C-A>
xnoremap + <C-A>
nnoremap - <C-X>
xnoremap - <C-X>

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

" nvim diagnostics
nnoremap # <Cmd>lua vim.diagnostic.open_float()<CR>

" simpler copy and paste
vnoremap <C-S-C> "+y
inoremap <C-S-V> <C-R><C-O>+

"-- AUTOCOMMANDS -------------------------------------------------------------

augroup init
  " clear previously defined autocommands
  au!
  au FileType asciidoc setlocal nosi comments=fb:-,fb:*,fb://
  au FileType cs,java setlocal et sw=4 sts=4
  au FileType make setlocal noet sw=8 sts=8
  au FileType markdown setlocal et sw=2 sts=2 tw=80
  au FileType python,sh setlocal fo-=t
  au LspAttach * inoremap <buffer> <C-N> <C-X><C-O>
  au VimEnter,WinNew * call matchadd('Whitespace', '\s\+$')
augroup END

"-- OTHER SETTINGS -----------------------------------------------------------

let g:is_bash = 1

colorscheme gale

if has('gui_running')
  lua vim.lsp.enable('ts_ls')
endif

"-- NEOVIDE ------------------------------------------------------------------

" One of the reasons why I originally wanted to use Neovide is because it
" supports ligatures. But now I think it looks better without them.

let g:neovide_cursor_animation_length = 0.05
let g:neovide_cursor_trail_size = 0.5
