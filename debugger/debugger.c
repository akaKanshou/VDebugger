#include "debugger.h"
#include "cmdline.h"

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>

Debugger *newDebugger(int c_pid) {
    Debugger *dbg = (Debugger *)malloc(sizeof(Debugger));
    dbg->c_pid = c_pid;
    return dbg;
}

int runDebugger(Debugger *dbg) {
    if (dbg == NULL) {
        return 2;
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

            handleCommand(dbg, cmnd, buffer);
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
        debugContinue(dbg);
        return 0;
    case PAUSE:
        return 0;
    default:
        fprintf(stdout, "Unknown command");
        return 0;
    }

    fprintf(stderr, "Unreachable state in handleCommand");
    return 1;
}