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
    STEP,

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

// new_buffer initializes a Buffer and returns a pointer to it.
// if size > 0: malloc's a buffer of specified size to data.
Buffer *new_buffer(int size);

// peek_buffer reads the next character from buffer without advancing rseek.
// If there is nothing to read, null character is returned.
char peek_buffer(Buffer *buffer);

// peek_buffer_back reads the last character from buffer without advancing
// rseek. If there is nothing to read, null character is returned.
char peek_buffer_back(Buffer *buffer);

// read_from_buffer reads the next character from buffer and advances rseek
// by 1. If there is nothing to read, null character is returned.
char read_from_buffer(Buffer *buffer);

// write_to_buffer writes to buffer.
char write_to_buffer(Buffer *buffer, char c);

// pop_buffer reduces the wseek by 1, effectively popping the last element.
// Returns the popped character.
// Returns null character if buffer was empty.
char pop_buffer(Buffer *buffer);

// Resets both rseek and wseek to 0
void reset_seek(Buffer *buffer);

// poll_input takes input from user.
// Returns 0 on EOF or OOM.
char poll_input(Buffer *line);

// parse_input parses the data in line by the character and stores in buffer.
// Returns 1 if there a command ready to parsed. At this point buffer has a
// command ready to read. Returns 0 if more input is needed. Returns -1 in case
// of buffer overflow.
int parse_input(Buffer *buffer, Buffer *line);

// match_command matches the command string stored in buffer with available
// commands. If no command matches, returns the INVALID command.
COMMAND match_command(char *buffer);

bool is_whitespace(char c);

// next_token advances buffer upto the next null character and returns the
// address to that character. The buffer upto this token is cleared.
// A token is only useable till the next call to next_token.
char *next_token(Buffer *buffer);

// match_breakpoint_option matches the command string stored in buffer with
// available breakpoint command options. If no option matches, returns the
// INVALID_BREAKPOINT_OPT command.
BREAKPOINT_OPTIONS match_breakpoint_option(char *buffer);

// match_register_option matches the command string stored in buffer with
// available register command options. If no option matches, returns the
// INVALID_REGISTER_OPT command.
REGISTER_OPTIONS match_register_option(char *buffer);

#endif