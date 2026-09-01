#ifndef DEBUGGER_H
#define DEBUGGER_h

#include "cmdline.h"
#include <stdlib.h>

typedef struct Debugger {
    int c_pid;
} Debugger;

// newDebugger returns a pointer to a Debugger initialized with the process id
// of the specified child process.
Debugger *newDebugger(int c_pid);

// runDebugger waits for the debugee to start and initializes the REPL loop.
int runDebugger(Debugger *dbg);

// handleCommand calls the approriate handler for specified command.
int handleCommand(Debugger *dbg, COMMAND cmnd, Buffer *buffer);

// debugContinue continues the paused debugee.
int debugContinue(Debugger *dbg);

#endif
