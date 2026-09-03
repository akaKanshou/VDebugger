#include "debugger.h"
#include "cmdline.h"

#include <linux/limits.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

Debugger *newDebugger(int c_pid) {
    Debugger *dbg = (Debugger *)malloc(sizeof(Debugger));
    dbg->c_pid = c_pid;
    dbg->breakpoints = hashmap_new(sizeof(Breakpoint), 0, 0, 0, BreakpointHash,
                                   BreakpointCmp, NULL, NULL);
    dbg->loadAddress = getLoadAddress(c_pid);
    return dbg;
}

void freeDebugger(Debugger *dbg) { free(dbg); }

WORD getLoadAddress(int c_pid) {
    char sysCall[64], memOffsetHex[64];
    sprintf(sysCall, "cat /proc/%i/maps | head -c 16", c_pid);
    FILE *fpipe = popen(sysCall, "r");
    if (!fpipe) return 0ll;
    if (fgets(memOffsetHex, 64, fpipe) == NULL) return 0ll;
    return strtoll(memOffsetHex, NULL, 16);
}

int runDebugger(Debugger *dbg) {
    if (dbg == NULL) {
        return -1;
    }

    int status, options = 0;
    waitpid(dbg->c_pid, &status, options);

    int res;
    COMMAND cmnd;
    Buffer *buffer = newBuffer(CMD_MAX_SIZE), *line = newBuffer(0);
    while (cmnd != EXIT && pollInput(line)) {
        while (res = parseInput(buffer, line)) {
            if (res < 0) {
                cmnd = INVALID;
                resetSeek(buffer);
                continue;
            }

            cmnd = matchCommand(nextToken(buffer));
            if (cmnd == EXIT) {
                break;
            }

            if (!handleCommand(dbg, cmnd, buffer)) {
                fprintf(stdout, "\nOK\n"); // cmdline
            } else {
                fprintf(stdout, "\n!x!\n"); // cmdline
            }
            resetSeek(buffer);
        }

        linenoiseFree(line->data);
        resetSeek(line);
    }

    free(buffer->data);
    free(buffer);
    free(line);
    return 0;
}

int debugContinue(Debugger *dbg) {
    ptrace(PTRACE_CONT, dbg->c_pid, NULL, NULL);

    int status, options = 0;
    waitpid(dbg->c_pid, &status, options);

    return 0;
}

int handleCommand(Debugger *dbg, COMMAND cmnd, Buffer *buffer) {
    switch (cmnd) {
    case CONTINUE:
        return debugContinue(dbg);
    case BREAKPOINT:
        return handleBreakpoint(dbg, buffer);
    case REG:
        return handleRegister(dbg, buffer);
    default:
        fprintf(stdout, "Unknown command"); // cmdline
        return 0;
    }

    fprintf(stderr, "Unreachable state in handleCommand"); // cmdline
    return -1;
}

// =======================================
// BREAKPOINT
// =======================================

int handleBreakpoint(Debugger *dbg, Buffer *buffer) {
    BREAKPOINT_OPTIONS action = matchBreakpointOption(nextToken(buffer)),
                       mode = matchBreakpointOption(nextToken(buffer));
    if (action == INVALID_BREAKPOINT_OPT || mode == INVALID_BREAKPOINT_OPT)
        return -1;

    char *argToken = nextToken(buffer);
    if (!argToken) return -1;

    WORD arg;
    switch (mode) {
    case BREAKPOINT_ARG_MEMADDR:
        char *argEnd;
        arg = strtoll(argToken, &argEnd, 16);
        if (*argEnd) return -1;
        break;
    case BREAKPOINT_ARG_LINENUM:
        break;
    default:
        return -2;
    }

    switch (action) {
    case ENABLE_BREAKPOINT:
        return enableBreakpoint(dbg, arg);
    case DISABLE_BREAKPOINT:
        return disableBreakpoint(dbg, arg);
    }

    return -1;
}

Breakpoint makeBreakpoint(WORD memAddrOffset) {
    Breakpoint breakpoint = {
        .memAddrOffset = memAddrOffset, .savedData = 0, .enabled = false};
    return breakpoint;
}

int enableBreakpoint(Debugger *dbg, WORD memAddr) {
    memAddr += dbg->loadAddress;

    Breakpoint breakpoint = makeBreakpoint(memAddr);
    const Breakpoint *previous = hashmap_get(dbg->breakpoints, &breakpoint);
    if (previous && previous->enabled == true) return 1;

    WORD data = ptrace(PTRACE_PEEKDATA, dbg->c_pid, memAddr, NULL);
    breakpoint.savedData = data;

    data = (data & ~0xff) | 0xcc;
    ptrace(PTRACE_POKEDATA, dbg->c_pid, memAddr, data, NULL);

    breakpoint.enabled = true;
    hashmap_set(dbg->breakpoints, &breakpoint);
    return 0;
}

