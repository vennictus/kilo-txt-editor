#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L   // Enables POSIX 2008 features (for getline)

/*** defines ***/

// Converts a normal key (e.g., 'x') into its Ctrl-key equivalent (e.g., Ctrl-X)
#define CTRL_KEY(k) ((k) & 0x1f)

#define KILO_VERSION "0.0.1"      // Version string for the editor
#define KILO_TAB_STOP 8           // Number of spaces per tab when rendering
#define KILO_QUIT_TIMES 3         // Number of times to confirm quit if unsaved changes exist

// Enum for non-ASCII keys, starting from 1000 to avoid collision with ASCII codes
enum editorKey {
  ARROW_LEFT = 1000,
  BACKSPACE = 127,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/*** includes ***/
#include <ctype.h>       // For isprint(), iscntrl(), etc.
#include <errno.h>       // For errno and perror()
#include <stdio.h>       // For standard input/output functions
#include <stdlib.h>      // For malloc(), realloc(), free(), exit(), atexit()
#include <string.h>      // For memcpy(), memset(), strdup()
#include <sys/ioctl.h>   // For ioctl() to get terminal window size
#include <sys/types.h>   // For basic system data types
#include <termios.h>     // For terminal I/O configuration
#include <unistd.h>      // For read(), write(), STDIN_FILENO
#include <time.h>        // For time(), used in message bar timeout
#include <stdarg.h>      // For variable argument functions (status message formatting)
#include <fcntl.h>       // For open() flags

/*** data ***/

// Represents one line (row) of text in the editor
typedef struct erow {
  int size;      // Number of characters in the row (not counting null terminator)
  int rsize;     // Rendered size (after expanding tabs into spaces)
  char *chars;   // The raw characters in the line
  char *render;  // The rendered version of the line
} erow;

// Global editor configuration (state)
struct editorConfig {
  int cx, cy;                 // Cursor position in characters (x = column, y = row)
  int rx;                     // Rendered cursor x position (accounts for tabs)
  int rowoff;                 // Vertical scroll offset (top visible row)
  int coloff;                 // Horizontal scroll offset (leftmost visible column)
  int screenrows;             // Number of visible rows in the terminal
  int screencols;             // Number of visible columns in the terminal
  int numrows;                // Number of rows currently in the file
  erow *row;                  // Array of rows (the text buffer)
  char *filename;             // Name of the open file
  char statusmsg[80];         // Message displayed on the status bar
  time_t statusmsg_time;      // Time when the status message was set
  struct termios orig_termios;// Stores original terminal attributes for restoration
  int dirty;                  // Flag indicating if the file has unsaved changes
};

// Global instance of editor configuration
struct editorConfig E;

/*** prototypes ***/
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen(void);
int editorReadKey(void);
char *editorPrompt(const char *prompt);

/*** terminal handling ***/

// Restores terminal and exits with an error message
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);  // Clear the screen
  write(STDOUT_FILENO, "\x1b[H", 3);   // Move cursor to top-left
  perror(s);                           // Print error message
  exit(1);
}

// Disables raw mode and restores the terminal's original settings
void disableRawMode(void) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

// Enables raw mode: disables canonical input, echo, and signal generation
void enableRawMode(void) {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcgetattr");

  atexit(disableRawMode);  // Restore terminal automatically on exit

  struct termios raw = E.orig_termios;

  // Disable input processing features (CR-to-NL, XON/XOFF, etc.)
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

  // Disable output processing (no automatic newline conversion)
  raw.c_oflag &= ~(OPOST);

  // Set character size to 8 bits
  raw.c_cflag |= (CS8);

  // Disable echo, canonical mode, extended input, and signals (Ctrl-C, etc.)
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  // Control behavior of read(): return after 0 bytes or 0.1 seconds
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  // Apply modified terminal settings
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

// Reads one keypress, handles escape sequences for arrow keys and others
int editorReadKey(void) {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  // Handle escape sequences (starting with '\x1b')
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

// Gets current cursor position using terminal query
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;

  return 0;
}

// Gets terminal window size using ioctl(); falls back to cursor method if needed
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** row operations ***/

// Converts a cursor x-position into a rendered x-position (accounts for tabs)
int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  for (int j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    rx++;
  }
  return rx;
}

// Updates the rendered version of a row (expands tabs into spaces)
void editorUpdateRow(erow *row) {
  int tabs = 0;
  for (int j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);

  int idx = 0;
  for (int j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % KILO_TAB_STOP != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }

  row->render[idx] = '\0';
  row->rsize = idx;
}

// Appends a new row of text to the editor buffer at position `at`
// this shifts existing rows down and inserts the new row. used for open and newline.
void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);
  E.numrows++;
  E.dirty++;
}

// Frees memory used by a row
void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
}

// Delete the row at position `at` and shift remaining rows up.
void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  E.numrows--;
  E.dirty++;
}

// Insert character c at position at inside a given row. resize and update render.
void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1); // include null
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

// Delete character at `at` inside row
void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

// Append a c-string of length len to the end of a row
void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

/*** editor operations ***/

// Insert a character at the current cursor position, creating a row if needed
void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    // we're at a new line after the last row; insert an empty row
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

