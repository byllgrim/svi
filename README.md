# svi
Simple Vi like text editor

This is an extention of [mvi](https://github.com/byllgrim/mvi).
For more information see the readme of mvi.

## Commands

    :[num]    - move to line num
    :d        - delete line
    :q        - quit
    :q!       - force quit
    :w [file] - write to file
    A         - append at end of line
    I         - insert at start of line
    h,j,k,l   - left, down, up, right
    i         - insert mode
    ESC       - normal mode

## Dependencies
* ncursesw
* [libutf](http://git.suckless.org/libutf/)
