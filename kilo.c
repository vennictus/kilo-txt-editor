/*** includes ***/
#include <ctype.h> //this is the library that contains the isprint function
#include <stdio.h> //this is the library that contains the standard input and output functions
#include <termios.h> //this is the library that contains the termios structure and functions
#include <unistd.h> //this is the library that contains the read function
#include <stdlib.h> //this is the library that contains the exit function
#include <errno.h> //this is the library that contains the perror function

// what is raw mode? Raw mode is a mode where the terminal does not process input and output, meaning that characters are read one by one without waiting for a newline, and special characters like Ctrl-C are not interpreted. the other mode is called canonical mode, where the terminal processes input and output, meaning that characters are buffered until a newline is received, and special characters are interpreted.

// function to disable raw mode and restore the terminal to its original state when the program exits
struct termios orig_termios;

/*** terminal ***/


//function to handle errors  
void die(const char *s) {
  perror(s);
  exit(1);
} 


// Function to disable raw mode
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}


// Function to enable raw mode

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr"); 
  // tcgetattr gets the current terminal attributes and stores them in the orig_termios structure to save the original state of the terminal
  atexit(disableRawMode); 
  // atexit registers the disableRawMode function to be called when the program exits, ensuring that the terminal is restored to its original state
  struct termios raw = orig_termios; 
  // copy the original terminal attributes to the raw structure
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  // disable various input flags:
  // BRKINT: disable break condition interrupt  
  // ICRNL: disable carriage return to newline translation on input
  // INPCK: disable parity checking
  // ISTRIP: disable stripping of the eighth bit
  // IXON: disable software flow control (Ctrl-S and Ctrl-Q)
  raw.c_oflag &= ~(OPOST);
  // disable output processing
    raw.c_cflag |= (CS8);
  // set character size to 8 bits per byte
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  // disable echoing of input characters 
  // disable canonical mode
  // disable extended input processing
  // disable signal characters (like Ctrl-C and Ctrl-Z)
  raw.c_cc[VMIN] = 0;   // set minimum number of bytes of input needed before read() can return to 0
  raw.c_cc[VTIME] = 1;  // set maximum time to wait before read() returns to 1 deciseconds (0.1 seconds) 
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr"); 
  // tcsetattr sets the terminal attributes to the values in the raw structure. TCSAFLUSH means to apply the changes after flushing any input that has not been read yet
  // the 3 arguments are: the file descriptor to set the attributes for (STDIN_FILENO), the action to take (TCSAFLUSH), and a pointer to the termios structure containing the new attributes (&raw)
}

/*** init ***/

int main() {
  enableRawMode(); 
  // enable raw mode
  //while 1 is an infinite loop that will continue until we break out of it
  while (1) {
    char c = '\0';
   if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
    //the arguments are: the file descriptor to read from (STDIN_FILENO), a pointer to the buffer to store the read data (&c), and the number of bytes to read (1)
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == 'q') break;
  }
  //
  // read one character at a time from standard input (STDIN_FILENO) into the variable c, and continue until the character 'q' is read
  // our read funtion takes three arguments: the file descriptor to read from (STDIN_FILENO), a pointer to the buffer to store the read data (&c), and the number of bytes to read (1)
  // if the character is a control character (iscntrl(c) returns true), print its ASCII value as an integer
  return 0;
}