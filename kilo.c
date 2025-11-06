/*** includes ***/
#include <ctype.h>       // for isprint() and character classification
#include <errno.h>       // for errno and perror()
#include <stdio.h>       // for standard input/output functions
#include <stdlib.h>      // for malloc(), realloc(), free(), exit(), atexit()
#include <string.h>      // for memcpy() and memset()
#include <sys/ioctl.h>   // for ioctl() to get window size
#include <termios.h>     // for terminal I/O control
#include <unistd.h>      // for read(), write(), STDIN_FILENO

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
// Converts a key (like 'x') to its Ctrl-key equivalent (e.g., Ctrl-X → 24)

#define KILO_VERSION "0.0.1"

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/

// Structure to store editor state (cursor, screen size, terminal info)
struct editorConfig {
  int cx, cy;               // Cursor x and y positions
  int screenrows;           // Number of visible rows
  int screencols;           // Number of visible columns
  struct termios orig_termios; // Original terminal state
};

// Global instance of editor configuration
struct editorConfig E;

/*** terminal ***/

// Prints error, resets terminal, and exits
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);  // Clear screen
  write(STDOUT_FILENO, "\x1b[H", 3);   // Move cursor to top-left
  perror(s);
  exit(1);
}

// Disable raw mode and restore the terminal to its original settings
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

// Enable raw mode (no buffering, no echo, direct key reading)
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcgetattr");

  atexit(disableRawMode); // Ensure restoration on program exit

  struct termios raw = E.orig_termios;

  // Input flags: turn off special processing (flow control, CR to NL, etc.)
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

  // Output flags: disable post-processing (e.g., NL → CR/NL)
  raw.c_oflag &= ~(OPOST);

  // Control flags: force 8-bit characters
  raw.c_cflag |= (CS8);

  // Local flags: disable echo, canonical mode, Ctrl signals
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  // Set read() behavior: return after 0 bytes or 0.1s
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

// Read a single keypress (handles arrow keys as special sequences)
int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
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
// Get current cursor position using terminal query
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

// Get terminal window size using ioctl() or cursor fallback
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

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

// Append a string to dynamic buffer
void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

// Free dynamic buffer
void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

// Draw placeholder tildes (~) and a welcome message
void editorDrawRows(struct abuf *ab) {
  for (int y = 0; y < E.screenrows; y++) {
    if (y == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
        "Kilo editor -- version %s", KILO_VERSION);
      if (welcomelen > E.screencols) welcomelen = E.screencols;
      int padding = (E.screencols - welcomelen) / 2;
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--) abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "~", 1);
    }

    abAppend(ab, "\x1b[K", 3); // Clear line to right
    if (y < E.screenrows - 1)
      abAppend(ab, "\r\n", 2);
  }
}

// Refresh entire screen and reposition cursor
void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6);  // Hide cursor
  abAppend(&ab, "\x1b[H", 3);     // Move cursor to top
  editorDrawRows(&ab);

  // Move cursor to current position
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);  // Show cursor again
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** input ***/

// Move cursor based on arrow key
void editorMoveCursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      }
      break;
    case ARROW_RIGHT:
      if (E.cx != E.screencols - 1) {
        E.cx++;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy != E.screenrows - 1) {
        E.cy++;
      }
      break;
  }
}

// Handle keypress events (quit, arrows, etc.)
void editorProcessKeypress() {
  int c = editorReadKey();
  switch (c) {
    case CTRL_KEY('x'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

       case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      E.cx = E.screencols - 1;
      break;



      break;
    case PAGE_UP:
    case PAGE_DOWN:
      {
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
  }
}

/*** init ***/

// Initialize editor state
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
}

/*** main ***/

int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
