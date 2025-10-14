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
- async reads of: _DIRECTORIES_ (magenta bar), utilizing a task queue
- async reading of: _REGULAR FILES_ (blue bar), utilizing a task queue
- async input
- async output
- 10k+ fps
- remembers the selected index when browsing the directory tree
- vim-binds: `j` (down), `k` (up), `h` (left), `l` (right)
- mouse support
- sorted directory preview
- resizable window

### SOON™

- search for file name
- help for shortkeys
- remap shortkeys
- multi-select is basically already in the code - it just has no application so far
- multiple panels
- other directory sorting methods

