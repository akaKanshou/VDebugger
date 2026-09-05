#include "cmdline.h"
#include <string.h>

const int CMD_MAX_SIZE = 1024;

Buffer *new_buffer(int size) {
    Buffer *buffer = (Buffer *)malloc(sizeof(Buffer));
    if (!buffer) return NULL;

    buffer->rseek = buffer->wseek = buffer->size = 0;
    buffer->data = NULL;
    if (size > 0) {
        buffer->data = (char *)malloc(sizeof(char) * (size + 1));
        buffer->size = size;
        buffer->data[size] = '\0';
    }

    return buffer;
}

char peek_buffer(Buffer *buffer) {
    if (buffer->rseek >= buffer->size) return '\0';
    return buffer->data[buffer->rseek];
}

char peek_buffer_back(Buffer *buffer) {
    if ((buffer->size == 0) || (buffer->wseek == 0)) return '\0';
    return buffer->data[buffer->wseek - 1];
}

char read_from_buffer(Buffer *buffer) {
    if (buffer->size == 0) {
        if (buffer->data[buffer->rseek]) {
            return buffer->data[buffer->rseek++];
        }
        return '\0';
    }

    if (buffer->rseek < buffer->size) {
        return buffer->data[buffer->rseek++];
    }

    return '\0';
}

char write_to_buffer(Buffer *buffer, char c) {
    if (buffer->wseek >= buffer->size) return 1;
    buffer->data[buffer->wseek++] = c;
    return 0;
}

char pop_buffer(Buffer *buffer) {
    if (buffer->wseek == 0) return '\0';
    return buffer->data[buffer->wseek--];
}

void reset_seek(Buffer *buffer) { buffer->rseek = buffer->wseek = 0; }

char poll_input(Buffer *line) {
    if (line->data = linenoise("vdb> ")) {
        linenoiseHistoryAdd(line->data);
        return 1;
    } else {
        return 0;
    }
}

int parse_input(Buffer *buffer, Buffer *line) {
    if (line->data == NULL) {
        strcpy(buffer->data, "exit");
        return 1;
    }

    char next;
    while (next = read_from_buffer(line)) {
        if (next == ';') {
            write_to_buffer(buffer, '\0');
            return 1;
        }

        if (!is_whitespace(next)) {
            if (write_to_buffer(buffer, next)) {
                return -1;
            }
        } else if (peek_buffer_back(buffer) != '\0') {
            if (write_to_buffer(buffer, '\0')) {
                return -1;
            }
        }
    }
    if (peek_buffer_back(buffer) != '\0') {
        write_to_buffer(buffer, '\0');
    }

    return 0;
}

COMMAND match_command(char *buffer) {
    if (!buffer) {
        return INVALID;
    }

    if (!strcmp(buffer, "continue")) {
        return CONTINUE;
    }

    if (!strcmp(buffer, "reg")) {
        return REG;
    }

    if (!strcmp(buffer, "exit")) {
        return EXIT;
    }

    if (!strcmp(buffer, "break")) {
        return BREAKPOINT;
    }

    if (!strcmp(buffer, "step")) {
        return STEP;
    }

    return INVALID;
}

bool is_whitespace(char c) {
    switch (c) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
        return true;
    default:
        return false;
    }
}

char *next_token(Buffer *buffer) {
    if (!buffer || !buffer->data[buffer->rseek]) return NULL;
    if (buffer->size && buffer->rseek >= buffer->wseek) return NULL;
    char *next = buffer->data + buffer->rseek;
    while (read_from_buffer(buffer) != '\0') {
        // skip characters
    }

    if (next[0] == '\0') return NULL;
    return next;
}

BREAKPOINT_OPTIONS match_breakpoint_option(char *buffer) {
    if (!buffer) {
        return INVALID_BREAKPOINT_OPT;
    }

    if (!strcmp(buffer, "enable")) {
        return ENABLE_BREAKPOINT;
    }

    if (!strcmp(buffer, "disable")) {
        return DISABLE_BREAKPOINT;
    }

    if (!strcmp(buffer, "addr")) {
        return BREAKPOINT_ARG_MEMADDR;
    }

    if (!strcmp(buffer, "line")) {
        return BREAKPOINT_ARG_LINENUM;
    }

    return INVALID_BREAKPOINT_OPT;
}

REGISTER_OPTIONS match_register_option(char *buffer) {
    if (!buffer) return INVALID_REGISTER_OPT;

    if (!strcmp(buffer, "read")) {
        return READ_REGISTER;
    }

    if (!strcmp(buffer, "write")) {
        return WRITE_REGISTER;
    }

    if (!strcmp(buffer, "s")) {
        return REGISTER_ARG_ABBR;
    }

    if (!strcmp(buffer, "d")) {
        return REGISTER_ARG_DWARF;
    }

    if (!strcmp(buffer, "a")) {
        return REGISTER_ARG_ALL;
    }

    return INVALID_REGISTER_OPT;
}