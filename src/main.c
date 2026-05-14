#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void die(const char *s);

void enable_raw_mode(void);

void disable_raw_mode(void);

int main(void) {
    enable_raw_mode();

    while (true) {
        char c = '\0';
        if (-1 == read(STDIN_FILENO, &c, 1) && errno != EAGAIN)
            die("read");

        if (iscntrl(c)) {
            (void)printf("%d\r\n", c);
        } else {
            (void)printf("%d ('%c')\r\n", c, c);
        }

        if ('q' == c)
            break;
    }

    return EXIT_SUCCESS;
}

void die(const char *s) {
    perror(s);
    exit(EXIT_FAILURE);
}

void enable_raw_mode(void) {
    if (-1 == tcgetattr(STDIN_FILENO, &orig_termios))
        die("tcgetattr");
    (void)atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw))
        die("tcsetattr");
}

void disable_raw_mode(void) {
    if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios))
        die("tcsetattr");
}
