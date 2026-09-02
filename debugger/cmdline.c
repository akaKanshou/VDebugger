#include "cmdline.h"
#include <string.h>

const int CMD_MAX_SIZE = 1024;

Buffer *newBuffer(int size) {
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

char peekBuffer(Buffer *buffer) {
    if (buffer->rseek >= buffer->size) return '\0';
    return buffer->data[buffer->rseek];
}

char peekBufferBack(Buffer *buffer) {
    if ((buffer->size == 0) || (buffer->wseek == 0)) return '\0';
    return buffer->data[buffer->wseek - 1];
}

char readFromBuffer(Buffer *buffer) {
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

char writeToBuffer(Buffer *buffer, char c) {
    if (buffer->wseek >= buffer->size) return 1;
    buffer->data[buffer->wseek++] = c;
    return 0;
}

char popBuffer(Buffer *buffer) {
    if (buffer->wseek == 0) return '\0';
    return buffer->data[buffer->wseek--];
}

void resetSeek(Buffer *buffer) { buffer->rseek = buffer->wseek = 0; }

char pollInput(Buffer *line) {
    if (line->data = linenoise("vdb> ")) {
        linenoiseHistoryAdd(line->data);
        return 1;
    } else {
        return 0;
    }
}

int parseInput(Buffer *buffer, Buffer *line) {
    if (line->data == NULL) {
        strcpy(buffer->data, "exit");
        return 1;
    }

    char next;
    while (next = readFromBuffer(line)) {
        if (next == ';') {
            writeToBuffer(buffer, '\0');
            return 1;
        }

        if (!isWhitespace(next)) {
            if (writeToBuffer(buffer, next)) {
                return -1;
            }
        } else if (peekBufferBack(buffer) != '\0') {
            if (writeToBuffer(buffer, '\0')) {
                return -1;
            }
        }
    }
    if (peekBufferBack(buffer) != '\0') {
        writeToBuffer(buffer, '\0');
    }

    return 0;
}

COMMAND matchCommand(char *buffer) {
    if (!buffer) {
        return INVALID;
    }

    if (!strcmp(buffer, "continue")) {
        return CONTINUE;
    }

    if (!strcmp(buffer, "pause")) {
        return PAUSE;
    }

    if (!strcmp(buffer, "exit")) {
        return EXIT;
    }

    if (!strcmp(buffer, "break")) {
        return BREAKPOINT;
    }

    return INVALID;
}

bool isWhitespace(char c) {
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

char *nextToken(Buffer *buffer) {
    if (!buffer || !buffer->data[buffer->rseek]) return NULL;
    char *next = buffer->data + buffer->rseek;
    while (readFromBuffer(buffer) != '\0') {
        // skip characters
    }

    if (next[0] == '\0') return NULL;
    return next;
}

BREAKPOINT_OPTIONS matchBreakpointOption(char *buffer) {
    if (!buffer) {
        return INVALID_BREAKPOINT;
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

    return INVALID_BREAKPOINT;
}