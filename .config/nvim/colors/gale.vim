" Gale color scheme for Vim

hi clear
if exists("syntax_on")
  syntax reset
endif
let g:colors_name = "gale"

if has("gui_running")
  hi Normal guifg=#e7e7f0 guibg=#202024
  hi NormalNC guifg=#e7e7f0 guibg=#101012
else
  hi Normal ctermfg=NONE ctermbg=NONE guifg=NONE guibg=NONE
  hi NormalNC ctermfg=NONE ctermbg=NONE guifg=NONE guibg=NONE
endif

hi ModeMsg gui=bold guifg=NONE
hi NonText ctermfg=8 guifg=#555555
hi StatusLine guifg=#000000 guibg=#ffffff
hi StatusLineNC guifg=#aaaaaa guibg=#303036
hi Title cterm=bold ctermfg=13 gui=bold guifg=#d598ff
hi Whitespace guibg=#404048
hi WinSeparator guifg=#303036

hi! link LineNr NonText
hi! link SignColumn NonText
hi! link TabLine StatusLineNC
hi! link TabLineSel StatusLine

hi Comment ctermfg=5 guifg=#999999
hi Constant ctermfg=3 guifg=#ffb760
hi Delimiter guifg=#c0c0d0
hi Error guifg=#ff5d7c guibg=#501030
hi Operator guifg=#c0c0d0
hi PreCondit ctermfg=6 guifg=#50b2e0
hi PreProc ctermfg=14 guifg=#70d9f0
hi Special ctermfg=3 guifg=#ffb760
hi SpecialChar ctermfg=3 guifg=#ffb760
hi Statement cterm=bold ctermfg=9 gui=bold guifg=#ff5d7c
hi String ctermfg=11 guifg=#ffe070
hi Type ctermfg=13 guifg=#d598ff

hi! link Character String
hi! link Function Identifier
hi! link Identifier Type
hi! link Noise Delimiter
hi! link Quote Delimiter
