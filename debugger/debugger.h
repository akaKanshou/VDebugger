#ifndef DEBUGGER_H
#define DEBUGGER_h

#include "cmdline.h"
#include "hashmap.h"
#include "registers.h"

#include <stdbool.h>
#include <stdlib.h>

typedef int64_t WORD;

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

// Wrapper for waitpid.
// TODO: Error handling.
int wait_for_signal(Debugger *dbg, int *status, int options);

// handleCommand calls the approriate handler for specified command.
// Returns 0 on success, 1 if debugger should terminate and, < 0 on failure.
int handleCommand(Debugger *dbg, COMMAND cmnd, Buffer *buffer);

// debugContinue continues the paused debugee.
// Returns 0 on success, 1 on debugee terminating and, < 0 on failure.
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

// =======================================
// REGISTERS
// =======================================

// handleBreakpoint handles a register command.
// Returns 0 on success and < 0 on error.
// Command syntax:
// 1) reg read a
// 2) reg read [s <register_abbreviation> | d <register_dwarf_number>]
// 3) reg write [s <register_abbr> | d <register_dwarf_num>] <value_decimal>
int handleRegister(Debugger *dbg, Buffer *buffer);

// getRegsStruct populates the given regs_struct with the values of all
// registers.
// Returns 0 on success and < 0 on error.
int getRegsStruct(Debugger *dbg, regs_struct *regs);

// setRegsStruct sets the value of registers as to regs_struct.
// Returns 0 on success, and < 0 on failure
int setRegsStruct(Debugger *dbg, regs_struct *regs);

// getRegsValue gets the value of the specific register reg and stores it in
// value ptr.
// Returns 0 on success and < 0 on error.
int getRegValue(Debugger *dbg, REGISTER reg, WORD *value);

// setRegsValue sets the value of the specific register reg to the given value.
// Returns 0 on success and < 0 on error.
int setRegValue(Debugger *dbg, REGISTER reg, WORD value);

// =======================================

// =======================================
// Step
// =======================================

// singleStep steps over one instruction.
// Returns 0 on success, < 0 on error.
int single_step(Debugger *dbg);

// step_over_breakpoint steps over the current breakpoint IF EXISTS.
// Disables breakpoint -> Rewinds PC to breakpoint -> Steps over once -> Enables
// breakpoint.
// Returns 0 on success or if breakpoint doesn't exist, < 0 on error.
int step_over_breakpoint(Debugger *dbg);

// handle_step handles a step over command.
// Returns 0 on success and < 0 on error.
// Command syntax:
// step <times>
int handle_step(Debugger *dbg, Buffer *buffer);

// =======================================

// =======================================
// Errors
// =======================================

// get_waitpid_err returns a human readable error string for waitpid errors
const char *get_waitpid_err(int err);

// get_ptrace_err returns a human readable error string for waitpid errors
const char *get_ptrace_err(int err);

// =======================================

#endif
