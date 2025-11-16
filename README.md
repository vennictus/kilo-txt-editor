# kilo-txt-editor - A Tiny Text Editor in Pure C

[![Language](https://img.shields.io/badge/language-C-blue)]()
[![Build](https://img.shields.io/badge/build-GCC-red)]()
[![Terminal](https://img.shields.io/badge/platform-Linux%2FUnix-lightgrey)]()
[![Status](https://img.shields.io/badge/status-Complete-brightgreen)]()

---

> **A lightweight, minimal text editor built entirely from scratch in C.**
> Built using raw terminal handling, escape codes, custom rendering, and low-level file operations — no external libraries, no dependencies.

---

## ❤️ Features

*  Insert/delete characters
*  Proper backspace behavior (with line merging)
*  Newline insertion & clean row splitting
*  Dirty-flag tracking
*  Cursor navigation (Arrows, Home/End, PgUp/PgDn)
*  Smooth scrolling (horizontal & vertical)
*  Tab rendering with correct cursor alignment
*  Open, save, and "Save As"
*  Incremental search with highlight & navigation
*  Syntax highlighting for C/C++ (keywords, comments, strings, numbers)
*  Highlight restoration when exiting search
*  Status bar, message bar & welcome screen
*  Quit protection when unsaved

---

## 🧰 Build & Run

```bash
gcc -o kilo kilo.c -Wall -Wextra -pedantic
./kilo filename.txt
```

Run without a file:

```bash
./kilo
```

---

## 🎹 Keybindings

| Key               | Action                           |
| ----------------- | -------------------------------- |
| **Ctrl-S**        | Save                             |
| **Ctrl-X**        | Quit (with unsaved confirmation) |
| **Ctrl-Y**        | Search                           |
| **Arrow Keys**    | Move cursor                      |
| **Home/End**      | Jump to line boundaries          |
| **PgUp/PgDn**     | Fast scroll                      |
| **Backspace/Del** | Delete                           |
| **Enter**         | Insert newline                   |

---

## 📁 Project Structure

```
kilo-txt-editor/
 ├── kilo.c
 ├── README.md
 └── test files (optional)
```

---

## 📜 License

Based on the original Kilo tutorial by Salvatore Sanfilippo (antirez).


You are free to modify, use, and extend this implementation.


MIT License © 2025 Vennictus