int disableBreakpoint(Debugger *dbg, WORD memAddr) {
    memAddr += dbg->loadAddress;

    Breakpoint breakpoint = makeBreakpoint(memAddr);
    const Breakpoint *previous = hashmap_get(dbg->breakpoints, &breakpoint);
    if (!previous || previous->enabled == false) return 1;

    ptrace(PTRACE_POKEDATA, dbg->c_pid, memAddr, previous->savedData, NULL);

    breakpoint.enabled = false;
    hashmap_set(dbg->breakpoints, &breakpoint);
    return 0;
}

int BreakpointCmp(const void *a, const void *b, void *udata) {
    const Breakpoint *memA = a;
    const Breakpoint *memB = b;
    return memA->memAddrOffset != memB->memAddrOffset;
}

uint64_t BreakpointHash(const void *item, uint64_t seed0, uint64_t seed1) {
    const Breakpoint *memA = item;
    return hashmap_murmur(&memA->memAddrOffset, sizeof(memA->memAddrOffset),
                          seed0, seed1);
}

// =======================================

// =======================================
// REGISTERS
// =======================================

// TODO: Refactor this function. Does too much.
int handleRegister(Debugger *dbg, Buffer *buffer) {
    REGISTER_OPTIONS action = matchRegisterOption(nextToken(buffer)),
                     mode = matchRegisterOption(nextToken(buffer));

    if (action == INVALID_REGISTER_OPT || mode == INVALID_REGISTER_OPT) {
        return -1;
    }

    char *arg = nextToken(buffer);
    int res;

    regs_struct regs;
    REGISTER reg;
    if (res = getRegsStruct(dbg, &regs)) {
        return res;
    }

    switch (action) {
    case READ_REGISTER:
        if (mode == REGISTER_ARG_ALL) {
            if (arg) {
                fprintf(stdout, "invalid arg %s\n", arg);
                return -1;
            }

            printRegisters(&regs);
            return 0;
        }

        if (!arg) return -1;

        if (mode == REGISTER_ARG_ABBR) {
            reg = abbrToReg(arg);
        } else if (mode == REGISTER_ARG_DWARF) {
            int dwarfn = strtol(arg, &arg, 16);
            if (arg + 1 < buffer->data + buffer->rseek) {
                return -1;
            }
            reg = DWARFNToReg(dwarfn);
        }
        if (reg == NO_SUCH_REGISTER) return -1;

        fprintf(stdout, "value :%llx\n", *getRegister(&regs, reg)); // cmdline

        return 0;
    case WRITE_REGISTER:
        if (!arg) return -1;

        if (mode == REGISTER_ARG_ABBR) {
            reg = abbrToReg(arg);
        } else if (mode == REGISTER_ARG_DWARF) {
            int dwarfn = strtol(arg, &arg, 10);
            if (arg + 1 < buffer->data + buffer->rseek) {
                return -1;
            }
            reg = DWARFNToReg(dwarfn);
        }
        if (reg == NO_SUCH_REGISTER) return -1;

        arg = nextToken(buffer);
        if (!arg) return -1;
        WORD value = strtoll(arg, &arg, 10);
        if (arg + 1 < buffer->data + buffer->rseek) {
            return -1;
        }

        setRegValue(dbg, reg, value);

        return 0;
    }

    return -1;
}

int getRegsStruct(Debugger *dbg, regs_struct *regs) {
    ptrace(PTRACE_GETREGS, dbg->c_pid, NULL, regs);
    return 0;
}

int setRegsStruct(Debugger *dbg, regs_struct *regs) {
    ptrace(PTRACE_SETREGS, dbg->c_pid, NULL, regs);
    return 0;
}

int getRegValue(Debugger *dbg, REGISTER reg, WORD *value) {
    int res;

    regs_struct regs;
    if (res = getRegsStruct(dbg, &regs)) return res;

    UWORD *regl = getRegister(&regs, reg);
    if (!regl) return NO_SUCH_REGISTER;

    *value = *regl;
    return 0;
}

int setRegValue(Debugger *dbg, REGISTER reg, WORD value) {
    int res;

    regs_struct regs;
    if (res = getRegsStruct(dbg, &regs)) return res;

    UWORD *regl = getRegister(&regs, reg);
    if (!regl) return NO_SUCH_REGISTER;

    *regl = value;
    if (res = setRegsStruct(dbg, &regs)) return res;

    return 0;
}

// =======================================