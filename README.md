# svi
Simple Vi like text editor

This is an extension of [mvi](https://github.com/byllgrim/mvi).
For more information see the readme of mvi.

## Commands

    /[term]   - search for term
    :[num]    - move to line num
    :d        - delete line
    :q        - quit
    :q!       - force quit
    :w [file] - write to file
    A         - append at end of line
    I         - insert at start of line
    h,j,k,l   - left, down, up, right
    i         - insert mode
    n         - search downwards
    ESC       - normal mode

## Dependencies
* ncursesw
* [libutf](http://git.suckless.org/libutf/)
