cd /home/${USER}/Development/linux-stable/
:cs add /home/${USER}/Development/linux-stable/cscope.out
:set tags=/home/${USER}/Development/linux-stable/tags

set tabstop=8 softtabstop=8 shiftwidth=8 noexpandtab cindent cc=80 | %retab | autocmd BufWritePre * %s/\s\+$//e

" 80 characters line
set colorcolumn=81
"execute "set colorcolumn=" . join(range(81,335),',')
highlight ColorColumn ctermbg=Black ctermfg=DarkRed

" Highlight trailing spaces
" http://vim.wikia.com/wiki/Highlight_unwanted_spaces
highlight ExtraWhitespace ctermbg=red guibg=red
match ExtraWhitespace /\s\+$/
autocmd BufWinEnter * match ExtraWhitespace /\s\+$/
autocmd InsertEnter * match ExtraWhitespace /\s\+\%#\@<!$/
autocmd InsertLeave * match ExtraWhitespace /\s\+$/
autocmd BufWinLeave * call clearmatches()

