" ---
" ~/.config/nvim/init.vim
" ---
"
" This works on both Linux and Windows.

"-- PLUGINS ------------------------------------------------------------------

" load vim-plug if available
runtime autoload/plug.vim
if exists('*plug#begin')
  call plug#begin()
  source ~/.config/nvim/plug.vim
  call plug#end()
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
" don't automatically hide windows
set nohidden
" enable the mouse in all modes
set mouse=a
" show various types of whitespace
set list listchars=trail:\ ,tab:-->

" show title if the terminal supports it
if $TERM !=# "linux"
  set title
endif

" always use unix line endings
if has('win32')
  set fileformats=unix
else
  set fileformats=unix,dos
endif

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
nnoremap <Space> <C-D>
xnoremap <Space> <C-D>
nnoremap <BS> <C-U>
xnoremap <BS> <C-U>

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
  au FileType cs,java setlocal sw=4 sts=4
  au FileType make setlocal noet
  au FileType markdown setlocal et sw=2 sts=2 tw=80
  au FileType python,sh setlocal fo-=t

augroup END

"-- GUI ----------------------------------------------------------------------

if has('gui_running')
  if exists('g:neovide')
    " Neovide supports ligatures
    set guifont=Cascadia\ Code:h10
  else
    set guifont=Cascadia\ Mono:h10
  endif
endif

"-- NEOVIDE ------------------------------------------------------------------

if exists('g:neovide')
  let g:neovide_cursor_animation_length = 0.05
  let g:neovide_cursor_trail_size = 0.5
endif

"-- COLORS -------------------------------------------------------------------

let g:sonokai_disable_italic_comment = 1
if !has('gui_running')
  let g:sonokai_transparent_background = 1
  let g:sonokai_spell_foreground = 'colored'
endif
let g:sonokai_disable_terminal_colors = 1
let g:sonokai_better_performance = 1
silent! colorscheme sonokai

" color integration
hi Normal ctermbg=NONE guibg=#202030
hi NormalNC ctermbg=NONE guibg=#181824
hi StatusLine guibg=#34344e
hi StatusLineNC guibg=#28283c
hi Error ctermfg=7 ctermbg=203 guifg=#ff507c guibg=#501030
hi ErrorText cterm=underline ctermfg=203
hi! link SignColumn LineNr
hi Whitespace ctermfg=NONE ctermbg=1 guifg=#606090 guibg=#404060

if $TERM ==# "linux"
  hi Visual cterm=reverse ctermfg=NONE ctermbg=NONE
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

"-- GALLADE ------------------------------------------------------------------

function s:gallade() abort
  if exists('g:gallade_buffer') && bufexists(g:gallade_buffer)
    let switchbuf = &switchbuf
    let &switchbuf = 'useopen'
    execute 'sbuffer' g:gallade_buffer
    let &switchbuf = switchbuf
  else
    horizontal terminal gallade
    let g:gallade_buffer = bufnr()
    nnoremap <buffer> q <c-w>q
  endif
endfunction

command! Gallade call s:gallade()
nnoremap <c-g> <cmd>Gallade<cr>

"-- LSP ----------------------------------------------------------------------

lua << EOF
local ok
ok, lspconfig = pcall(require, 'lspconfig')
if ok then
  lspconfig.ts_ls.setup {}
end
EOF
