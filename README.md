# gaki

Minimal TUI directory browser, inspired by `mc`, `ranger`, `yazi` or similar.

## 📦 Install
```shell
meson setup build
meson install -C build
```

## ✨ Features

- currently _only_ differentiates between _DIRECTORIES_ (or symlinks) and _REGULAR FILES_.
- preview of _DIRECTORIES_ and _REGULAR FILES_ and _IMAGES_
- parent view
- async reads of: _DIRECTORIES_ (magenta bar), utilizing a task queue
- async reading of: _REGULAR FILES_ (blue bar), utilizing a task queue
- async input
- async draw
- 10k+ fps _(`-O3 -march=native -flto=auto -DNDEBUG` and spamming random inputs in code)_
- remembers the selected index when browsing the directory tree
- mouse support _(parent, current and preview pane + scrolling)_
- sorted directory preview _(via filename)_
- auto-resizable window
- edit regular files, open .mkv videos
- search and filter a directory and easily see filters in parent/child

## ⌨️ Hotkeys

- `q` exit program
- `h` exit folder
- `j` file down
- `k` file up
- `l` enter folder or file via. `$EDITOR`
- `t` create tab
- `L` next tab
- `H` previous tab
- `/` clear search & enter search
- `?` enter search
- `f` clear filter & enter filter
- `F` enter filter
- `.` toggle dot file visibility

### Text input

For e.g. search or filter:

- Confirm with `enter`
- Clear + exit text with `esc`

## SOON™

### I-want-this-now-prio

- update directory/files on external change

- help for hotkeys
- remap hotkeys
- log/errors instead of hard quitting, when e.g. no EDITOR or child failed
- user defined program when opening files
- multi-select is basically already in the code
- allow custom launch args for when launching with multi-select

- tabs with pinned ones... layout, how?

### lower prio

- scroll text preview up/down
- basic syntax highlighting in text?

- multiple panels basically already in the code via. tabs, but I do want multi-panels as well

- rename file(s), move file(s), delete file(s)
- other directory sorting methods

- video preview, scrollable
- image preview via kitty protocol, pdf preview?
- input handling, detectable press/release for keys?
- maybe: integrate fzf?


