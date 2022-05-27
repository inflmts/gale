" detect if vim-plug is installed
silent! if plug#begin()
  " install plugins
  Plug 'sainnhe/sonokai'
  Plug 'preservim/nerdtree'
  Plug 'fcpg/vim-altscreen'
  call plug#end()

  " apply custom colorscheme
  silent! colorscheme sonokai
endif

let g:NERDTreeShowHidden = 1
let g:NERDTreeDirArrowExpandable = "+"
let g:NERDTreeDirArrowCollapsible = "-"
let g:NERDTreeCascadeSingleChildDir = 0

"
" OPTIONS
"

" backspace over everything
set backspace=indent,eol,start
" indentation
set et sw=2 sts=2 sta ai si
" search
set ic is
" vertical split to the right
set splitright
" single space between joins
set nojoinspaces
" scroll
set scrolloff=5 sidescroll=5 sidescrolloff=5
" display
set number showcmd nowrap title
" automatically insert comment string (ro)
" correct comment join (j)
set formatoptions+=roj
" enable the mouse
set mouse=a

" m   disable menu bar
" M   don't source menu.vim
" t   disable tearoff menu items
" T   disable toolbar
" l/L disable left scrollbar (in split windows)
" r/R disable right scrollbar (in split windows)
set go-=m go-=M go-=t go-=T go-=l go-=L go-=r go-=R

"
" KEYBINDINGS
"

" pseudovim bindings in command line
" use <C-C> to cancel instead of <Esc>
cnoremap <Esc>i <Home>
cnoremap <Esc>a <End>
cnoremap <Esc>h <Left>
cnoremap <Esc>l <Right>
cnoremap <Esc>b <S-Left>
cnoremap <Esc>w <S-Right>
cnoremap <Esc>j <PageDown>
cnoremap <Esc>k <PageUp>
cnoremap <Esc>x <Del>

nnoremap < <<
nnoremap > >>
nnoremap <C-S> :w<CR>
nnoremap <C-T> :NERDTreeToggle<CR>

vnoremap < <gv
vnoremap > >gv

"
" MISCELLANEOUS
"

hi Normal       ctermbg=NONE
hi EndOfBuffer  ctermbg=NONE
hi Terminal     ctermfg=NONE ctermbg=NONE
hi Comment      cterm=NONE
hi LineNr       ctermbg=233

" color trailing whitespace red
hi TrailSpace   ctermbg=1
match TrailSpace /\s\+$/
au WinNew * match TrailSpace /\s\+$/

if $TERM == "linux"
  hi Visual cterm=reverse
endif

au BufRead README* setl tw=80
au BufRead Makefile setl noet

" vim:ft=vim
