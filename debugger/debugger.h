#ifndef DEBUGGER_H
#define DEBUGGER_h

typedef long long int WORD;

#include "cmdline.h"
#include "hashmap.h"

#include <stdbool.h>
#include <stdlib.h>

typedef struct hashmap hashmap;

typedef struct Debugger {
    int c_pid;
    WORD loadAddress;
    hashmap *breakpoints;
} Debugger;

// newDebugger returns a pointer to a Debugger initialized with the process id
// of the specified child process.
Debugger *newDebugger(int c_pid);

// freeDebugger frees the Debugger struct.
void freeDebugger(Debugger *dbg);

// runDebugger waits for the debugee to start and initializes the REPL loop.
int runDebugger(Debugger *dbg);

// getLoadAddress queries /proc/<c_pid>/maps for the memory address offset of
// the debugee. popen is called.
WORD getLoadAddress(int c_pid);

// handleCommand calls the approriate handler for specified command.
int handleCommand(Debugger *dbg, COMMAND cmnd, Buffer *buffer);

// debugContinue continues the paused debugee.
int debugContinue(Debugger *dbg);

// =======================================
// BREAKPOINT
// =======================================

typedef struct Breakpoint {
    WORD memAddrOffset;
    WORD savedData;
    bool enabled;
    const char *setKey;
} Breakpoint;

// makeBreakpoint returns a breakpoint struct initialized with the
// specified memory address offset.
Breakpoint makeBreakpoint(WORD memAddrOffset);

// handleBreakpoint handles a breakpoint command.
// Returns 0 on success and < 0 on error.
// Command syntax:
// break [enable | disable] [addr | line] <offset>
int handleBreakpoint(Debugger *dbg, Buffer *buffer);

// enableBreakpoint enables the breakpoint at the specified memory address
// If no such breakpoint exists, it is created.
// Returns 0 if breakpoint is successfully enabled; returns 1 if breakpoint was
// already enabled.
int enableBreakpoint(Debugger *dbg, WORD memAddr);

// disablesBreakpoint disables the breakpoint at the specified memory address.
// Returns 0 if breakpoint is successfully disabled; returns 1 if breakpoint was
// already disabled or didnt exist.
int disableBreakpoint(Debugger *dbg, WORD memAddr);

// WORDCmp returns the equivalence of two WORDS.
int BreakpointCmp(const void *a, const void *b, void *udata);

// WORDHash computes the hash of given WORD;
uint64_t BreakpointHash(const void *item, uint64_t seed0, uint64_t seed1);

// =======================================

#endif
