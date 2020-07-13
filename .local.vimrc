set makeprg=build.py
"set makeprg=build_non_unity.bat

nnoremap <silent> <leader>m :wa<CR>:call MakeAndShowQF()<CR>
nnoremap <silent> <leader>md :wa<CR>:call MakeAndShowQF('--dev')<CR>


