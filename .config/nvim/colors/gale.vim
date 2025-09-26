" Gale color scheme for Vim

hi clear
if exists("syntax_on")
  syntax reset
endif

hi LineNr guifg=#505068
hi ModeMsg gui=bold guifg=NONE
hi Normal ctermbg=NONE guifg=#e7e7f0 guibg=#202030
hi NormalNC ctermbg=NONE guibg=#181824
hi StatusLine guifg=#000000 guibg=#e7e7f0
hi StatusLineNC guibg=#303048
hi Title cterm=bold gui=bold guifg=#d598ff
hi Whitespace ctermbg=red guibg=#404060
hi WinSeparator guifg=#303048

hi! link SignColumn LineNr

hi Constant guifg=#ffb760
hi Delimiter guifg=#c0c0d0
hi Error ctermfg=7 ctermbg=203 guifg=#ff507c guibg=#501030
hi Function guifg=#40e7fd
hi Operator guifg=#fc5d7c
hi PreProc guifg=#70d9f0
hi Statement cterm=bold ctermfg=203 gui=bold guifg=#fc5d7c
hi String guifg=#ffe070
hi Type guifg=#d598ff

hi! link Character String
hi! link Function Identifier
hi! link Identifier Type
hi! link Noise Delimiter
hi! link Quote Delimiter
hi! link Special Constant

let g:colors_name = "gale"
