```
 _  ___ _       
| |/ (_) | ___  
| ' /| | |/ _ \
| . \| | | (_) |
|_|\_\_|_|\___/

A tiny text editor written in pure C
```

# kilo-txt-editor - A Tiny Text Editor in Pure C

[![Language](https://img.shields.io/badge/language-C-blue)]()
[![Build](https://img.shields.io/badge/build-GCC-red)]()
[![Terminal](https://img.shields.io/badge/platform-Linux%2FUnix-lightgrey)]()
[![Status](https://img.shields.io/badge/status-Complete-brightgreen)]()

---

A lightweight, minimal text editor built entirely from scratch in C.

Built using raw terminal handling, escape codes, custom rendering, and low-level file operations — no external libraries, no dependencies.

---

## Features

* Insert and delete characters
* Proper backspace behavior with line merging
* Newline insertion and clean row splitting
* Dirty-flag tracking
* Cursor navigation (arrow keys, Home/End, Page Up/Page Down)
* Smooth vertical and horizontal scrolling
* Tab rendering with correct cursor alignment
* Open, save, and "Save As" support
* Incremental search with live highlighting and navigation
* Syntax highlighting for C and C++ (keywords, comments, strings, numbers)
* Highlight restoration when exiting search mode
* Status bar, message bar, and welcome screen
* Quit protection when unsaved changes exist

---

## Build and Run

```bash
gcc -o kilo kilo.c -Wall -Wextra -pedantic
./kilo filename.txt
```

Run without a file:

```bash
./kilo
```

---

## Keybindings

| Key             | Action                           |
| --------------- | -------------------------------- |
| Ctrl-S          | Save                             |
| Ctrl-X          | Quit (with unsaved confirmation) |
| Ctrl-Y          | Search                           |
| Arrow Keys      | Move cursor                      |
| Home / End      | Jump to line boundaries          |
| Page Up / Down  | Fast scroll                      |
| Backspace / Del | Delete                           |
| Enter           | Insert newline                   |

---

## Project Structure

```
kilo-txt-editor/
 ├── kilo.c
 ├── README.md
 └── test files (optional)
```

---

## License

Based on the original Kilo editor tutorial by Salvatore Sanfilippo (antirez).

You are free to modify, use, and extend this implementation.

MIT License © 2025 Vennictus