// Insert a newline at the current cursor position: split current row or insert empty row
void editorInsertNewline(void) {
  if (E.cx == 0) {
    // cursor at start — insert empty row at current position
    editorInsertRow(E.cy, "", 0);
  } else {
    // split current row at cursor
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;
}

// Delete a character at the cursor (backspace behavior)
// if at start of line, append this row into previous and delete the row.
void editorDelChar(void) {
  if (E.cy == E.numrows) return;
  if (E.cx == 0 && E.cy == 0) return;

  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    // join this line with previous
    int prev_len = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
    E.cx = prev_len;
  }
}

/*** file I/O ***/

// Converts all rows to a single buffer with '\n' separators, returns pointer and sets buflen
char *editorRowsToString(int *buflen) {
  int totlen = 0;
  for (int j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1; // include '\n'

  *buflen = totlen;
  char *buf = malloc(totlen);
  char *p = buf;
  for (int j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

// Opens a file and reads its contents line by line
void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }

  free(line);
  fclose(fp);
  E.dirty = 0;
}

// Save buffer to disk. If no filename, prompt user (editorPrompt)
void editorSave(void) {
  if (E.filename == NULL) {
    char *fname = editorPrompt("Save as: %s (ESC to cancel)");
    if (fname == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    free(E.filename);
    E.filename = fname; // editorPrompt returns malloc'd string
  }

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd != -1) {
    if (write(fd, buf, len) == len) {
      close(fd);
      free(buf);
      E.dirty = 0;
      editorSetStatusMessage("%d bytes written to disk", len);
      return;
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** append buffer ***/

// Append buffer: accumulates screen output before writing it all at once
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

// Appends string s of length len to buffer ab
void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

// Frees the memory used by the buffer
void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

// Updates the scroll offsets to ensure cursor is within the visible window
void editorScroll(void) {
  E.rx = 0;
  if (E.cy < E.numrows)
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);

  if (E.cy < E.rowoff)
    E.rowoff = E.cy;
  if (E.cy >= E.rowoff + E.screenrows)
    E.rowoff = E.cy - E.screenrows + 1;

  if (E.rx < E.coloff)
    E.coloff = E.rx;
  if (E.rx >= E.coloff + E.screencols)
    E.coloff = E.rx - E.screencols + 1;
}

// Draws the visible rows (or ~ for empty lines)
void editorDrawRows(struct abuf *ab) {
  for (int y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      // Display a centered welcome message if file is empty
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Kilo editor -- version %s", KILO_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) { abAppend(ab, "~", 1); padding--; }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      // Draw a line of text (rendered, respecting coloff)
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;
      abAppend(ab, &E.row[filerow].render[E.coloff], len);
    }
    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

// Draws the status bar at the bottom of the screen
void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4); // inverted colors
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    E.filename ? E.filename : "[No Name]", E.numrows,
    E.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
    E.cy + 1, E.numrows);
  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3); // reset colors
  abAppend(ab, "\r\n", 2);
}

// Draws the message bar (used for temporary messages)
void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

// Refreshes the screen: redraws everything and repositions the cursor
void editorRefreshScreen(void) {
  editorScroll();
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6); // Hide cursor
  abAppend(&ab, "\x1b[H", 3);    // Move cursor to top-left
  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
    (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6); // Show cursor again
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

// Sets a status message to display for 5 seconds
void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** input ***/

// Moves cursor based on arrow key input
void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) E.cy--;
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows - 1) E.cy++; // ensure bounds (numrows-1 is last valid)
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) E.cx = rowlen;
}

// Processes a single keypress event
void editorProcessKeypress(void) {
  static int quit_times = KILO_QUIT_TIMES;

  int c = editorReadKey();
  switch (c) {
    case '\r':
      editorInsertNewline();
      break;
    case CTRL_KEY('x'):
      if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING!!! File has unsaved changes. Press Ctrl-X %d more times to quit.", quit_times);
        quit_times--;
        return;
      }
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
      editorDelChar();
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows - 1) E.cy = E.numrows - 1;
        }
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      editorInsertChar(c);
      break;
  }
  quit_times = KILO_QUIT_TIMES;
}

/*** prompt (used for save as and other user text input) ***/

// Prompt the user with `prompt` and read a line. returns a malloc'd string or NULL on cancel.
// This is a simple blocking prompt that uses the editor's input handling.
char *editorPrompt(const char *prompt) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  if (!buf) return NULL;
  size_t buflen = 0;

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == '\x1b') {
      editorSetStatusMessage("");
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        buf[buflen] = '\0';
        editorSetStatusMessage("");
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen + 1 >= bufsize) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
    } else if (c == BACKSPACE || c == CTRL_KEY('h')) {
      if (buflen != 0) buflen--;
    }
  }
}

/*** initialization ***/

// Initializes the editor state and gets terminal size
void initEditor(void) {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.dirty = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
  E.screenrows -= 2; // Reserve space for status and message bars
}

/*** main ***/

// Program entry point
int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();

  if (argc >= 2)
    editorOpen(argv[1]);

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-X = quit");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
