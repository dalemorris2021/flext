#include <asm-generic/ioctls.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define FLEXT_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

typedef struct termios termios;

typedef struct winsize winsize;

typedef struct {
    char *buf;
    int length;
} AppendBuffer;

typedef struct {
    int32_t cursor_x;
    int32_t cursor_y;
    int32_t screen_rows;
    int32_t screen_columns;
    termios orig_termios;
} EditorConfig;

enum editor_key {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
};

termios orig_termios;

EditorConfig config;

void die(const char *s);

void enable_raw_mode(void);

void disable_raw_mode(void);

int32_t editor_read_key(void);

void editor_process_keypress(int32_t c);

void editor_refresh_screen(void);

void editor_draw_rows(AppendBuffer *ab);

void editor_move_cursor(int32_t key);

int32_t get_window_size(int32_t *rows, int32_t *columns);

int32_t get_cursor_position(int *rows, int *columns);

void init_editor(void);

void AppendBuffer_append(AppendBuffer *ab, const char *s, int len);

void AppendBuffer_free(AppendBuffer *ab);

int main(void) {
    enable_raw_mode();
    init_editor();

    while (true) {
        editor_refresh_screen();
        int32_t c = editor_read_key();
        editor_process_keypress(c);
    }

    return EXIT_SUCCESS;
}

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(EXIT_FAILURE);
}

void enable_raw_mode(void) {
    if (-1 == tcgetattr(STDIN_FILENO, &config.orig_termios))
        die("tcgetattr");
    (void)atexit(disable_raw_mode);

    termios raw = config.orig_termios;
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
    if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.orig_termios))
        die("tcsetattr");
}

int32_t editor_read_key(void) {
    int32_t nread;
    char c;

    while (1 != (nread = read(STDIN_FILENO, &c, 1))) {
        if (-1 == nread && EAGAIN != errno)
            die("read");
    }

    if ('\x1b' == c) {
        uint32_t seq_size = 3;
        char seq[seq_size];

        if (1 != read(STDIN_FILENO, &seq[0], 1))
            return '\x1b';
        if (1 != read(STDIN_FILENO, &seq[1], 1))
            return '\x1b';

        if ('[' == seq[0]) {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                    case '1':
                        return HOME_KEY;
                    case '3':
                        return DEL_KEY;
                    case '4':
                        return END_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

void editor_process_keypress(int32_t c) {
    switch (c) {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(EXIT_SUCCESS);
        break;
    case HOME_KEY:
        config.cursor_x = 0;
        break;
    case END_KEY:
        config.cursor_x = config.screen_columns - 1;
        break;
    case PAGE_UP: {
        int32_t times = config.screen_rows;
        while (times--)
            editor_move_cursor(ARROW_UP);
    } break;
    case PAGE_DOWN: {
        int32_t times = config.screen_rows;
        while (times--)
            editor_move_cursor(ARROW_DOWN);
    } break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editor_move_cursor(c);
        break;
    }
}

void editor_refresh_screen(void) {
    AppendBuffer ab = (AppendBuffer)ABUF_INIT;

    AppendBuffer_append(&ab, "\x1b[?25l", 6);
    AppendBuffer_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);

    uint32_t buf_size = 32;
    char buf[buf_size];
    (void)snprintf(buf, buf_size, "\x1b[%d;%dH", config.cursor_y + 1,
                   config.cursor_x + 1);
    AppendBuffer_append(&ab, buf, strlen(buf));

    AppendBuffer_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.length);
    AppendBuffer_free(&ab);
}

void editor_draw_rows(AppendBuffer *ab) {
    for (size_t i = 0; (int32_t)i < config.screen_rows; i++) {
        if ((int32_t)i == config.screen_rows / 3) {
            uint32_t welcome_size = 80;
            char welcome[welcome_size];

            int32_t welcome_length =
                snprintf(welcome, welcome_size, "Flext editor -- version %s",
                         FLEXT_VERSION);
            if (welcome_length > config.screen_columns)
                welcome_length = config.screen_columns;

            int32_t padding = (config.screen_columns - welcome_length) / 2;
            if (padding) {
                AppendBuffer_append(ab, "~", 1);
                padding -= 1;
            }
            while (padding--)
                AppendBuffer_append(ab, " ", 1);

            AppendBuffer_append(ab, welcome, welcome_length);
        } else {
            AppendBuffer_append(ab, "~", 1);
        }

        AppendBuffer_append(ab, "\x1b[K", 3);

        if ((int32_t)i < config.screen_rows - 1) {
            AppendBuffer_append(ab, "\r\n", 2);
        }
    }
}

void editor_move_cursor(int32_t key) {
    switch (key) {
    case ARROW_LEFT:
        if (0 != config.cursor_x) {
            config.cursor_x -= 1;
        }
        break;
    case ARROW_RIGHT:
        if (config.screen_columns - 1 != config.cursor_x) {
            config.cursor_x += 1;
        }
        break;
    case ARROW_UP:
        if (0 != config.cursor_y) {
            config.cursor_y -= 1;
        }
        break;
    case ARROW_DOWN:
        if (config.screen_rows - 1 != config.cursor_y) {
            config.cursor_y += 1;
        }
        break;
    }
}

int32_t get_window_size(int32_t *rows, int32_t *columns) {
    winsize ws;

    if (-1 == ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) || 0 == ws.ws_col) {
        if (12 != write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12))
            return -1;
        return get_cursor_position(rows, columns);
    } else {
        *columns = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

int32_t get_cursor_position(int *rows, int *columns) {
    uint32_t buf_size = 32;
    char buf[buf_size];
    uint32_t i = 0;

    if (4 != write(STDOUT_FILENO, "\x1b[6n", 4))
        return -1;

    while (i < buf_size - 1) {
        if (1 != read(STDIN_FILENO, &buf[i], 1))
            break;
        if ('R' == buf[i])
            break;

        i++;
    }
    buf[i] = '\0';

    if ('\x1b' != buf[0] || '[' != buf[1])
        return -1;
    if (2 != sscanf(&buf[2], "%d;%d", rows, columns))
        return -1;

    return 0;
}

void init_editor(void) {
    config.cursor_x = 0;
    config.cursor_y = 0;

    if (-1 == get_window_size(&config.screen_rows, &config.screen_columns))
        die("get_window_size");
}

void AppendBuffer_append(AppendBuffer *ab, const char *s, int length) {
    char *new = realloc(ab->buf, length + ab->length);

    if (!new)
        return;
    memcpy(&new[ab->length], s, length);
    ab->buf = new;
    ab->length += length;
}

void AppendBuffer_free(AppendBuffer *ab) { free(ab->buf); }
