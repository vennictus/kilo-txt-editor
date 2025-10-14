#include <ctype.h> //this is the library that contains the isprint function
#include <stdio.h> //this is the library that contains the standard input and output functions
#include <termios.h> //this is the library that contains the termios structure and functions
#include <unistd.h> //this is the library that contains the read function
#include <stdlib.h> //this is the library that contains the exit function

// what is raw mode? Raw mode is a mode where the terminal does not process input and output, meaning that characters are read one by one without waiting for a newline, and special characters like Ctrl-C are not interpreted. the other mode is called canonical mode, where the terminal processes input and output, meaning that characters are buffered until a newline is received, and special characters are interpreted.

// function to disable raw mode and restore the terminal to its original state when the program exits
struct termios orig_termios;
void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}


// Function to enable raw mode

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios); 
  // tcgetattr gets the current terminal attributes and stores them in the orig_termios structure to save the original state of the terminal
  atexit(disableRawMode); 
  // atexit registers the disableRawMode function to be called when the program exits, ensuring that the terminal is restored to its original state
  tcgetattr(STDIN_FILENO, &orig_termios);
  // tcgetattr gets the current terminal attributes and stores them in the orig_termios structure to save the original state of the terminal
  struct termios raw = orig_termios; 
  // copy the original terminal attributes to the raw structure
  raw.c_lflag &= ~(ECHO); 
  // disable echoing of input characters
  raw.c_lflag &= ~(ICANON); 
  // disable canonical mode
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); 
  // tcsetattr sets the terminal attributes to the values in the raw structure. TCSAFLUSH means to apply the changes after flushing any input that has not been read yet
}



int main() {
  enableRawMode(); 
  // enable raw mode
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    if (iscntrl(c)) {
      printf("%d\n", c);
    } else {
      printf("%d ('%c')\n", c, c);
    }
  }
  // read one character at a time from standard input (STDIN_FILENO) into the variable c, and continue until the character 'q' is read
  // our read funtion takes three arguments: the file descriptor to read from (STDIN_FILENO), a pointer to the buffer to store the read data (&c), and the number of bytes to read (1)
  // if the character is a control character (iscntrl(c) returns true), print its ASCII value as an integer
  return 0;
}