" Gale color scheme for Vim

hi clear
if exists("syntax_on")
  syntax reset
endif
let g:colors_name = "gale"

if has("gui_running")
  hi Normal guifg=#e7e7f0 guibg=#202028
  hi NormalNC guifg=#e7e7f0 guibg=#101014
else
  hi Normal ctermfg=NONE ctermbg=NONE guifg=NONE guibg=NONE
  hi NormalNC ctermfg=NONE ctermbg=NONE guifg=NONE guibg=NONE
endif

hi ModeMsg gui=bold guifg=NONE
hi NonText guifg=#54545f
hi StatusLine guifg=#000000 guibg=#f0f0f0
hi StatusLineNC guifg=#f0f0f0 guibg=#303036
hi Title cterm=bold gui=bold guifg=#d598ff
hi Whitespace guibg=#404048
hi WinSeparator guifg=#303036

hi! link LineNr NonText
hi! link SignColumn NonText

hi Comment guifg=#999999
hi Constant guifg=#ffb760
hi Delimiter guifg=#c0c0d0
hi Error guifg=#ff5d7c guibg=#501030
hi Operator guifg=#ff5d7c
hi PreProc guifg=#70d9f0
hi Statement cterm=bold gui=bold guifg=#ff5d7c
hi String guifg=#ffe070
hi Type guifg=#d598ff

hi! link Character String
hi! link Function Identifier
hi! link Identifier Type
hi! link Noise Delimiter
hi! link Quote Delimiter
hi! link Special Constant
