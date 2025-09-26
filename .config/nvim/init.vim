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
" incremental search
set incsearch
" formatting
set fo=tcqroj nojs
" scrolling
set nowrap so=5 ss=10 siso=10
" line numbers
set number
" automatically re-read a file if it's changed externally
set autoread
" vsplit appears on right
set splitright
" insert mode completion
set completeopt=menu,preview,longest
" command line completion
set wildmenu wildmode=longest:full,full
" hide buffers when switching to another buffer
set hidden
" enable the mouse in all modes
set mouse=a
" set terminal/gui title
set title
" always use unix line endings
set fileformats=unix
" do not attempt to use 24-bit color in the terminal
"set notermguicolors
" gui font
set guifont=Cascadia\ Mono:h10

"-- KEYBINDINGS --------------------------------------------------------------
"
" See `:h map-table` for all map modes in a convenient table.
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

" Use <S-Tab> to trigger omni completion.
" While the popup menu is open, <Tab> and <S-Tab> cycle through entries.
inoremap <expr> <Tab> pumvisible() ? "\<C-N>" : "\<Tab>"
inoremap <expr> <S-Tab> pumvisible() ? "\<C-P>" : "\<C-X><C-O>"

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
  au VimEnter,WinNew * call matchadd('Whitespace', '\s\+$')
augroup END

"-- OTHER SETTINGS -----------------------------------------------------------

let g:is_bash = 1

colorscheme gale

"-- NEOVIDE ------------------------------------------------------------------

if exists('g:neovide')
  let g:neovide_cursor_animation_length = 0.05
  let g:neovide_cursor_trail_size = 0.5
  " Neovide supports ligatures
  "set guifont=Cascadia\ Code:h10
endif

"-- RESTART ------------------------------------------------------------------
"
" This provides basic functionality to make restarting easier.
"

let s:restart_session_file = stdpath('state') .. '/restart-session.vim'

function s:restart() abort
  execute 'mksession! ' .. fnameescape(s:restart_session_file)
  qall
endfunction

function s:resume() abort
  execute 'source ' .. fnameescape(s:restart_session_file)
endfunction

" The :Restart command writes the session file and exits Nvim.
command! Restart call s:restart()

" The :Resume command loads the session file.
command! Resume call s:resume()

"-- LSP ----------------------------------------------------------------------

if has('gui_running')
lua << EOF
vim.lsp.enable('ts_ls')
EOF
endif
