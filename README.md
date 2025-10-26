# gaki

Minmal TUI directory browser, inspired by `mc`, `ranger`, `yazi` or similar.

## 📦 Install
```shell
meson setup build
meson install -C build
```

## ✨ Features

- currently _only_ differentiates between _DIRECTORIES_ (or symlinks) and _REGULAR FILES_.
- preview of _DIRECTORIES_ and _REGULAR FILES_
- parent view
- async reads of: _DIRECTORIES_ (magenta bar), utilizing a task queue
- async reading of: _REGULAR FILES_ (blue bar), utilizing a task queue
- async input
- async output
- 10k+ fps _(`-O3 -march=native -flto=auto -DNDEBUG` and spamming random inputs in code)_
- remembers the selected index when browsing the directory tree
- mouse support
- sorted directory preview
- resizable window

## ⌨️ Hotkeys

- `q` exit program
- `h` exit folder
- `j` file down
- `k` file up
- `l` enter folder or file via. `$EDITOR`
- `t` create tab
- `L` next tab
- `H` previous tab

## SOON™

- search for file name
- filter for file name
- help for hotkeys
- remap hotkeys
- user defined program when "opening" files
- multi-select is basically already in the code - it just has no application so far
- rename file(s)
- multiple panels basically already in the code via. tabs, but I do want multi-panels as well
- other directory sorting methods
- maybe: integrate fzf?
- log/errors instead of hard quitting, when e.g. no EDITOR or child failed

