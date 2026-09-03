#ifndef CMDLINE_H
#define CMDLINE_H

#include "linenoise.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// The length limit set on commands passed to the debugger
extern const int CMD_MAX_SIZE;

typedef enum COMMAND {
    CONTINUE,
    REG,
    BREAKPOINT,
    EXIT,

    NUM_CMDS,
    INVALID,
} COMMAND;

typedef enum BREAKPOINT_OPTIONS {
    ENABLE_BREAKPOINT,
    DISABLE_BREAKPOINT,

    BREAKPOINT_ARG_MEMADDR,
    BREAKPOINT_ARG_LINENUM,

    INVALID_BREAKPOINT_OPT,
} BREAKPOINT_OPTIONS;

typedef enum REGISTER_OPTIONS {
    READ_REGISTER,
    WRITE_REGISTER,

    REGISTER_ARG_ABBR,
    REGISTER_ARG_DWARF,
    REGISTER_ARG_ALL,

    INVALID_REGISTER_OPT,
} REGISTER_OPTIONS;

// Minimal char buffer class
typedef struct Buffer {
    // Underlying buffer.
    // Caller has the responsibility of allocation and deallocation.
    char *data;

    // size is the length of the buffer in characters.
    // a value of 0 indicates allocation from linenoise and an unknown length.
    int size;

    // rseek is the read seek of the bufer.
    int rseek;

    // rseek is the write seek of the bufer.
    int wseek;
} Buffer;

// newBuffer initializes a Buffer and returns a pointer to it.
// if size > 0: malloc's a buffer of specified size to data.
Buffer *newBuffer(int size);

// peekBuffer reads the next character from buffer without advancing rseek.
// If there is nothing to read, null character is returned.
char peekBuffer(Buffer *buffer);

// peekBuffer reads the last character from buffer without advancing rseek.
// If there is nothing to read, null character is returned.
char peekBufferBack(Buffer *buffer);

// readFromBuffer reads the next character from buffer and advances rseek by 1.
// If there is nothing to read, null character is returned.
char readFromBuffer(Buffer *buffer);

// writeToBuffer writes to buffer.
char writeToBuffer(Buffer *buffer, char c);

// popBuffer reduces the wseek by 1, effectively popping the last element.
// Returns the popped character.
// Returns null character if buffer was empty.
char popBuffer(Buffer *buffer);

// Resets both rseek and wseek to 0
void resetSeek(Buffer *buffer);

// pollInput takes input from user.
// Returns 0 on EOF or OOM.
char pollInput(Buffer *line);

// parseInput parses the data in line by the character and stores in buffer.
// Returns 1 if there a command ready to parsed. At this point buffer has a
// command ready to read. Returns 0 if more input is needed. Returns -1 in case
// of buffer overflow.
int parseInput(Buffer *buffer, Buffer *line);

// matchCommand matches the command string stored in buffer with available
// commands. If no command matches, returns the INVALID command.
COMMAND matchCommand(char *buffer);

bool isWhitespace(char c);

// nextToken advances buffer upto the next null character and returns the
// address to that character. The buffer upto this token is cleared.
// A token is only useable till the next call to nextToken.
char *nextToken(Buffer *buffer);

// matchBreakpointOption matches the command string stored in buffer with
// available breakpoint command options. If no option matches, returns the
// INVALID_BREAKPOINT_OPT command.
BREAKPOINT_OPTIONS matchBreakpointOption(char *buffer);

// matchRegisterOption matches the command string stored in buffer with
// available register command options. If no option matches, returns the
// INVALID_REGISTER_OPT command.
REGISTER_OPTIONS matchRegisterOption(char *buffer);

#endif