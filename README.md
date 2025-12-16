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
- `J` preview down _(text or directory)_
- `K` preview up _(text or directory)_
- `l` enter folder
- `t` create tab
- `L` next tab
- `H` previous tab
- `/` clear search & enter search
- `?` enter search
- `f` clear filter & enter filter
- `F` enter filter
- `.` toggle dot file visibility
- `p` toggle preview fullscreen
- `enter` enter file _(text,png,jpg,mkv,mp4)_

### Text input

For e.g. search or filter:

- Confirm with `enter`
- Clear + exit text with `esc`

## SOON™

### I-want-this-now-prio (100%-unchanging-roadmap®)

- v0.0.6 fullscreen preview toggle
- v0.0.6 shortcut system, which should allow for..
    - ..help listing for hotkeys
    - ..remapping hotkeys

- v0.0.7 tabs with pinned ones
- v0.0.7 grep support with preview (I have a vision of it)
- v0.0.7 rename file(s), move file(s), delete file(s)

- v0.0.8 use case for multi select: _(it is already coded, just not visualized, it has no functionality)_
    - allow custom launch args for when launching with multi-select
    - user defined program when opening files

- whenever: other directory sorting methods
- whenever: update directory/files on external change
- **(probably asap)** log/errors instead of hard quitting, when e.g. no EDITOR or child failed
- **(probably asap)** when long path/filename .. do not only show beginning

### lower prio

- basic syntax highlighting in text?
- multiple panels basically already in the code via. tabs, but I do want multi-panels as well

- video preview, scrollable
- image preview via kitty protocol, pdf preview?
- quit and pushd/cd into directory (with helper script)

- input handling, detectable press/release for keys?

- think about: adding infrastructure for piping custom lists/sublists into gaki, would be cool (use case?)
    - functionality through cli
    - should provide a library as well


